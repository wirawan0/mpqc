//
// transform_tbint.cc
//
// Copyright (C) 2004 Edward Valeev
//
// Author: Edward Valeev <edward.valeev@chemistry.gatech.edu>
// Maintainer: EV
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef __GNUG__
#pragma implementation
#endif

#include <stdexcept>

#include <util/misc/formio.h>
#include <util/state/state_bin.h>
#include <util/ref/ref.h>
#include <math/scmat/local.h>
#include <chemistry/qc/basis/integral.h>
#include <chemistry/qc/mbptr12/transform_tbint.h>

using namespace std;
using namespace sc;

inline int max(int a,int b) { return (a > b) ? a : b;}

/*-----------
  TwoBodyMOIntsTransform
 -----------*/
static ClassDesc TwoBodyMOIntsTransform_cd(
  typeid(TwoBodyMOIntsTransform),"TwoBodyMOIntsTransform",1,"virtual public SavableState",
  0, 0, 0);

TwoBodyMOIntsTransform::TwoBodyMOIntsTransform(const std::string& name, const Ref<MOIntsTransformFactory>& factory,
                                               const Ref<MOIndexSpace>& space1, const Ref<MOIndexSpace>& space2,
                                               const Ref<MOIndexSpace>& space3, const Ref<MOIndexSpace>& space4) :
  name_(name), factory_(factory), space1_(space1), space2_(space2), space3_(space3), space4_(space4)
{
  mem_ = MemoryGrp::get_default_memorygrp();
  msg_ = MessageGrp::get_default_messagegrp();
  thr_ = ThreadGrp::get_default_threadgrp();

  // Default values
  memory_ = factory_->memory();
  debug_ = factory_->debug();
  dynamic_ = factory_->dynamic();
  print_percent_ = factory_->print_percent();
  ints_method_ = factory_->ints_method();
  file_prefix_ = factory_->file_prefix();
  
  init_vars();
}

TwoBodyMOIntsTransform::TwoBodyMOIntsTransform(StateIn& si) : SavableState(si)
{
  si.get(name_);
  factory_ << SavableState::restore_state(si);
  ints_acc_ << SavableState::restore_state(si);
  
  space1_ << SavableState::restore_state(si);
  space2_ << SavableState::restore_state(si);
  space3_ << SavableState::restore_state(si);
  space4_ << SavableState::restore_state(si);

  mem_ = MemoryGrp::get_default_memorygrp();
  msg_ = MessageGrp::get_default_messagegrp();
  thr_ = ThreadGrp::get_default_threadgrp();

  double memory; si.get(memory); memory_ = (size_t) memory;
  si.get(debug_);
  int dynamic; si.get(dynamic); dynamic_ = (bool) dynamic;
  si.get(print_percent_);
  int ints_method; si.get(ints_method); ints_method_ = (MOIntsTransformFactory::StoreMethod) ints_method;
  si.get(file_prefix_);

  init_vars();
}

TwoBodyMOIntsTransform::~TwoBodyMOIntsTransform()
{
}

void
TwoBodyMOIntsTransform::save_data_state(StateOut& so)
{
  so.put(name_);
  SavableState::save_state(factory_.pointer(),so);
  SavableState::save_state(ints_acc_.pointer(),so);
  
  SavableState::save_state(space1_.pointer(),so);
  SavableState::save_state(space2_.pointer(),so);
  SavableState::save_state(space3_.pointer(),so);
  SavableState::save_state(space4_.pointer(),so);

  so.put((double)memory_);
  so.put(debug_);
  so.put((int)dynamic_);
  so.put(print_percent_);
  so.put((int)ints_method_);
  so.put(file_prefix_);
}

///////////////////////////////////////////////////////
// Compute the batchsize for the transformation
//
// Only arrays allocated before exiting the loop over
// i-batches are included here  - only these arrays
// affect the batch size.
///////////////////////////////////////////////////////
int
TwoBodyMOIntsTransform::compute_transform_batchsize_(size_t mem_static, int rank_i)
{
  // Check is have enough for even static objects
  size_t mem_dyn = 0;
  if (memory_ <= mem_static)
    return 0;
  else
    mem_dyn = memory_ - mem_static;

  // Determine if calculation is possible at all (i.e., if ni=1 possible)
  int ni = 1;
  distsize_t maxdyn = compute_transform_dynamic_memory_(ni);
  if (maxdyn > mem_dyn) {
    return 0;
  }

  ni = 2;
  while (ni<=rank_i) {
    maxdyn = compute_transform_dynamic_memory_(ni);
    if (maxdyn >= mem_dyn) {
      ni--;
      break;
    }
    ni++;
  }
  if (ni > rank_i) ni = rank_i;

  return ni;
}


void
TwoBodyMOIntsTransform::init_vars()
{
  int me = msg_->me();

  int restart_orbital = ints_acc_.nonnull() ? ints_acc_->next_orbital() : 0;
  int rank_i = space1_->rank() - restart_orbital;

  mem_static_ = 0;
  int ni = 0;
  if (me == 0) {
    // mem_static should include storage in MOIndexSpace
    mem_static_ = space1_->memory_in_use() +
                  space2_->memory_in_use() +
                  space3_->memory_in_use() +
                  space4_->memory_in_use(); // scf vector
    int nthreads = thr_->nthread();
    // ... plus the integrals evaluators
    mem_static_ += nthreads * factory_->integral()->storage_required_grt(space1_->basis(),space2_->basis(),
                                                            space3_->basis(),space4_->basis());
    batchsize_ = compute_transform_batchsize_(mem_static_,rank_i); 
  }

  // Send value of ni and mem_static to other nodes
  msg_->bcast(batchsize_);
  double mem_static_double = (double) mem_static_;
  msg_->bcast(mem_static_double);
  mem_static_ = (size_t) mem_static_double;
  
  npass_ = 0;
  int rest = 0;
  if (batchsize_ == rank_i) {
    npass_ = 1;
    rest = 0;
  }
  else {
    rest = rank_i%ni;
    npass_ = (rank_i - rest)/ni + 1;
    if (rest == 0) npass_--;
  }
}

void
TwoBodyMOIntsTransform::reinit_acc()
{
  if (ints_acc_.nonnull())
    ints_acc_ = 0;
  init_acc();
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
