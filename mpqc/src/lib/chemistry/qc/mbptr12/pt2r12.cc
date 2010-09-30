//
// pt2r12.cc
//
// Copyright (C) 2009 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
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

#include <chemistry/qc/mbptr12/pt2r12.h>
#include <chemistry/qc/mbptr12/print.h>
#include <chemistry/qc/mbptr12/orbitalspace_utils.h>
#include <math/scmat/local.h>
#include <chemistry/qc/mbptr12/compute_tbint_tensor.h>

using namespace std;
using namespace sc;

namespace {

  int triang_half_INDEX_ordered(int i, int j) {
    return(i*(i+1)/2+j);
  }

  int triang_half_INDEX(int i, int j) {
    return((i>j) ? triang_half_INDEX_ordered(i,j) : triang_half_INDEX_ordered(j,i));
  }

  int ordinary_INDEX(int i, int j, int coldim) {
    return(i*coldim+j);
  }

  /// tpdm_index and init_ioff: for indexing of density matrices.
  int tpdm_index(int i, int j, int k, int l,int coldim){
    //int ind_half1=triang_half_INDEX(i,j);
    //int ind_half2=triang_half_INDEX(k,l);
    int ind_half1=ordinary_INDEX(i,j,coldim);
    int ind_half2=ordinary_INDEX(k,l,coldim);
    return(triang_half_INDEX(ind_half1,ind_half2));
  }

  void vector_to_symmmatrix(RefSymmSCMatrix &matrix, const RefSCVector &vector) {
    int dim = matrix.dim().n();
    for(int i=0; i<dim; i++){
      for(int j=0; j<=i; j++) {
        matrix.set_element(i,j,vector.get_element(triang_half_INDEX(i,j)));
      }
    }
  }

  void symmmatrix_to_vector(RefSCVector &vector, const RefSymmSCMatrix &matrix) {
    int dim = matrix.dim().n();
    for(int i=0; i<dim; i++){
      for(int j=0; j<=i; j++) {
        vector.set_element(triang_half_INDEX(i,j),matrix.get_element(i,j));
      }
    }
  }

  void vector_to_matrix(RefSCMatrix &matrix, const RefSCVector &vector) {
    int dim1 = matrix.rowdim().n();
    int dim2 = matrix.coldim().n();
    for(int i=0; i<dim1; i++) {
      for(int j=0; j<dim2; j++) {
        matrix.set_element(i,j,vector.get_element(ordinary_INDEX(i,j,dim2)));
      }
    }
  }

  int lowerupper_index(int p, int q);

  void vector_to_matrix(RefSCMatrix &matrix,const RefSCVector &vector,const SpinCase2 &pairspin) {
    int dim1 = matrix.rowdim().n();
    int dim2 = matrix.coldim().n();
    if(pairspin==AlphaBeta) {
      for(int i=0; i<dim1; i++) {
        for(int j=0; j<dim2; j++) {
          matrix.set_element(i,j,vector.get_element(ordinary_INDEX(i,j,dim2)));
        }
      }
    }
    else {  // pairspin==AlphaAlpha || pairspin==BetaBeta
      matrix->assign(0.0);
      for(int i=0; i<dim1; i++) {
        for(int j=0; j<i; j++) {
          const double value = vector.get_element(lowerupper_index(i,j));
          matrix.set_element(i,j,value);
          matrix.set_element(j,i,-value);
        }
      }
    }
  }

  void matrix_to_vector(RefSCVector &vector, const RefSCMatrix &matrix) {
    int dim1 = matrix.rowdim().n();
    int dim2 = matrix.coldim().n();
    for(int i=0; i<dim1; i++) {
      for(int j=0; j<dim2; j++) {
        vector.set_element(ordinary_INDEX(i,j,dim2),matrix.get_element(i,j));
      }
    }
  }

  void matrix_to_vector(RefSCVector &vector, const RefSCMatrix &matrix,const SpinCase2 &pairspin) {
    int dim1 = matrix.rowdim().n();
    int dim2 = matrix.coldim().n();
    if(pairspin==AlphaBeta) {
      for(int i=0; i<dim1; i++) {
        for(int j=0; j<dim2; j++) {
          vector.set_element(ordinary_INDEX(i,j,dim2),matrix.get_element(i,j));
        }
      }
    }
    else {  // pairspin==AlphaAlpha || pairspin==BetaBeta
      for(int i=0; i<dim1; i++) {
        for(int j=0; j<i; j++) {
          vector.set_element(lowerupper_index(i,j),matrix.get_element(i,j));
        }
      }
    }
  }

  int lowertriang_index(int p,int q) {
    if(q>=p){
      throw ProgrammingError("lowertriang_index(p,q) -- q must be smaller than p.",__FILE__,__LINE__);
    }
    int index=p*(p+1)/2+q-p;
    return(index);
  }

  int lowerupper_index(int p, int q) {
    if(p>q) {
      return(lowertriang_index(p,q));
    }
    else if(q>p) {
      return(lowertriang_index(q,p));
    }
    else {
      throw ProgrammingError("lowerupper_index(p,q) -- p and q are not allowed to be equal.",__FILE__,__LINE__);
    }
  }

  double indexsizeorder_sign(int p,int q) {
    if(p>q) {
      return(1.0);
    }
    else if(q>p) {
      return(-1.0);
    }
    else {
      return(0.0);
    }
  }

  int antisym_pairindex(int i, int j) {
    int max_ij = std::max(i, j);
    int min_ij = std::min(i, j);
    return (max_ij -1)* max_ij/2 + min_ij;
  }

  template <typename MMatrix> // print out the elements from the two matrices which differ by more than thres
  void compare_element_diff(MMatrix & M1, MMatrix & M2, bool symmetry, unsigned int rowdim, unsigned int coldim, double thres)
  {
    for (int row = 0; row < rowdim; ++row)
    {
      if(!symmetry)
      {
        for (int col = 0; col < coldim; ++col)
        {
          if(fabs(M1(row, col) - M2(row, col)) > thres)
          {
            ExEnv::out0() << indent << "row, col, M1, M2, diff: " << row << ", " << col << ", " << M1(row, col) << ", " << M2(row, col) << std::endl
                          << indent << "                        " << M1(row, col) - M2(row, col) << std::endl;
          }
        }
      }
      else
      {
        for (int col = 0; col < row; ++col)
        {
          if(fabs(M1(row, col) - M2(row, col)) > thres)
          {
            ExEnv::out0() << indent << "row, col, M1, M2, diff: " << row << ", " << col << ", " << M1(row, col) << ", " << M2(row, col) << std::endl
                          << indent << "                        " << M1(row, col) - M2(row, col) << std::endl;
          }
        }
      }
    }
  }

}

////////////////////

static ClassDesc PT2R12_cd(typeid(PT2R12),"PT2R12",
                           1,"public Wavefunction",0,create<PT2R12>,create<PT2R12>);

PT2R12::PT2R12(const Ref<KeyVal> &keyval) : Wavefunction(keyval)
{
  ///* comment out the following to test cabs singles correction
  std::string nfzc_str = keyval->stringvalue("nfzc",KeyValValuestring("0"));
  if (nfzc_str == "auto")
    nfzc_ = molecule()->n_core_electrons()/2;
  else if (nfzc_str == "no" || nfzc_str == "false")
    nfzc_ = 0;
  else
    nfzc_ = atoi(nfzc_str.c_str());

  pt2_correction_ = keyval->booleanvalue("pt2_correction", KeyValValueboolean(true));
  omit_uocc_ = keyval->booleanvalue("omit_uocc", KeyValValueboolean(false));
  cabs_singles_ = keyval->booleanvalue("cabs_singles", KeyValValueboolean(false));
  cabs_singles_coupling_ = keyval->booleanvalue("cabs_singles_coupling", KeyValValueboolean(true));
  rotate_core_ = keyval->booleanvalue("rotate_core", KeyValValueboolean(true));
  cabs_keep2A2pterm_ = keyval->booleanvalue("cabs_keep2A2pterm", KeyValValueboolean(false));


  reference_ = require_dynamic_cast<Wavefunction*>(
        keyval->describedclassvalue("reference").pointer(),
        "PT2R12::PT2R12\n"
        );
  rdm2_ = require_dynamic_cast<RDM<Two>*>(
        keyval->describedclassvalue("rdm2").pointer(),
        "PT2R12::PT2R12\n"
        );
  assert(reference_ == rdm2_->wfn());
  rdm1_ = rdm2_->rdm_m_1();

  // this may update the accuracy of reference_ object
  this->set_desired_value_accuracy(desired_value_accuracy());

  Ref<WavefunctionWorld> world = new WavefunctionWorld(keyval, this);
  //world->memory(memory);
  const bool spin_restricted = true;
  // if omit_uocc is true, need to make an empty virtual space
  Ref<OrbitalSpace> virspace = 0;
  if (omit_uocc_) {
    virspace = new EmptyOrbitalSpace("", "", basis(), integral(), OrbitalSpace::symmetry);
  }
  Ref<RefWavefunction> ref = RefWavefunctionFactory::make(world,
                                                                reference_,
                                                                spin_restricted,
                                                                nfzc_,
                                                                0,
                                                                virspace);
  r12world_ = new R12WavefunctionWorld(keyval, ref);
  r12eval_ = new R12IntEval(r12world_);

  debug_ = keyval->intvalue("debug", KeyValValueint(0));
  r12eval_->debug(debug_);
}

PT2R12::PT2R12(StateIn &s) : Wavefunction(s) {
  reference_ << SavableState::restore_state(s);
  rdm2_ << SavableState::restore_state(s);
  rdm1_ << SavableState::restore_state(s);
  r12world_ << SavableState::restore_state(s);
  r12eval_ << SavableState::restore_state(s);
  s.get(nfzc_);
  s.get(omit_uocc_);
  s.get(cabs_singles_);
  s.get(cabs_singles_coupling_);
  s.get(debug_);
}

PT2R12::~PT2R12() {
  // need to manually break up a cycle of smart pointers
  R12WavefunctionWorld* r12world_ptr = r12world_.pointer();
  r12world_.clear();
  r12world_ptr->dereference();
  r12world_ptr->~R12WavefunctionWorld();
}

void PT2R12::save_data_state(StateOut &s) {
  Wavefunction::save_data_state(s);
  SavableState::save_state(reference_, s);
  SavableState::save_state(rdm2_, s);
  SavableState::save_state(rdm1_, s);
  SavableState::save_state(r12world_, s);
  SavableState::save_state(r12eval_, s);
  s.put(nfzc_);
  s.put(omit_uocc_);
  s.put(cabs_singles_coupling_);
  s.put(debug_);
}

void
PT2R12::obsolete() {
  reference_->obsolete();
  r12eval_->obsolete();
  rdm1_->obsolete();
  rdm2_->obsolete();
  r12world_->world()->obsolete();
  r12world_->obsolete();
  Wavefunction::obsolete();
}

void
PT2R12::set_desired_value_accuracy(double acc)
{
  Function::set_desired_value_accuracy(acc);
  if (reference_->desired_value_accuracy_set_to_default()) {
    // reference should be computed to higher accuracy
    const double ref_acc = acc * ref_to_pt2r12_acc();
    reference_->set_desired_value_accuracy(ref_acc);
  }
}

RefSymmSCMatrix PT2R12::hcore_mo() {
  Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
  Ref<OrbitalSpace> space = r12eval_->orbs(Alpha);
  RefSCMatrix coeffs = space->coefs();
  RefSCDimension nmodim = coeffs.rowdim();
  RefSCDimension naodim = coeffs.coldim();
  int nmo = nmodim.n();
  int nao = naodim.n();
  RefSCMatrix coeffs_nb = localkit->matrix(naodim,nmodim);
  for(int i=0; i<nao; i++) {
    for(int j=0; j<nmo; j++) {
      coeffs_nb.set_element(i,j,coeffs.get_element(i,j));
    }
  }

  Ref<OneBodyInt> Hcore=integral()->hcore();
  const double *buffer_Hcore = Hcore->buffer();
  Ref<GaussianBasisSet> basis = this->basis();
  int nshell = basis->nshell();

  RefSymmSCMatrix Hcore_ao = localkit->symmmatrix(naodim);

  for(int P=0; P<nshell; P++) {
    int nump = basis->shell(P).nfunction();
    for(int Q=0; Q<nshell; Q++) {
      int numq = basis->shell(Q).nfunction();

      Hcore->compute_shell(P,Q);
      int index = 0;
      for(int p=0; p<nump; p++) {
        int op = basis->shell_to_function(P) + p;
        for(int q=0; q<numq; q++) {
          int oq = basis->shell_to_function(Q) + q;
          index = p * numq + q;

          Hcore_ao.set_element(op,oq,buffer_Hcore[index]);
        }
      }
    }
  }

  RefSymmSCMatrix Hcore_mo = localkit->symmmatrix(nmodim);
  Hcore_mo.assign(0.0);
  Hcore_mo.accumulate_transform(coeffs_nb,Hcore_ao,SCMatrix::TransposeTransform);
  //Hcore_mo.accumulate_transform(coeffs_nb,Hcore_ao);

  if(debug_>=DefaultPrintThresholds::mostN2) {
    Hcore_mo->print("total hcore_mo");
  }

  return(Hcore_mo);
}

RefSymmSCMatrix PT2R12::hcore_mo(SpinCase1 spin) {
  const Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
  const Ref<OrbitalSpace> space = r12eval_->orbs(spin);
  const int nmo = space->rank();
  const RefSCMatrix coeffs = space->coefs();
  const RefSCDimension nmodim = new SCDimension(nmo);
  const RefSCDimension naodim = coeffs.coldim();
  const int nao = naodim.n();
  RefSCMatrix coeffs_nb = localkit->matrix(naodim,nmodim);
  for(int i=0; i<nao; i++) {
    for(int j=0; j<nmo; j++) {
      coeffs_nb.set_element(i,j,coeffs.get_element(i,j));
    }
  }

  const Ref<OneBodyInt> Hcore=integral()->hcore();
  const double *buffer_Hcore = Hcore->buffer();
  const Ref<GaussianBasisSet> basis = this->basis();
  const int nshell = basis->nshell();

  RefSymmSCMatrix Hcore_ao = localkit->symmmatrix(naodim);

  for(int P=0; P<nshell; P++) {
    int nump = basis->shell(P).nfunction();
    for(int Q=0; Q<nshell; Q++) {
      int numq = basis->shell(Q).nfunction();

      Hcore->compute_shell(P,Q);
      int index = 0;
      for(int p=0; p<nump; p++) {
        int op = basis->shell_to_function(P) + p;
        for(int q=0; q<numq; q++) {
          int oq = basis->shell_to_function(Q) + q;
          index = p * numq + q;

          Hcore_ao.set_element(op,oq,buffer_Hcore[index]);
        }
      }
    }
  }

  RefSymmSCMatrix Hcore_mo = localkit->symmmatrix(nmodim);
  Hcore_mo.assign(0.0);
  Hcore_mo.accumulate_transform(coeffs_nb,Hcore_ao,SCMatrix::TransposeTransform);

  if(debug_>=DefaultPrintThresholds::mostN2) {
    Hcore_mo.print(prepend_spincase(spin,"hcore_mo").c_str());
  }

  return(Hcore_mo);
}

RefSymmSCMatrix PT2R12::moints() {
  Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
  Ref<OrbitalSpace> space = r12eval_->orbs(Alpha);

  RefSCMatrix coeffs = space->coefs();

  RefSCDimension nmodim = coeffs.rowdim();
  RefSCDimension naodim = coeffs.coldim();
  int nmo = nmodim.n();
  int nao = naodim.n();
  RefSCDimension nmodim_nb = new SCDimension(nmo);
  RefSCDimension naodim_nb = new SCDimension(nao);

  RefSCMatrix coeffs_nb = localkit->matrix(naodim,nmodim);
  for(int i=0; i<nao; i++) {
    for(int j=0; j<nmo; j++) {
      coeffs_nb.set_element(i,j,coeffs.get_element(i,j));
    }
  }

  RefSCDimension triangdim = new SCDimension(triang_half_INDEX(nmo-1,nmo-1)+1);

  /// getting AO integrals
  RefSymmSCMatrix aoints = localkit->symmmatrix(triangdim);

  Ref<TwoBodyInt> twoint = integral()->electron_repulsion();
  const double *buffer = twoint->buffer();

  Ref<GaussianBasisSet> basis = this->basis();

  int nshell = basis->nshell();
  for(int P = 0; P < nshell; P++) {
    int nump = basis->shell(P).nfunction();

    for(int Q = 0; Q < nshell; Q++) {
      int numq = basis->shell(Q).nfunction();

      for(int R = 0; R < nshell; R++) {
        int numr = basis->shell(R).nfunction();

        for(int S = 0; S < nshell; S++) {
          int nums = basis->shell(S).nfunction();

          twoint->compute_shell(P,Q,R,S);

          int index = 0;
          for(int p=0; p < nump; p++) {
            int op = basis->shell_to_function(P)+p;

            for(int q = 0; q < numq; q++) {
              int oq = basis->shell_to_function(Q)+q;

              for(int r = 0; r < numr; r++) {
                int oor = basis->shell_to_function(R)+r;

                for(int s = 0; s < nums; s++,index++) {
                  int os = basis->shell_to_function(S)+s;

                  aoints.set_element(triang_half_INDEX(op,oq),triang_half_INDEX(oor,os),buffer[index]);

                }
              }
            }
          }

        }
      }
    }
  }

  twoint = 0;

  /// tranformation of rs indices to MOs.
  RefSCMatrix moints_pqRS = localkit->matrix(triangdim,triangdim); // moints in chemist's notation order
  RefSymmSCMatrix mat_rs = localkit->symmmatrix(naodim);
  RefSymmSCMatrix mat_RS = localkit->symmmatrix(nmodim);
  for(int p=0; p<nmo; p++) {
    for(int q=0; q<=p; q++) {
      int ind_pq = triang_half_INDEX(p,q);
      RefSCVector vec_rs = aoints.get_row(ind_pq);
      vector_to_symmmatrix(mat_rs,vec_rs);
      mat_RS.assign(0.0);
      mat_RS.accumulate_transform(coeffs_nb,mat_rs,SCMatrix::TransposeTransform);
      symmmatrix_to_vector(vec_rs,mat_RS);
      moints_pqRS.assign_row(vec_rs,ind_pq);
    }
  }

  aoints = RefSymmSCMatrix();

  /// transformation of pq indices to MOs
  RefSymmSCMatrix moints_PQRS = localkit->symmmatrix(triangdim);  // moints in chemist's notation order
  RefSymmSCMatrix mat_pq = localkit->symmmatrix(naodim);
  RefSymmSCMatrix mat_PQ = localkit->symmmatrix(nmodim);
  for(int R=0; R<nmo; R++) {
    for(int S=0; S<=R; S++) {
      int ind_RS = triang_half_INDEX(R,S);
      RefSCVector vec_pq = moints_pqRS.get_column(ind_RS);
      vector_to_symmmatrix(mat_pq,vec_pq);
      mat_PQ.assign(0.0);
      mat_PQ.accumulate_transform(coeffs_nb,mat_pq,SCMatrix::TransposeTransform);
      symmmatrix_to_vector(vec_pq,mat_PQ);
      moints_PQRS.assign_row(vec_pq,ind_RS);
    }
  }

  moints_pqRS = RefSCMatrix();

  return(moints_PQRS);
}

RefSCMatrix PT2R12::moints(SpinCase2 pairspin) {
  Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
  Ref<OrbitalSpace> space1;
  Ref<OrbitalSpace> space2;

  if(pairspin == AlphaBeta) {
    space1 = r12eval_->orbs(Alpha);
    space2 = r12eval_->orbs(Beta);
  }
  if(pairspin == AlphaAlpha) {
    space1 = r12eval_->orbs(Alpha);
    space2 = r12eval_->orbs(Alpha);
  }
  if(pairspin == BetaBeta) {
    space1 = r12eval_->orbs(Beta);
    space2 = r12eval_->orbs(Beta);
  }

  RefSCMatrix coeffs1 = space1->coefs();
  RefSCMatrix coeffs2 = space2->coefs();

  RefSCDimension nmodim = coeffs1.rowdim();
  RefSCDimension naodim = coeffs1.coldim();
  int nmo = nmodim.n();
  int nao = naodim.n();
  RefSCDimension nmodim_nb = new SCDimension(nmo);
  RefSCDimension naodim_nb = new SCDimension(nao);

  RefSCMatrix coeffs1_nb = localkit->matrix(naodim,nmodim);
  for(int i=0; i<nao; i++) {
    for(int j=0; j<nmo; j++) {
      coeffs1_nb.set_element(i,j,coeffs1.get_element(i,j));
    }
  }
  RefSCMatrix coeffs2_nb = localkit->matrix(naodim,nmodim);
  for(int i=0; i<nao; i++) {
    for(int j=0; j<nmo; j++) {
      coeffs2_nb.set_element(i,j,coeffs2.get_element(i,j));
    }
  }

  RefSCDimension triangdim = new SCDimension(triang_half_INDEX(nmo-1,nmo-1)+1);

  /// getting AO integrals
  RefSymmSCMatrix aoints = localkit->symmmatrix(triangdim);

  Ref<TwoBodyInt> twoint = integral()->electron_repulsion();
  const double *buffer = twoint->buffer();

  Ref<GaussianBasisSet> basis = this->basis();

  int nshell = basis->nshell();
  for(int P = 0; P < nshell; P++) {
    int nump = basis->shell(P).nfunction();

    for(int Q = 0; Q < nshell; Q++) {
      int numq = basis->shell(Q).nfunction();

      for(int R = 0; R < nshell; R++) {
        int numr = basis->shell(R).nfunction();

        for(int S = 0; S < nshell; S++) {
          int nums = basis->shell(S).nfunction();

          twoint->compute_shell(P,Q,R,S);

          int index = 0;
          for(int p=0; p < nump; p++) {
            int op = basis->shell_to_function(P)+p;

            for(int q = 0; q < numq; q++) {
              int oq = basis->shell_to_function(Q)+q;

              for(int r = 0; r < numr; r++) {
                int oor = basis->shell_to_function(R)+r;

                for(int s = 0; s < nums; s++,index++) {
                  int os = basis->shell_to_function(S)+s;


                  aoints.set_element(triang_half_INDEX(op,oq),triang_half_INDEX(oor,os),buffer[index]);

                }
              }
            }
          }

        }
      }
    }
  }

  twoint = 0;

  /// tranformation of rs indices to MOs.
  RefSCMatrix moints_pqRS = localkit->matrix(triangdim,triangdim); // moints in chemist's notation order
  RefSymmSCMatrix mat_rs = localkit->symmmatrix(naodim);
  RefSymmSCMatrix mat_RS = localkit->symmmatrix(nmodim);
  for(int p=0; p<nmo; p++) {
    for(int q=0; q<=p; q++) {
      int ind_pq = triang_half_INDEX(p,q);
      RefSCVector vec_rs = aoints.get_row(ind_pq);
      vector_to_symmmatrix(mat_rs,vec_rs);
      mat_RS.assign(0.0);
      mat_RS.accumulate_transform(coeffs2_nb,mat_rs,SCMatrix::TransposeTransform);
      symmmatrix_to_vector(vec_rs,mat_RS);
      moints_pqRS.assign_row(vec_rs,ind_pq);
    }
  }

  aoints = RefSymmSCMatrix();

  /// transformation of pq indices to MOs
  RefSCMatrix moints_PQRS = localkit->matrix(triangdim,triangdim);  // moints in chemist's notation order
  RefSymmSCMatrix mat_pq = localkit->symmmatrix(naodim);
  RefSymmSCMatrix mat_PQ = localkit->symmmatrix(nmodim);
  for(int R=0; R<nmo; R++) {
    for(int S=0; S<=R; S++) {
      int ind_RS = triang_half_INDEX(R,S);
      RefSCVector vec_pq = moints_pqRS.get_column(ind_RS);
      vector_to_symmmatrix(mat_pq,vec_pq);
      mat_PQ.assign(0.0);
      mat_PQ.accumulate_transform(coeffs1_nb,mat_pq,SCMatrix::TransposeTransform);
      symmmatrix_to_vector(vec_pq,mat_PQ);
      moints_PQRS.assign_row(vec_pq,ind_RS);
    }
  }

  moints_pqRS = RefSCMatrix();

  return(moints_PQRS);
}

RefSCMatrix PT2R12::g(SpinCase2 pairspin,
                      const Ref<OrbitalSpace>& space1,
                      const Ref<OrbitalSpace>& space2) {
  const Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
  Ref<SpinMOPairIter> PQ_iter = new SpinMOPairIter(space1,space2,pairspin);
  const int nmo1 = space1->rank();
  const int nmo2 = space2->rank();
  const int pairrank = PQ_iter->nij();
  const RefSCDimension pairdim = new SCDimension(pairrank);
  RefSCMatrix G = localkit->matrix(pairdim,pairdim);
  G.assign(0.0);

  // find equivalent spaces in the registry
  Ref<OrbitalSpaceRegistry> oreg = this->r12world()->world()->tfactory()->orbital_registry();
  if (!oreg->value_exists(space1) || !oreg->value_exists(space2))
    throw ProgrammingError("PT2R12::g() -- spaces must be registered",__FILE__,__LINE__);
  const std::string key1 = oreg->key(space1);
  Ref<OrbitalSpace> s1 = oreg->value(key1);
  const std::string key2 = oreg->key(space2);
  Ref<OrbitalSpace> s2 = oreg->value(key2);

  const bool antisymmetrize = (pairspin==AlphaBeta) ? false : true;
  std::vector<std::string> tforms;
  {
    const std::string tform_key = ParsedTwoBodyFourCenterIntKey::key(s1->id(),s2->id(),
                                                                     s1->id(),s2->id(),
                                                                     std::string("ERI"),
                                                                     std::string(TwoBodyIntLayout::b1b2_k1k2));
    tforms.push_back(tform_key);
  }

  r12eval_->compute_tbint_tensor<ManyBodyTensors::I_to_T,false,false>(G,TwoBodyOper::eri,s1,s1,s2,s2,
      antisymmetrize,tforms);

  return(G);
}

RefSCMatrix PT2R12::g(SpinCase2 pairspin,
                      const Ref<OrbitalSpace>& bra1,
                      const Ref<OrbitalSpace>& bra2,
                      const Ref<OrbitalSpace>& ket1,
                      const Ref<OrbitalSpace>& ket2)
{
      const Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
      Ref<SpinMOPairIter> braiter12 = new SpinMOPairIter(bra1,bra2,pairspin);
      Ref<SpinMOPairIter> ketiter12 = new SpinMOPairIter(ket1,ket2,pairspin);
      RefSCMatrix G = localkit->matrix(new SCDimension(braiter12->nij()),
                                       new SCDimension(ketiter12->nij()));
      G.assign(0.0);

      const int nket1 = ket1->rank();
      const int nket2 = ket2->rank();

      const bool antisymmetrize = (pairspin != AlphaBeta);
      const bool bra1_eq_bra2 = (*bra1 == *bra2);
      const bool ket1_eq_ket2 = (*ket1 == *ket2);


      // find equivalent spaces in the registry
      Ref<OrbitalSpaceRegistry> oreg = this->r12world()->world()->tfactory()->orbital_registry();
      if (!oreg->value_exists(bra1) || !oreg->value_exists(bra2) || !oreg->value_exists(ket1) || !oreg->value_exists(ket2))
        throw ProgrammingError("PT2R12::g() -- spaces must be registered",__FILE__,__LINE__);


      // compute (bra1 ket1 | bra2 ket2)
      Ref<DistArray4> da4_b1k1_b2k2;
      {
        const std::string tform_b1k1_b2k2_key = ParsedTwoBodyFourCenterIntKey::key(bra1->id(),bra2->id(),
                                                                                   ket1->id(),ket2->id(),
                                                                                   std::string("ERI"),
                                                                                   std::string(TwoBodyIntLayout::b1b2_k1k2));
        Ref<TwoBodyMOIntsTransform> tform_b1k1_b2k2 = this->r12world()->world()->moints_runtime4()->get( tform_b1k1_b2k2_key );
        tform_b1k1_b2k2->compute();
        da4_b1k1_b2k2 = tform_b1k1_b2k2->ints_acc();
      }
      da4_b1k1_b2k2->activate();


      Ref<DistArray4> da4_b2k1_b1k2;
      const bool need_b2k1_b1k2 = !bra1_eq_bra2 && !ket1_eq_ket2 && antisymmetrize; // if(...) then essentially there is no interchangablility since the index pair are
      if (need_b2k1_b1k2)                                                           // in different spaces;
      { // compute (bra2 ket1 | bra1 ket2)
        const std::string tform_b2k1_b1k2_key = ParsedTwoBodyFourCenterIntKey::key(bra2->id(),bra1->id(),
                                                                                   ket1->id(),ket2->id(),
                                                                                   std::string("ERI"),
                                                                                   std::string(TwoBodyIntLayout::b1b2_k1k2));
        Ref<TwoBodyMOIntsTransform> tform_b2k1_b1k2 = this->r12world()->world()->moints_runtime4()->get( tform_b2k1_b1k2_key );
        tform_b2k1_b1k2->compute();
        da4_b2k1_b1k2 = tform_b2k1_b1k2->ints_acc();
        da4_b2k1_b1k2->activate();
      }

      for(braiter12->start(); *braiter12; braiter12->next())
      {
        const int b1 = braiter12->i();
        const int b2 = braiter12->j();
        const int b12 = braiter12->ij();

        const double* blk_b1b2 = da4_b1k1_b2k2->retrieve_pair_block(b1, b2, 0);

        const double* blk_b2b1 = 0;
        if (antisymmetrize && !ket1_eq_ket2 && bra1_eq_bra2)  // bra1 and bra2 are in the same space, but ket1 and ket2 not; we get b2b1 block from da4_b1k1_b2k2
          blk_b2b1 = da4_b1k1_b2k2->retrieve_pair_block(b2, b1, 0);
        else if (antisymmetrize && need_b2k1_b1k2)            // neither of the index pairs are in the same space; then get b2b1 block from da4_b2k1_b1k2
          blk_b2b1 = da4_b2k1_b1k2->retrieve_pair_block(b2, b1, 0);

        for(ketiter12->start(); *ketiter12; ketiter12->next())
        {
          const int k1 = ketiter12->i();
          const int k2 = ketiter12->j();
          const int k12 = ketiter12->ij();

          if (!antisymmetrize)
            G.set_element( b12, k12, blk_b1b2[k12] );
          else
          {
            const int k12_rect = k1 * nket2 + k2;
            if (blk_b2b1) {  // this branch is for the case when we need b2b1 block
              G.set_element( b12, k12, blk_b1b2[k12_rect] - blk_b2b1[k12_rect] );
            }
            else {          // the other case: ket1 and ket2 are in the same space; we can antisymmetrize G by subtracting k21 from k12
              const int k21_rect = k2 * nket1 + k1;
              G.set_element( b12, k12, blk_b1b2[k12_rect] - blk_b1b2[k21_rect] );
            }
          }
        }

        da4_b1k1_b2k2->release_pair_block(b1, b2, 0);
        if (blk_b2b1) {
          if (need_b2k1_b1k2)
            da4_b2k1_b1k2->release_pair_block(b2, b1, 0);
          else
            da4_b1k1_b2k2->release_pair_block(b2, b1, 0);
        }
      }
  return(G);
}

RefSCMatrix PT2R12::f(SpinCase1 spin) {
  Ref<OrbitalSpace> space = rdm1_->orbs(spin);
  Ref<OrbitalSpaceRegistry> oreg = this->r12world()->world()->tfactory()->orbital_registry();
  if (!oreg->value_exists(space)) {
    oreg->add(make_keyspace_pair(space));
  }
  const std::string key = oreg->key(space);
  space = oreg->value(key);
  RefSCMatrix fmat = r12eval_->fock(space,space,spin);

  return(fmat);
}


RefSCMatrix PT2R12::C(SpinCase2 S) {
  //return(r12energy_->C(S));
  Ref<LocalSCMatrixKit> local_matrix_kit = new LocalSCMatrixKit();
  RefSCMatrix Cmat = local_matrix_kit->matrix(r12eval_->dim_GG(S),r12eval_->dim_gg(S));
  if(S==AlphaBeta) {
    SpinMOPairIter OW_iter(r12eval_->GGspace(Alpha), r12eval_->GGspace(Beta), S );
    SpinMOPairIter PQ_iter(r12eval_->ggspace(Alpha), r12eval_->GGspace(Beta), S );
    Ref<R12Technology::GeminalDescriptor> geminaldesc = r12world()->r12tech()->corrfactor()->geminaldescriptor();
    CuspConsistentGeminalCoefficient coeff_gen(S,geminaldesc);
    for(OW_iter.start(); int(OW_iter); OW_iter.next()) {
      for(PQ_iter.start(); int(PQ_iter); PQ_iter.next()) {
        unsigned int O = OW_iter.i();
        unsigned int W = OW_iter.j();
        unsigned int P = PQ_iter.i();
        unsigned int Q = PQ_iter.j();
        int OW = OW_iter.ij();
        int PQ = PQ_iter.ij();
        Cmat.set_element(OW,PQ,coeff_gen.C(O,W,P,Q));
      }
    }
  }
  else {
    SpinCase1 spin = (S==AlphaAlpha) ? Alpha : Beta;
    SpinMOPairIter OW_iter(r12eval_->GGspace(spin), r12eval_->GGspace(spin), S );
    SpinMOPairIter PQ_iter(r12eval_->ggspace(spin), r12eval_->GGspace(spin), S );
    Ref<R12Technology::GeminalDescriptor> geminaldesc = r12world()->r12tech()->corrfactor()->geminaldescriptor();
    CuspConsistentGeminalCoefficient coeff_gen(S,geminaldesc);
    for(OW_iter.start(); int(OW_iter); OW_iter.next()) {
      for(PQ_iter.start(); int(PQ_iter); PQ_iter.next()) {
        unsigned int O = OW_iter.i();
        unsigned int W = OW_iter.j();
        unsigned int P = PQ_iter.i();
        unsigned int Q = PQ_iter.j();
        int OW = OW_iter.ij();
        int PQ = PQ_iter.ij();
        Cmat.set_element(OW,PQ,coeff_gen.C(O,W,P,Q));
      }
    }
  }
  return(Cmat);
}

RefSCMatrix PT2R12::V_genref_projector2(SpinCase2 pairspin) {
  SpinCase1 spin1 = case1(pairspin);
  SpinCase1 spin2 = case2(pairspin);

  const Ref<OrbitalSpace>& GG1_space = r12eval_->GGspace(spin1);
  const Ref<OrbitalSpace>& GG2_space = r12eval_->GGspace(spin2);
  const Ref<OrbitalSpace>& gg1_space = r12eval_->ggspace(spin1);
  const Ref<OrbitalSpace>& gg2_space = r12eval_->ggspace(spin2);

  Ref<SCMatrixKit> localkit = new LocalSCMatrixKit;
  RefSCMatrix V_genref = localkit->matrix(r12eval_->dim_GG(pairspin),r12eval_->dim_gg(pairspin));
  V_genref.assign(0.0);

  const bool antisymmetrize = pairspin!=AlphaBeta;
  const bool part1_equiv_part2 = (GG1_space == GG2_space && gg1_space == gg2_space);

  const RefSCMatrix V_intermed = r12eval_->V(pairspin);
  const RefSymmSCMatrix tpdm = rdm2_gg(pairspin);
  V_genref.accumulate(V_intermed * tpdm);

#if 0
  V_intermed.print(prepend_spincase(pairspin, "V").c_str());
  tpdm.print(prepend_spincase(pairspin, "gamma").c_str());
  V_genref.print(prepend_spincase(pairspin, "V.gamma").c_str());
#endif

  return(V_genref);
}

RefSCMatrix PT2R12::V_transformed_by_C(SpinCase2 pairspin) {
  RefSCMatrix T = C(pairspin);
  RefSCMatrix V = r12eval_->V(pairspin);
  RefSCMatrix Vtransposed = V.t();
  RefSCMatrix VT = Vtransposed*T;
  return(VT);
}

RefSymmSCMatrix PT2R12::X_transformed_by_C(SpinCase2 pairspin) {
  RefSCMatrix T = C(pairspin);
  RefSymmSCMatrix X = r12eval_->X(pairspin);
  RefSCDimension transformed_dim = T.coldim();
  RefSymmSCMatrix XT = T.kit()->symmmatrix(transformed_dim);
  XT.assign(0.0);
  XT.accumulate_transform(T,X,SCMatrix::TransposeTransform);

  return(XT);
}

RefSymmSCMatrix PT2R12::B_transformed_by_C(SpinCase2 pairspin) {
  RefSymmSCMatrix B = r12eval_->B(pairspin);
  RefSCMatrix T = C(pairspin);
  RefSCDimension transformed_dim = T.coldim();
  RefSymmSCMatrix BT = T.kit()->symmmatrix(transformed_dim);
  BT.assign(0.0);
  BT.accumulate_transform(T,B,SCMatrix::TransposeTransform);

  return(BT);
}

RefSymmSCMatrix PT2R12::phi_cumulant(SpinCase2 spin12) {
  RefSCMatrix fmat[NSpinCases1];
  RefSymmSCMatrix opdm[NSpinCases1];
  RefSymmSCMatrix tpcm[NSpinCases2];

  for(int s=0; s<NSpinCases1; s++) {
    SpinCase1 spin = static_cast<SpinCase1>(s);
    fmat[s] = f(spin);
    opdm[s] = rdm1(spin);
  }
  const int nmo = opdm[0].dim().n();

  // R12 intermediates, transformed with geminal amplitude matrix
  for(int i=0; i<NSpinCases2; i++) {
    SpinCase2 S = static_cast<SpinCase2>(i);
    tpcm[i] = lambda2(S);
  }

  // precompute intermediates
  RefSCMatrix K[NSpinCases1];   // \gamma f
  RefSCMatrix I[NSpinCases1];   // \gamma f \gamma
  RefSCMatrix M[NSpinCases1];   // trace(f lambda)
  for(int i=0; i<NSpinCases1; i++) {
    K[i] = fmat[i].clone();
    K[i].assign(0.0);
    for(int u=0; u<nmo; u++) {
      for(int z=0; z<nmo; z++) {
        for(int y=0; y<nmo; y++) {
          K[i].accumulate_element(u,z,opdm[i].get_element(u,y)*fmat[i].get_element(y,z));
        }
      }
    }
    I[i] = fmat[i].clone();
    I[i].assign(0.0);
    for(int q=0; q<nmo; q++) {
      for(int v=0; v<nmo; v++) {
        for(int z=0; z<nmo; z++) {
            I[i].accumulate_element(q,v,K[i].get_element(q,z)*opdm[i].get_element(z,v));
        }
      }
    }
  }
#if 0
  for(int s=0; s<NSpinCases1; ++s) {
    K[s] = opdm[s] * fmat[s];
    I[s] = K[s] * opdm[s];
  }
#endif

  M[Alpha] = fmat[Alpha].clone(); M[Alpha].assign(0.0);
  M[Beta] = fmat[Beta].clone(); M[Beta].assign(0.0);
  // each M has contributions from 2 pairspincases
  // AlphaAlpha contributes to Alpha only
  for(int P=0; P<nmo; ++P) {
    for(int Q=0; Q<nmo; ++Q) {
      int PQ_aa, sign_PQ=+1;
      if (P > Q) {
        PQ_aa = P*(P-1)/2 + Q;
      }
      else {
        PQ_aa = Q*(Q-1)/2 + P;
        sign_PQ = -1;
      }
      const int PQ_ab = P * nmo + Q;
      const int QP_ab = Q * nmo + P;
      for(int U=0; U<nmo; ++U) {
        for(int V=0; V<nmo; ++V) {
          int UV_aa, sign_UV=+1;
          if (U > V) {
            UV_aa = U*(U-1)/2 + V;
          }
          else {
            UV_aa = V*(V-1)/2 + U;
            sign_UV = -1;
          }
          const int UV_ab = U * nmo + V;
          const int VU_ab = V * nmo + U;

          double M_PU_a = 0.0;
          double M_PU_b = 0.0;
          if (P!=Q && U!=V) {
            M_PU_a += sign_PQ * sign_UV * fmat[Alpha].get_element(Q, V) * tpcm[AlphaAlpha].get_element(PQ_aa, UV_aa);
            M_PU_b += sign_PQ * sign_UV * fmat[Beta].get_element(Q, V) * tpcm[BetaBeta].get_element(PQ_aa, UV_aa);
          }
          M_PU_a += fmat[Beta].get_element(Q, V) * tpcm[AlphaBeta].get_element(PQ_ab, UV_ab);
          M_PU_b += fmat[Alpha].get_element(Q, V) * tpcm[AlphaBeta].get_element(QP_ab, VU_ab);
          M[Alpha].accumulate_element(P, U, M_PU_a);
          M[Beta].accumulate_element(P, U, M_PU_b);
        }
      }
    }
  }

  if (this->debug_ >= DefaultPrintThresholds::allN2) {
    for(int s=0; s<NSpinCases1; ++s) {
      const SpinCase1 spin = static_cast<SpinCase1>(s);
      K[s].print(prepend_spincase(spin,"K new").c_str());
      I[s].print(prepend_spincase(spin,"I new").c_str());
      M[s].print(prepend_spincase(spin,"M new").c_str());
      fmat[s].print(prepend_spincase(spin,"Fock matrix").c_str());
    }
  }

  // compute phi:
  // \phi^{u v}_{p q} = & P(p q) P(u v) (\gamma^u_p \gamma^{q_3}_q f^{q_2}_{q_3} \gamma^v_{q_2}
  //                                     + \frac{1}{2} \gamma^v_{q_2} f^{q_2}_{q_3} \lambda^{u q_3}_{p q}
  //                                     + \frac{1}{2} \gamma^{q_2}_p f^{q_3}_{q_2} \lambda^{u v}_{q_3 q}
  //                                     - \gamma^u_p f^{q_2}_{q_3} \lambda^{q_3 v}_{q_2 q})
  Ref<LocalSCMatrixKit> local_kit = new LocalSCMatrixKit();
  RefSymmSCMatrix phi = local_kit->symmmatrix(tpcm[spin12].dim());
  phi.assign(0.0);
  const SpinCase1 spin1 = case1(spin12);
  const SpinCase1 spin2 = case2(spin12);
  Ref<OrbitalSpace> orbs1 = rdm2_->orbs(spin1);
  Ref<OrbitalSpace> orbs2 = rdm2_->orbs(spin2);
  SpinMOPairIter UV_iter(orbs1,orbs2,spin12);
  SpinMOPairIter PQ_iter(orbs1,orbs2,spin12);

  if (spin12 == AlphaBeta) {
    for(PQ_iter.start(); int(PQ_iter); PQ_iter.next()) {
      const int P = PQ_iter.i();
      const int Q = PQ_iter.j();
      const int PQ = PQ_iter.ij();
      const int pp = P;
      const int qq = Q;
      const int pq = pp * nmo + qq;

      for(UV_iter.start(); int(UV_iter); UV_iter.next()) {
        const int U = UV_iter.i();
        const int V = UV_iter.j();
        const int UV = UV_iter.ij();
        const int uu = U;
        const int vv = V;
        const int uv = uu * nmo + vv;

        // first term is easy
        double phi_PQ_UV = (   I[Alpha].get_element(pp, uu) * opdm[Beta].get_element(qq, vv) +
                            opdm[Alpha].get_element(pp, uu) *    I[Beta].get_element(qq, vv)
                           );

        // second and third terms contain contributions from the cumulant of same spin
        for(int q3=0; q3<nmo; ++q3) {
          int uq3 = uu * nmo + q3;
          int q3v = q3 * nmo + vv;
          int pq3 = pp * nmo + q3;
          int q3q = q3 * nmo + qq;
          // +1 instead of +1/2 because there are 2 permutation operators!
          phi_PQ_UV += (tpcm[AlphaBeta].get_element(pq, uq3) * K[Beta].get_element(vv, q3) +
                        tpcm[AlphaBeta].get_element(pq, q3v) * K[Alpha].get_element(uu, q3) +
                        tpcm[AlphaBeta].get_element(pq3, uv) * K[Beta].get_element(qq, q3) +
                        tpcm[AlphaBeta].get_element(q3q, uv) * K[Alpha].get_element(pp, q3)
                       );
        }

        // fourth term is also easy
        phi_PQ_UV -= (   M[Alpha].get_element(pp, uu) * opdm[Beta].get_element(qq, vv) +
                      opdm[Alpha].get_element(pp, uu) *    M[Beta].get_element(qq, vv)
                     );

        phi(PQ,UV) = phi_PQ_UV;
      }
    }
  }
  else if (spin12 == AlphaAlpha || spin12 == BetaBeta) {
    const SpinCase1 spin = spin1;
    for(PQ_iter.start(); int(PQ_iter); PQ_iter.next()) {
      const int P = PQ_iter.i();
      const int Q = PQ_iter.j();
      const int PQ = PQ_iter.ij();
      const int pp = P;
      const int qq = Q;
      assert(pp >= qq);
      const int pq = pp * (pp - 1) / 2 + qq;

      for(UV_iter.start(); int(UV_iter); UV_iter.next()) {
        const int U = UV_iter.i();
        const int V = UV_iter.j();
        const int UV = UV_iter.ij();
        const int uu = U;
        const int vv = V;
        assert(uu >= vv);
        const int uv = uu * (uu - 1) / 2 + vv;

        // first term is easy
        double phi_PQ_UV = (   I[spin].get_element(pp, uu) * opdm[spin].get_element(qq, vv) +
                            opdm[spin].get_element(pp, uu) *    I[spin].get_element(qq, vv) -
                               I[spin].get_element(pp, vv) * opdm[spin].get_element(qq, uu) -
                            opdm[spin].get_element(pp, vv) *    I[spin].get_element(qq, uu)
                           );

        // second and third terms contain contributions from the cumulant of same spin
        // +1 instead of +1/2 because there are 2 permutation operators!
        // V
        for(int q3=0; q3<uu; ++q3) {
          const int uq3 = uu * (uu-1)/2 + q3;
          phi_PQ_UV += tpcm[spin12].get_element(pq, uq3) * K[spin].get_element(vv, q3);
        }
        for(int q3=uu+1; q3<nmo; ++q3) {
          const int q3u = q3 * (q3-1)/2 + uu;
          // - sign!
          phi_PQ_UV -= tpcm[spin12].get_element(pq, q3u) * K[spin].get_element(vv, q3);
        }
        // U
        for(int q3=0; q3<vv; ++q3) {
          const int vq3 = vv * (vv-1)/2 + q3;
          // - sign!
          phi_PQ_UV -= tpcm[spin12].get_element(pq, vq3) * K[spin].get_element(uu, q3);
        }
        for(int q3=vv+1; q3<nmo; ++q3) {
          const int q3v = q3 * (q3-1)/2 + vv;
          phi_PQ_UV += tpcm[spin12].get_element(pq, q3v) * K[spin].get_element(uu, q3);
        }
        // Q
        for(int q3=0; q3<pp; ++q3) {
          const int pq3 = pp * (pp-1)/2 + q3;
          phi_PQ_UV += tpcm[spin12].get_element(pq3, uv) * K[spin].get_element(qq, q3);
        }
        for(int q3=pp+1; q3<nmo; ++q3) {
          const int q3p = q3 * (q3-1)/2 + pp;
          // - sign!
          phi_PQ_UV -= tpcm[spin12].get_element(q3p, uv) * K[spin].get_element(qq, q3);
        }
        // P
        for(int q3=0; q3<qq; ++q3) {
          const int qq3 = qq * (qq-1)/2 + q3;
          // - sign!
          phi_PQ_UV -= tpcm[spin12].get_element(qq3, uv) * K[spin].get_element(pp, q3);
        }
        for(int q3=qq+1; q3<nmo; ++q3) {
          const int q3q = q3 * (q3-1)/2 + qq;
          // -1 instead of -1/2 because there are 2 permutation operators!
          phi_PQ_UV += tpcm[spin12].get_element(q3q, uv) * K[spin].get_element(pp, q3);
        }

        // fourth term is also easy
        phi_PQ_UV -= (    M[spin].get_element(pp, uu) * opdm[spin].get_element(qq, vv) +
                       opdm[spin].get_element(pp, uu) *    M[spin].get_element(qq, vv) -
                          M[spin].get_element(pp, vv) * opdm[spin].get_element(qq, uu) -
                       opdm[spin].get_element(pp, vv) *    M[spin].get_element(qq, uu)
                     );

        phi(PQ,UV) = phi_PQ_UV;
      }
    }
  }
  else { // invalid spin12
    assert(false);
  }

  if(debug_>=DefaultPrintThresholds::mostO4) {
    phi->print(prepend_spincase(spin12,"phi (new)").c_str());
  }

  return phi;
}

double PT2R12::energy_PT2R12_projector1(SpinCase2 pairspin) {
  const int nelectron = reference_->nelectron();
  SpinCase1 spin1 = case1(pairspin);
  SpinCase1 spin2 = case2(pairspin);
  Ref<OrbitalSpace> gg1space = r12eval_->ggspace(spin1);
  Ref<OrbitalSpace> gg2space = r12eval_->ggspace(spin2);
  SpinMOPairIter gg_iter(gg1space,gg2space,pairspin);

  RefSymmSCMatrix tpdm = rdm2_gg(pairspin);
  RefSymmSCMatrix Phi = phi_gg(pairspin);
  RefSCMatrix VT = V_transformed_by_C(pairspin);
  RefSymmSCMatrix TXT = X_transformed_by_C(pairspin);
  RefSymmSCMatrix TBT = B_transformed_by_C(pairspin);

  ExEnv::out0() << "pairspin "
                << ((pairspin==AlphaBeta) ? "AlphaBeta" : ((pairspin==AlphaAlpha) ? "AlphaAlpha" : "BetaBeta")) << endl;

  // printing pair energies
  RefSCMatrix VT_t_tpdm = 2.0*VT*tpdm;
  RefSCMatrix TBT_t_tpdm = TBT*tpdm;
  RefSCMatrix TXT_t_Phi = TXT*Phi;
  RefSCMatrix HylleraasMatrix = VT_t_tpdm + TBT_t_tpdm - TXT_t_Phi;

  const double energy = this->compute_energy(HylleraasMatrix, pairspin);
  return(energy);
}

double PT2R12::energy_PT2R12_projector2(SpinCase2 pairspin) {
  const int nelectron = reference_->nelectron();
  SpinCase1 spin1 = case1(pairspin);
  SpinCase1 spin2 = case2(pairspin);
  Ref<OrbitalSpace> gg1space = r12eval_->ggspace(spin1);
  Ref<OrbitalSpace> gg2space = r12eval_->ggspace(spin2);
  SpinMOPairIter gg_iter(gg1space,gg2space,pairspin);

  RefSymmSCMatrix TBT = B_transformed_by_C(pairspin);
  RefSymmSCMatrix tpdm = rdm2_gg(pairspin);
  RefSCMatrix TBT_tpdm = TBT*tpdm;
  RefSCMatrix HylleraasMatrix = TBT_tpdm;
  if (this->debug_ >=  DefaultPrintThresholds::mostO4) {
    TBT.print(prepend_spincase(pairspin,"TBT").c_str());
    TBT_tpdm.print(prepend_spincase(pairspin,"TBTg").c_str());
  }
  TBT=0;
  tpdm=0;

  RefSymmSCMatrix TXT = X_transformed_by_C(pairspin);
  RefSymmSCMatrix Phi = phi_gg(pairspin);
  RefSCMatrix TXT_t_Phi = TXT*Phi; TXT_t_Phi.scale(-1.0);
  HylleraasMatrix.accumulate(TXT_t_Phi);
  if (this->debug_ >=  DefaultPrintThresholds::mostO4) {
    TXT.print(prepend_spincase(pairspin,"TXT").c_str());
    TXT_t_Phi.print(prepend_spincase(pairspin,"-TXTf").c_str());
  }
  TXT=0;
  Phi=0;
  TXT_t_Phi=0;

  RefSCMatrix V_genref = V_genref_projector2(pairspin);
  RefSCMatrix T = C(pairspin);
  RefSCMatrix V_t_T = 2.0*V_genref.t()*T;
  HylleraasMatrix.accumulate(V_t_T);
  if (this->debug_ >=  DefaultPrintThresholds::mostO4) {
    V_genref.print(prepend_spincase(pairspin,"Vg").c_str());
    V_t_T.print(prepend_spincase(pairspin,"gVT").c_str());
    HylleraasMatrix.print(prepend_spincase(pairspin,"H2").c_str());
  }
  T=0;
  V_genref=0;

  const double energy = this->compute_energy(HylleraasMatrix, pairspin);
  return(energy);
}

namespace {
  RefSymmSCMatrix convert_to_local_kit(const RefSymmSCMatrix& A) {
    RefSymmSCMatrix result;
    Ref<LocalSCMatrixKit> kit_cast_to_local; kit_cast_to_local << A.kit();
    if (kit_cast_to_local.null()) {
      Ref<LocalSCMatrixKit> local_kit = new LocalSCMatrixKit();
      RefSymmSCMatrix A_local = local_kit->symmmatrix(A.dim());
      A_local->convert(A);
      result = A_local;
    }
    else
      result = A;
    return result;
  }
}

RefSymmSCMatrix sc::PT2R12::rdm1(SpinCase1 spin)
{
  return convert_to_local_kit(rdm1_->scmat(spin));
}

RefSymmSCMatrix sc::PT2R12::rdm2(SpinCase2 spin)
{
  // since LocalSCMatrixKit is used everywhere, convert to Local kit
  return convert_to_local_kit(rdm2_->scmat(spin));
}

RefSymmSCMatrix sc::PT2R12::lambda2(SpinCase2 spin)
{
  // since LocalSCMatrixKit is used everywhere, convert to Local kit
  return convert_to_local_kit(rdm2_->cumulant()->scmat(spin));
}

RefSymmSCMatrix sc::PT2R12::rdm1_gg(SpinCase1 spin)
{
  RefSymmSCMatrix rdm = this->rdm1(spin);
  Ref<OrbitalSpace> orbs = rdm1_->orbs(spin);
  Ref<OrbitalSpace> gspace = r12eval_->ggspace(spin);
  // if density is already in required spaces?
  if (*orbs == *gspace)
    return rdm;

  Ref<LocalSCMatrixKit> local_kit = new LocalSCMatrixKit;
  RefSymmSCMatrix result = local_kit->symmmatrix(gspace->dim());
  result.assign(0.0);
  // it's possible for gspace to be a superset of orbs
  std::vector<int> omap = map(*orbs, *gspace);
  const int nmo = orbs->rank();

  for(int R=0; R<nmo; ++R)
    for(int C=0; C<=R; ++C) {
      const int rr = omap[R];
      const int cc = omap[C];
      if (rr == -1 || cc == -1) continue;
      const double rdm_R_C = rdm.get_element(rr, cc);
      result.set_element(R, C, rdm_R_C);
    }

  return result;
}

RefSymmSCMatrix sc::PT2R12::rdm2_gg(SpinCase2 spin)
{
  RefSymmSCMatrix rdm = this->rdm2(spin);
  return this->_rdm2_to_gg(spin, rdm);
}

RefSymmSCMatrix sc::PT2R12::lambda2_gg(SpinCase2 spin)
{
  RefSymmSCMatrix lambda = this->lambda2(spin);
  return this->_rdm2_to_gg(spin, lambda);
}

RefSymmSCMatrix sc::PT2R12::phi_gg(SpinCase2 spin)
{
  RefSymmSCMatrix phi = this->phi_cumulant(spin);
  return this->_rdm2_to_gg(spin, phi);
}

RefSymmSCMatrix sc::PT2R12::_rdm2_to_gg(SpinCase2 spin,
                                        RefSymmSCMatrix rdm)
{
  const SpinCase1 spin1 = case1(spin);
  const SpinCase1 spin2 = case2(spin);
  Ref<OrbitalSpace> orbs1 = rdm2_->orbs(spin1);
  Ref<OrbitalSpace> orbs2 = rdm2_->orbs(spin2);
  Ref<OrbitalSpace> gspace1 = r12eval_->ggspace(spin1);
  Ref<OrbitalSpace> gspace2 = r12eval_->ggspace(spin2);
  // if density is already in required spaces?
  if (*orbs1 == *gspace1 && *orbs2 == *gspace2)
    return rdm;

  Ref<LocalSCMatrixKit> local_kit = new LocalSCMatrixKit;
  RefSymmSCMatrix result = local_kit->symmmatrix(r12eval_->dim_gg(spin));
  result.assign(0.0);
  // it's possible for gspace to be a superset of orbs
  std::vector<int> map1 = map(*orbs1, *gspace1);
  std::vector<int> map2 = map(*orbs2, *gspace2);
  SpinMOPairIter UV_iter(gspace1,gspace2,spin);
  SpinMOPairIter PQ_iter(gspace1,gspace2,spin);
  const int nmo = orbs1->rank();

  for(PQ_iter.start(); int(PQ_iter); PQ_iter.next()) {
    const int P = PQ_iter.i();
    const int Q = PQ_iter.j();
    const int PQ = PQ_iter.ij();
    const int pp = map1[P];
    const int qq = map2[Q];
    if (pp == -1 || qq == -1) continue;   // skip if these indices are not in the source rdm2
    int pq;
    double pfac_pq = 1.0;
    switch(spin) {
      case AlphaBeta: pq = pp * nmo + qq; break;
      case AlphaAlpha:
      case BetaBeta:
        if (pp > qq) {
          pq = (pp * (pp-1)/2 + qq);
        }
        else {
          pq = (qq * (qq-1)/2 + pp);
          pfac_pq = -1.0;
        }
    }

    for(UV_iter.start(); int(UV_iter); UV_iter.next()) {
      const int U = UV_iter.i();
      const int V = UV_iter.j();
      const int UV = UV_iter.ij();
      const int uu = map1[U];
      const int vv = map2[V];
      if (uu == -1 || vv == -1) continue;   // skip if these indices are not in the source rdm2
      int uv;
      double pfac_uv = 1.0;
      switch(spin) {
        case AlphaBeta: uv = uu * nmo + vv; break;
        case AlphaAlpha:
        case BetaBeta:
          if (uu > vv) {
            uv = (uu * (uu-1)/2 + vv);
          }
          else {
            uv = (vv * (vv-1)/2 + uu);
            pfac_uv = -1.0;
          }
      }

      const double rdm_PQ_UV = pfac_pq * pfac_uv * rdm.get_element(pq, uv);
      result.set_element(PQ, UV, rdm_PQ_UV);
    }
  }

#if 0
  rdm.print(prepend_spincase(spin, "2-rdm (full)").c_str());
  result.print(prepend_spincase(spin, "2-rdm (occ)").c_str());
#endif

  return result;
}

RefSymmSCMatrix sc::PT2R12::density()
{
  throw FeatureNotImplemented("PT2R12::density() not yet implemented");
}

void sc::PT2R12::print(std::ostream & os) const
{
  os << indent << "PT2R12:" << endl;
  os << incindent;
  os << indent << "nfzc = " << nfzc_ << std::endl;
  os << indent << "omit_uocc = " << (omit_uocc_ ? "true" : "false") << std::endl;
  reference_->print(os);
  r12world()->print(os);
  Wavefunction::print(os);
  os << decindent;
}

void sc::PT2R12::compute()
{
  double energy_correction_r12 = 0.0;
  double energy_pt2r12[NSpinCases2];
  const bool spin_polarized = r12world()->ref()->spin_polarized();





if (pt2_correction_)
{
  for(int i=0; i<NSpinCases2; i++) // may comment out this part for pure cas
  {
    SpinCase2 pairspin = static_cast<SpinCase2>(i);
    double scale = 1.0;
    if (pairspin == BetaBeta && !spin_polarized) continue;
    switch (r12world()->r12tech()->ansatz()->projector())
    {
      case R12Technology::Projector_1:
        energy_pt2r12[i] = energy_PT2R12_projector1(pairspin);
        break;
      case R12Technology::Projector_2:
        energy_pt2r12[i] = energy_PT2R12_projector2(pairspin);
        break;
      default:
        abort();
    }
  }
}

  //calculate basis set incompleteness error (BSIE) with two choices of H0
  double alpha_correction = 0.0, beta_correction = 0.0, cabs_singles_correction = 0.0;
  double cabs_singles_correction_twobody_H0 = 0.0;
  const bool keep_result_for_cabs_singles_with_Fock_Hamiltonian = false; // if false, only do [2]_S with Dyall Hamiltonian
  if(cabs_singles_)
  {
    if(keep_result_for_cabs_singles_with_Fock_Hamiltonian)
    {
      alpha_correction = this->energy_cabs_singles(Alpha);
      if (spin_polarized)
        beta_correction =  this->energy_cabs_singles(Beta);
      else
        beta_correction = alpha_correction;
      cabs_singles_correction = alpha_correction + beta_correction;
    }
    cabs_singles_correction_twobody_H0 = this->energy_cabs_singles_twobody_H0();
  }
  if(keep_result_for_cabs_singles_with_Fock_Hamiltonian)
  {
    ExEnv::out0() << indent << scprintf("CABS singles energy correction:        %17.12lf",
                                      cabs_singles_correction) << endl;
    ExEnv::out0() << indent << scprintf("CASSCF+CABS singles correction:        %17.12lf",
                                      reference_->energy() + cabs_singles_correction) << endl;
  }
  ExEnv::out0() << indent << scprintf("CABS correction (twobody H0):          %17.12lf",
                                    cabs_singles_correction_twobody_H0) << endl;
  ExEnv::out0() << indent << scprintf("CASSCF+CABS (twobody H0):              %17.12lf",
                                    reference_->energy() + cabs_singles_correction_twobody_H0) << endl;


  if (!spin_polarized)
       energy_pt2r12[BetaBeta] = energy_pt2r12[AlphaAlpha];
  for(int i=0; i<NSpinCases2; i++)
       energy_correction_r12 +=  energy_pt2r12[i];
  const double energy = reference_->energy() + energy_correction_r12 + cabs_singles_correction_twobody_H0;

  ExEnv::out0() << indent << scprintf("Reference energy [au]:                 %17.12lf",
                                      reference_->energy()) << endl;
  #if 1
  {
  const double recomp_ref_energy = this->energy_recomputed_from_densities();
  ExEnv::out0() << indent << scprintf("Reference energy (%9s) [au]:     %17.12lf",
                                      (this->r12world()->world()->basis_df().null() ? "   recomp" : "recomp+DF"),
                                      recomp_ref_energy) << endl;
  }
  #endif
  ExEnv::out0() << indent << scprintf("Alpha-beta [2]_R12 energy [au]:        %17.12lf",
                                      energy_pt2r12[AlphaBeta]) << endl;
  ExEnv::out0() << indent << scprintf("Alpha-alpha [2]_R12 energy [au]:       %17.12lf",
                                      energy_pt2r12[AlphaAlpha]) << endl;
  if (spin_polarized) {
    ExEnv::out0() << indent << scprintf("Beta-beta [2]_R12 energy [au]:       %17.12lf",
                                        energy_pt2r12[BetaBeta]) << endl;
  }
  else {
    ExEnv::out0() << indent << scprintf("Singlet [2]_R12 energy [au]:           %17.12lf",
                                        energy_pt2r12[AlphaBeta] - energy_pt2r12[AlphaAlpha]) << endl;
    ExEnv::out0() << indent << scprintf("Triplet [2]_R12 energy [au]:           %17.12lf",
                                        3.0*energy_pt2r12[AlphaAlpha]) << endl;
  }
  ExEnv::out0() << indent << scprintf("[2]_R12 energy [au]:                   %17.12lf",
                                      energy_correction_r12) << endl;
  ExEnv::out0() << indent << scprintf("Total [2]_R12 energy [au]:             %17.12lf",
                                      energy) << endl;
  set_energy(energy);
}

int sc::PT2R12::nelectron()
{
  return reference_->nelectron();
}

int sc::PT2R12::spin_polarized()
{
  return reference_->spin_polarized();
}

double PT2R12::compute_energy(const RefSCMatrix &hmat,
                              SpinCase2 pairspin,
                              bool print_pair_energies,
                              std::ostream& os) {
  SpinCase1 spin1 = case1(pairspin);
  SpinCase1 spin2 = case2(pairspin);
  Ref<OrbitalSpace> gg1space = r12eval_->ggspace(spin1);
  Ref<OrbitalSpace> gg2space = r12eval_->ggspace(spin2);
  SpinMOPairIter gg_iter(gg1space,gg2space,pairspin);
  double energy = 0.0;

  if (print_pair_energies) {
    os << indent << prepend_spincase(pairspin, "[2]_R12 pair energies:") << endl;
    os << indent << scprintf("    i       j        e (ij)   ") << endl;
    os << indent << scprintf("  -----   -----   ------------") << endl;
  }
  for(gg_iter.start(); int(gg_iter); gg_iter.next()) {
    int i = gg_iter.i();
    int j = gg_iter.j();
    int ij = gg_iter.ij();
    const double e_ij = hmat.get_element(ij,ij);
    if (print_pair_energies) {
      os << indent << scprintf("  %3d     %3d     %12.9lf",
                               i+1,j+1,e_ij) << endl;
    }
    energy+=hmat.get_element(ij,ij);
  }
  if (print_pair_energies)
    os << indent << endl;
  return energy;
}

double sc::PT2R12::energy_cabs_singles(SpinCase1 spin)
{
  # define  printout false

  Ref<OrbitalSpace> activespace = this->r12world()->ref()->occ_act_sb();
  Ref<OrbitalSpace> pspace = rdm1_->orbs(spin);
  Ref<OrbitalSpace> vspace = this->r12world()->ref()->uocc_act_sb(spin);
  Ref<OrbitalSpace> cabsspace = this->r12world()->cabs_space(spin);

  Ref<OrbitalSpaceRegistry> oreg = this->r12world()->world()->tfactory()->orbital_registry();
  if (!oreg->value_exists(pspace))
  {
    oreg->add(make_keyspace_pair(pspace));
  }
  const std::string key = oreg->key(pspace);
  pspace = oreg->value(key);

  Ref<OrbitalSpace> all_virtual_space;
  if (cabs_singles_coupling_)
  {
    all_virtual_space = new OrbitalSpaceUnion("AA", "all virtuals", *vspace, *cabsspace, true);
    if (!oreg->value_exists(all_virtual_space)) oreg->add(make_keyspace_pair(all_virtual_space));
    const std::string AAkey = oreg->key(all_virtual_space);
    all_virtual_space = oreg->value(AAkey);
  }

  Ref<OrbitalSpace> Aspace;
  if (cabs_singles_coupling_)  Aspace = all_virtual_space;
  else                         Aspace = cabsspace;

  const unsigned int num_blocks = vspace->nblocks();// since the two spaces have the same blocksizes, we only need to define one
  const std::vector<unsigned int>& v_block_sizes = vspace->block_sizes();
  const std::vector<unsigned int>& cabs_block_sizes = cabsspace->block_sizes();
  const std::vector<unsigned int>& p_block_sizes = pspace->block_sizes(); // for debugging purposes
  const std::vector<unsigned int>& A_block_sizes = Aspace->block_sizes(); // for debugging purposes


  // get the fock matrices
  RefSCMatrix F_pA = r12eval_->fock(pspace,Aspace,spin);
  RefSCMatrix F_AA = r12eval_->fock(Aspace,Aspace,spin);
  RefSCMatrix F_pp = this->f(spin);
  RefSCMatrix F_pp_otherspin = this->f(other(spin));

  // RDMs
  //RefSymmSCMatrix gamma1 = this->rdm1(spin);
  //RefSymmSCMatrix gamma1_otherspin = this->rdm1(other(spin));
  RefSymmSCMatrix gamma1 = rdm1_->scmat(spin);
  RefSymmSCMatrix gamma1_otherspin = rdm1_->scmat(other(spin));
  RefSymmSCMatrix gamma2_ss = this->rdm2( case12(spin,spin) ); //ss: same spin
  RefSymmSCMatrix gamma2_os = this->rdm2( case12(spin,other(spin)) );//os: opposite spin

  // define H0 and necessary vectors
  const int no = pspace->rank();
  const int nX = Aspace->rank();
  const int nv = vspace->rank();
  const int n_cabs = cabsspace->rank();
  const int noX = no * nX;
  RefSCDimension dim_oX = new SCDimension(noX);
  RefSCDimension dim_o  = new SCDimension(no);
  RefSCDimension dim_X  = new SCDimension(nX);
  RefSymmSCMatrix H0 = gamma2_ss->kit()->symmmatrix(dim_oX); // H0 is the B matrix in our equaiton
  H0.assign(0.0);
  RefSymmSCMatrix Ixy = gamma2_ss->kit()->symmmatrix(dim_o);  // matrix to store intermediate quantities
  Ixy.assign(0.0);
  RefSCVector rhs_vector = gamma2_ss->kit()->vector(dim_oX);
  rhs_vector->assign(0.0);                                   // right hand vector


  if(!rotate_core_) // if forbit rotating core orbitals
  {
    ExEnv::out0()  << indent << "forbid exciting core orbitals" << std::endl << std::endl;
    std::vector<int> map_1_to_2 = map(*activespace, *pspace);
    for (int row_ind = 0; row_ind < no; ++row_ind)
    {
      if(map_1_to_2[row_ind] < 0)  // row_ind is a core-orbital index
      {
//        ExEnv::out0()  << "row_ind, DM: " << row_ind << ", " << gamma1(row_ind, row_ind) << std::endl;
        for (int A1 = 0; A1 < nX; ++A1)
        {
          F_pA.set_element(row_ind, A1, 0.0);
        }
      }
    }
    ExEnv::out0()  << indent << "end eliminating exciting core orbitals" << std::endl << std::endl;
  }

  if (cabs_singles_coupling_) // when using the combined space, the Fock matrix component f^i_a must be zeroed since it belongs to zeroth order Hamiltonian;
   {ExEnv::out0()  << "  zero out the Fock matrix element F^i_a" << endl << endl;
    unsigned int offset1 = 0;
    for (int block_counter1 = 0; block_counter1 < num_blocks; ++block_counter1)
    { for (int v_ind = 0; v_ind < v_block_sizes[block_counter1]; ++v_ind) // in each symmetry block, the OBS virtual indices are put before CABS indices
      { const unsigned int F_pA_v_ind =  offset1 + v_ind;
        for (int row_ind = 0; row_ind < no; ++row_ind)  F_pA.set_element(row_ind, F_pA_v_ind, 0.0);
      }
      offset1 += A_block_sizes[block_counter1];
    }
   }

  // compute the Right-Hand vector
  for(int x=0; x<no; ++x)
  {
    for(int B=0; B<nX; ++B)
    {
      double rhs_vector_xB = 0.0;
      for (int j = 0; j < no; ++j)
      {
        rhs_vector_xB += -1.0 *  gamma1(x, j) * F_pA(j, B);
      }
      rhs_vector->set_element(x * nX + B, rhs_vector_xB);
    }
  }


  //compute trace(f * gamma1) = f^p_q  gamma^q_p; this is an element needed to compute H0
  const double F_Gamma1_product = (F_pp * gamma1).trace() + (F_pp_otherspin * gamma1_otherspin).trace();


  // compute H0; x1/x2 etc denote difference spins; H0 corresponds to the B matrix from the notes

  // first, calculate the intermediate I(x,y) = f^q_p * \gamma^{xp}_{yq};
  for(int x=0; x<no; ++x)
  {
    for(int y=0; y<no; ++y)
    {
      double Ixy_xy = 0.0;
          {
            for (int p = 0; p < no; ++p)
            {
              for (int q = 0; q < no; ++q)
              {                    // firstly contribution from different spin terms; we differentiate alpha and beta spins because for 2-particle gamma of different
                                  // spins, for both upper and lower indices, the alpha-spin index is always before the beta-spin one
                const int x1p2 = (spin == Alpha)? (x * no + p):(p * no + x);
                const int y1q2 = (spin == Alpha)? (y * no + q):(q * no + y);
                Ixy_xy += F_pp_otherspin(q, p) * gamma2_os(x1p2, y1q2);
                if((x != p) && (y != q))  // contribution from gamma2_ss
                {
                  const int upp_ind_same_spin = antisym_pairindex(x,p);
                  const int low_ind_same_spin = antisym_pairindex(y,q);
                  Ixy_xy += indexsizeorder_sign(x,p) * indexsizeorder_sign(y, q) * F_pp(q, p) * gamma2_ss(upp_ind_same_spin, low_ind_same_spin);
                  //  contribution from same spin terms; the "?:" takes care of antisymmetry; for gamma^xp_..., only x<p portion stored
                }
              }
            }
          }
      Ixy(x,y) = Ixy_xy;
    }
  }


  for(int x=0; x<no; ++x)
  {
    for(int y=0; y<no; ++y)
    {
      const double gamma_xy = gamma1(x,y);
      const double Ixy_xy = Ixy(x, y);
      for(int B=0; B<nX; ++B)
      {
        const int xB = x*nX + B;              // calculate the row index
        for(int A=0; A<nX; ++A)
        {
          const int yA = y*nX + A;            //calculate the column index
          double h0_xB_yA = 0.0;
          h0_xB_yA += gamma_xy * F_AA(A,B);
          if(A == B)                          // corresponds to a Kronecker delta
          {
            h0_xB_yA += -1.0 *gamma_xy * F_Gamma1_product + Ixy_xy;
          }
          H0(xB, yA) = h0_xB_yA;
        }
      }
    }
  }


  H0.solve_lin(rhs_vector); // now rhs_vector stores the first-order wavefunction coefficients after solving the equation

#if printout
    RefDiagSCMatrix eigenmatrix_gamma1 = gamma1.eigvals();
    eigenmatrix_gamma1.print("Orbital Occupation Number");

    RefSymmSCMatrix F_pp_copy = gamma2_ss->kit()->symmmatrix(dim_o); //
    F_pp_copy.assign(0.0);
    for (int ind1 = 0; ind1 < no; ++ind1)
    {
      for (int ind2 = 0; ind2 < no; ++ind2)
     {
        F_pp_copy.set_element(ind1, ind2, F_pp.get_element(ind1, ind2));
      }
    }
    RefDiagSCMatrix eigenmatrix_F_pp = F_pp_copy.eigvals();
    eigenmatrix_F_pp.print("pspace Fock matrix eigenvalue");
#endif

#if printout
    RefSymmSCMatrix F_AA_copy = gamma2_ss->kit()->symmmatrix(dim_X);
    F_AA_copy.assign(0.0);
    for (int ind1 = 0; ind1 < nX; ++ind1)
    {
      for (int ind2 = 0; ind2 < nX; ++ind2)
      {
        F_AA_copy.set_element(ind1, ind2, F_AA.get_element(ind1, ind2));
      }
    }
    RefDiagSCMatrix eigenmatrix_F_AA = F_AA_copy.eigvals();
    eigenmatrix_F_AA.print("Aspace Fock matrix eigenvalue");
#endif

#if false
    RefDiagSCMatrix eigenmatrix = H0.eigvals();
    int BeigDimen = int(eigenmatrix.dim());
    for (int i = 0; i < BeigDimen; ++i)
    {
      if(eigenmatrix.get_element(i) < 0) ExEnv::out0() << indent << "negative eigenvalue!" << std::endl;
    }
    eigenmatrix.print("(one-body H0) B eigenvalue");
#endif


  double E_cabs_singles_one_spin = 0.0;
  for (int i = 0; i < no; ++i)
  {
    for (int j = 0; j < no; ++j)
    {
      const double gamma_ij = gamma1(i,j);
      for (int A = 0; A < nX; ++A)
      {
        E_cabs_singles_one_spin += F_pA(i,A) * gamma_ij * rhs_vector->get_element(j * nX + A);
      }
    }
  }

  return E_cabs_singles_one_spin;

}



double sc::PT2R12::energy_cabs_singles_twobody_H0()
{
  # define DEBUGG false

  const SpinCase1 spin = Alpha;
  Ref<OrbitalSpace> activespace = this->r12world()->ref()->occ_act_sb();
  Ref<OrbitalSpace> pspace = rdm1_->orbs(spin);
  Ref<OrbitalSpace> vspace = this->r12world()->ref()->uocc_act_sb(spin);
  Ref<OrbitalSpace> cabsspace = this->r12world()->cabs_space(spin);

  Ref<OrbitalSpaceRegistry> oreg = this->r12world()->world()->tfactory()->orbital_registry();
  if (!oreg->value_exists(pspace)) oreg->add(make_keyspace_pair(pspace));
  const std::string key = oreg->key(pspace);
  pspace = oreg->value(key);

  Ref<OrbitalSpace> all_virtual_space;
  if (cabs_singles_coupling_)
  {
    all_virtual_space = new OrbitalSpaceUnion("AA", "all virtuals", *vspace, *cabsspace, true);
    if (!oreg->value_exists(all_virtual_space)) oreg->add(make_keyspace_pair(all_virtual_space));
    const std::string AAkey = oreg->key(all_virtual_space);
    all_virtual_space = oreg->value(AAkey);
  }

  Ref<OrbitalSpace> orbital_and_cabs_virtual_space;
  Ref<OrbitalSpace> Aspace;
  if (cabs_singles_coupling_)  Aspace = all_virtual_space;
  else                         Aspace = cabsspace;

  const unsigned int num_blocks = vspace->nblocks();// since the two spaces have the same blocksizes, we only need to define one
  const std::vector<unsigned int>& p_block_sizes = pspace->block_sizes(); // for debugging purposes
  const std::vector<unsigned int>& v_block_sizes = vspace->block_sizes();
  const std::vector<unsigned int>& cabs_block_sizes = cabsspace->block_sizes();
  const std::vector<unsigned int>& A_block_sizes = Aspace->block_sizes(); // for debugging purposes

  // get the fock matrices
  RefSCMatrix F_pA_alpha  = r12eval_->fock(pspace,Aspace,spin);
  RefSCMatrix F_pA_beta   = r12eval_->fock(pspace,Aspace,other(spin));
  RefSCMatrix F_AA_alpha  = r12eval_->fock(Aspace,Aspace,spin);
  RefSCMatrix F_AA_beta   = r12eval_->fock(Aspace,Aspace,other(spin)); // for fock operator, the alpha and beta are not necessarily the same
  RefSCMatrix F_pp_alpha  = this->f(spin);
  RefSCMatrix F_pp_beta   = this->f(other(spin));

  // RDMs
  RefSymmSCMatrix gamma1_alpha = rdm1_->scmat(spin);
  RefSymmSCMatrix gamma1_beta = rdm1_->scmat(other(spin));
  RefSymmSCMatrix gamma2_aa = this->rdm2( case12(spin,spin) );
  RefSymmSCMatrix gamma2_bb = this->rdm2( case12(other(spin),other(spin)) );
  RefSymmSCMatrix gamma2_ab = this->rdm2( case12(spin,other(spin)) );

  // define H0 and necessary vectors
  const int no = pspace->rank();
  const int nv = vspace->rank();
  const int nX = Aspace->rank();
  const int n_cabs = cabsspace->rank();
  const int noX = no * nX;
  RefSCDimension dim_oX = new SCDimension(2 * noX);
  RefSCDimension dim_o  = new SCDimension(2 * no);
  RefSCDimension dim_X  = new SCDimension(2 * nX);
  RefSymmSCMatrix B = gamma2_aa->kit()->symmmatrix(dim_oX);
  B.assign(0.0);
  RefSCMatrix Ixy = gamma2_aa->kit()->matrix(dim_o, dim_o);//using a vector does save some space, but not so much
  Ixy.assign(0.0);
  RefSCVector rhs_vector = gamma2_aa->kit()->vector(dim_oX);
  rhs_vector->assign(0.0);


  ExEnv::out0()  << "  primary, virtual and cabs space dimensions: " << no << ", " << nv << ", " << n_cabs << endl;
  ExEnv::out0()  << "  block dimensions of pspace, vspace, cabsspace: " << endl << "  ";
  { for (int block_counter = 0; block_counter < num_blocks; ++block_counter)
      ExEnv::out0() << scprintf("%5d", p_block_sizes[block_counter]);
    ExEnv::out0() << endl << "  ";
    for (int block_counter = 0; block_counter < num_blocks; ++block_counter)
      ExEnv::out0()  << scprintf("%5d", v_block_sizes[block_counter]);
    ExEnv::out0() << endl << "  ";
    for (int block_counter = 0; block_counter < num_blocks; ++block_counter)
      ExEnv::out0()  << scprintf("%5d", cabs_block_sizes[block_counter]);
    ExEnv::out0() << endl;}



  if (cabs_singles_coupling_) // when using the combined space, the Fock matrix component f^i_a must be zeroed since it belongs to zeroth order Hamiltonian;
   {
    ExEnv::out0()  << "  zero out the Fock matrix element F^i_a" << endl << endl;
    unsigned int offset1 = 0;
    for (int block_counter1 = 0; block_counter1 < num_blocks; ++block_counter1)
    {
      for (int v_ind = 0; v_ind < v_block_sizes[block_counter1]; ++v_ind) // in each symmetry block, the OBS virtual indices are put before CABS indices
      { const unsigned int F_pA_v_ind =  offset1 + v_ind;
        for (int row_ind = 0; row_ind < no; ++row_ind)
        { F_pA_alpha.set_element(row_ind, F_pA_v_ind, 0.0);
          F_pA_beta.set_element(row_ind, F_pA_v_ind, 0.0);
        }
      }
      offset1 += A_block_sizes[block_counter1];
    }
   }

  std::vector<int> map_1_to_2 = map(*activespace, *pspace);
  if(!rotate_core_) // if forbit rotating core orbitals
  {
    ExEnv::out0()  << indent << "forbid exciting core orbitals" << std::endl << std::endl;
    for (int row_ind = 0; row_ind < no; ++row_ind)
    {
      if(map_1_to_2[row_ind] < 0)  // row_ind is a core-orbital index
      {
        for (int A1 = 0; A1 < nX; ++A1)
        {
          F_pA_alpha.set_element(row_ind, A1, 0.0);
          F_pA_beta.set_element(row_ind, A1, 0.0);
        }
      }
    }
    ExEnv::out0()  << indent << "end eliminating exciting core orbitals" << std::endl << std::endl;
  }

  // compute the Right-Hand vector
  for(int x1 = 0; x1<no; ++x1)
  {
    for(int B1=0; B1<nX; ++B1)
    {
      double rhs_vector_x1B1_alpha = 0.0;
      double rhs_vector_x1B1_beta = 0.0;
      for (int j1 = 0; j1 < no; ++j1)
      {
        rhs_vector_x1B1_alpha += -1.0 *  gamma1_alpha(x1, j1) * F_pA_alpha(j1, B1);
        rhs_vector_x1B1_beta += -1.0 *  gamma1_beta(x1, j1) * F_pA_beta(j1, B1);
      }
      rhs_vector->set_element(x1 * nX + B1, rhs_vector_x1B1_alpha);
      rhs_vector->set_element(x1 * nX + B1 + noX, rhs_vector_x1B1_beta); // first calculate alpha component, then beta component of RHS vector
    }
  }


  // compute H0; x1/x2 etc denote difference spins; H0 corresponds to the B matrix from the notes

  {                                         // calculate Ixy: the first two term
    int x1, y1, i1,  j1, j2, k1, k2;        //the overall minus sign is taken into account when multiplied by delta to calculate B
    RefSCMatrix   g_pppp_ab   = this->g(case12(spin, other(spin)), pspace, pspace, pspace, pspace);
    for(x1=0; x1<no; ++x1)
    {
        for(y1=0; y1 < no; ++y1)
        {
            double I_aa = 0.0;
            double I_bb = 0.0;
              for (i1 = 0; i1 < no; ++i1) //one contribution to Ixy: f^i1_y1 * gamma^x1_i1
              {
                I_aa += gamma1_alpha(x1, i1) * F_pp_alpha(i1, y1);
                I_bb += gamma1_beta(x1, i1) * F_pp_beta(i1, y1);
              }
            for (i1 = 0; i1 < no; ++i1)    // v^i1j2_y1k2 * (gamma^x1k2_i1j2 - gamma^x1_i1 * gamma^k2_j2)
            {
                for (j2 = 0; j2 < no; ++j2)
                {
                    for (k2 = 0; k2 < no; ++k2)
                    {
                        const double v_i1j2y1k2 = g_pppp_ab(i1* no + j2, y1 * no + k2);
                        const double cumu_x1k2i1j2_aa = gamma2_ab.get_element(x1 * no + k2, i1 * no + j2) - gamma1_alpha.get_element(x1, i1) * gamma1_beta.get_element(k2, j2);
                        const double cumu_x1k2i1j2_bb = gamma2_ab.get_element(k2 * no + x1, j2 * no + i1) - gamma1_beta.get_element(x1, i1) * gamma1_alpha.get_element(k2, j2);
                        I_aa +=  v_i1j2y1k2 * cumu_x1k2i1j2_aa;
                        I_bb +=  v_i1j2y1k2 * cumu_x1k2i1j2_bb;
                    }
                }
            }
            Ixy(x1, y1) =  I_aa;
            Ixy(no+ x1, no + y1) =  I_bb;
        }
    }
  }

  {                                       // calculate Ixy: the last term
      int x1, y1, i1,  j1, j2, k1, k2;
      RefSCMatrix   g_pppp_aa   = this->g(case12(spin, spin), pspace, pspace, pspace, pspace);
      //  RefSCMatrix & g_pppp_bb   = g_pppp_aa;
      for(x1=0; x1<no; ++x1)
      {
          for(y1=0; y1 < no; ++y1)
          {
              double I_aa = 0.0;
              double I_bb = 0.0;
              for (i1 = 0; i1 < no; ++i1)
              {
                  for (j1 = 0; j1 < no; ++j1)
                  {
                      for (k1 = 0; k1 < no; ++k1) // v^i1j1_y1k1 * (0.5 * gamma^x1k1_i1j1 - gamma^x1_i1 * gamma^k1_j1)
                      {
                          if((i1 != j1) && (y1 != k1))
                          {
                            const int g_i1j1y1k1_upp_ind = antisym_pairindex(i1, j1);
                            const int g_i1j1y1k1_low_ind = antisym_pairindex(y1, k1);
                            const double g_i1j1y1k1 = g_pppp_aa.get_element(g_i1j1y1k1_upp_ind, g_i1j1y1k1_low_ind);
                            const int ga2_x1k1i1j1_upp_ind = antisym_pairindex(x1, k1);
                            const int ga2_x1k1i1j1_low_ind = g_i1j1y1k1_upp_ind;
                            double semi_cumu_aa = -1.0 * gamma1_alpha.get_element(x1, i1) * gamma1_alpha.get_element(k1, j1)
                                                  + gamma1_alpha.get_element(x1, j1) * gamma1_alpha.get_element(k1, i1);
                            double semi_cumu_bb = -1.0 * gamma1_beta.get_element(x1, i1)  * gamma1_beta.get_element(k1, j1)
                                                  +gamma1_beta.get_element(x1, j1) * gamma1_beta.get_element(k1, i1);
                            if(x1 != k1)
                            {
                              semi_cumu_aa += indexsizeorder_sign(x1,k1) * indexsizeorder_sign(i1,j1) * gamma2_aa.get_element(ga2_x1k1i1j1_upp_ind, ga2_x1k1i1j1_low_ind);
                              semi_cumu_bb += indexsizeorder_sign(x1,k1) * indexsizeorder_sign(i1,j1) * gamma2_bb.get_element(ga2_x1k1i1j1_upp_ind, ga2_x1k1i1j1_low_ind);
                            }
                            I_aa +=  0.5 * indexsizeorder_sign(i1, j1) * indexsizeorder_sign(y1, k1) * g_i1j1y1k1 * semi_cumu_aa;
                            I_bb +=  0.5 * indexsizeorder_sign(i1, j1) * indexsizeorder_sign(y1, k1) * g_i1j1y1k1 * semi_cumu_bb;
                          }
                      }
                  }
              }
              Ixy(x1, y1) =  Ixy(x1, y1) + I_aa;
              Ixy(no+ x1, no + y1) =   Ixy(no+ x1, no + y1) + I_bb;
          }
      } // finsh calculating Ixy
  }


  // When dealing with unrelaxed core orbitals, Brillouin condition may not be satisfied, and thus we can not assume <| a^core_active H > = 0
  // in this case, our formulas still hold for (x, y) == (active, core);
  // since Ixy should be symmetric, we can calculate Ixy(active, core) for Ixy(core, active)
  // this works whether core orbital are relaxed or not
  for (int row = 0; row < no; ++row)
  {
    for (int col = 0; col < no; ++col) // only the lower (or upper) triangle, otherwise wrong
    {
      if(map_1_to_2[row] < 0 && map_1_to_2[col] > 0) // this means that row <--> core, col<-->active
      {
        Ixy(row, col) = Ixy(col, row);
        Ixy(no + row, no + col) = Ixy(no + col, no + row); // the corresponding beta-beta part
      }
    }
  }


  // explicitly symmetrize Ixy to counteract numerical inaccuracy
  for (int row = 0; row < 2 * no; ++row)
  {
    for (int col = 0; col < row; ++col) // only the lower (or upper) triangle, otherwise wrong
    {
      const double xy = Ixy(row, col);
      const double yx = Ixy(col, row);
      Ixy(row, col) = (xy + yx)/2.0;
      Ixy(col, row) = Ixy(row, col);
    }
  }



// calculate alpha-alpha and beta-beta portion of B: the first three terms
//  f^A1_B1 \gamma^x1_y1 - \delta^A1_B1 * I^x1_y1 + v^A1i2_B1j2 *(gamma^x1j2_y1i2 - \gamma^x1_y1 \gamma^j2_i2)
  {
    RefSCMatrix   g_ApAp_ab   = this->g(case12(spin, other(spin)), Aspace, pspace, Aspace, pspace);
    //  RefSCMatrix & g_pApA_ab   = g_ApAp_ab; // two-electron integrals are the same, as long as the indices are dealt with properly
    unsigned int x1, y1, B1, A1, i2, j2;
    for(x1 = 0; x1<no; ++x1)
    {
      for(y1=0; y1<no; ++y1)
      {
        const double gamma1_x1y1_alpha = gamma1_alpha(x1,y1);
        const double gamma1_x1y1_beta = gamma1_beta(x1, y1);
        const double I_x1y1_alpha = Ixy(x1, y1);
        const double I_x1y1_beta = Ixy(no + x1, no + y1);
        for(B1 = 0; B1 < nX; ++B1)
        {
          const int Baa_row_ind = x1*nX + B1;                            // the row index of  alpha-alpha portion of B matrix
          const int Bbb_row_ind = noX + Baa_row_ind;                     // the row index of  beta-beta portiono of B matrix
          for(A1 = 0; A1 < nX; ++A1)
          {
            double Baa = 0.0;                                            // initialize the B(x1B1, y1A1)
            double Bbb = 0.0;
            const int Baa_col_ind = y1*nX + A1;                          // the column index of alpha-alpha portion of B
            const int Bbb_col_ind = noX + Baa_col_ind;                   // corresponding beta-beta
            Baa += F_AA_alpha(A1, B1) * gamma1_x1y1_alpha;               // f^A1_B1 * gamma^x1_y1; THE MINUS SIGN OF THE INTERMEDIATE IXY IS TAKEN CARE OF HERE
            Bbb += F_AA_beta(A1, B1) * gamma1_x1y1_beta;
            if(A1 == B1)
            {
              Baa += -1.0 * I_x1y1_alpha;                                // -\delta^A1_B1 * I^x1_y1
              Bbb += -1.0 * I_x1y1_beta;
            }
            B(Baa_row_ind, Baa_col_ind) = Baa;
            B(Bbb_row_ind, Bbb_col_ind) = Bbb;
          }
        }
      }
    }
  }


  B.solve_lin(rhs_vector); // now rhs_vector stores the first-order wavefunction coefficients after solving the equation

#if (DEBUGG)
    RefDiagSCMatrix eigenmatrix = B.eigvals();
    int BeigDimen = int(eigenmatrix.dim());
    for (int i = 0; i < BeigDimen; ++i)
    {
      if(eigenmatrix.get_element(i) < 0) ExEnv::out0() << indent << "negative eigenvalue!" << std::endl;
    }
    eigenmatrix.print("(two-body H0) B eigenvalues");
#endif

  double E_cabs_singles = 0.0;
  for (int i = 0; i < no; ++i)
  {
    for (int j = 0; j < no; ++j)
    {
      const double gamma_ij_alpha = gamma1_alpha(i,j);
      const double gamma_ij_beta = gamma1_beta(i,j);
      for (int A = 0; A < nX; ++A)
      {
        E_cabs_singles += F_pA_alpha(i,A) * gamma_ij_alpha * rhs_vector->get_element(j * nX + A);
        E_cabs_singles += F_pA_beta(i,A) * gamma_ij_beta * rhs_vector->get_element(noX + j * nX + A);
      }
    }
  }


  return E_cabs_singles;
}


void PT2R12::brillouin_matrix() {
  RefSCMatrix fmat[NSpinCases1];
  RefSymmSCMatrix opdm[NSpinCases1];
  RefSymmSCMatrix tpcm[NSpinCases2];
  RefSCMatrix g[NSpinCases2];
  Ref<OrbitalSpace> pspace[NSpinCases1];  // all orbitals in OBS
  Ref<OrbitalSpace> mspace[NSpinCases1];  // all occupied orbitals (core + active)
  std::vector<unsigned int> m2p[NSpinCases1];

  Ref<OrbitalSpaceRegistry> oreg = this->r12world()->world()->tfactory()->orbital_registry();

  for(int s=0; s<NSpinCases1; s++) {
    SpinCase1 spin = static_cast<SpinCase1>(s);

    Ref<OrbitalSpace> ospace = rdm1_->orbs(spin);
    if (spin == Alpha) oreg->add(make_keyspace_pair(ospace)); // for some reason this space has not been added

    pspace[s] = this->r12world()->ref()->orbs_sb(spin);
    if (!oreg->value_exists(pspace[s])) {
      oreg->add(make_keyspace_pair(pspace[s]));
    }
    {
      const std::string key = oreg->key(pspace[s]);
      pspace[s] = oreg->value(key);
    }

    mspace[s] = this->r12world()->ref()->occ_sb(spin);
    if (!oreg->value_exists(mspace[s])) {
      oreg->add(make_keyspace_pair(mspace[s]));
    }
    {
      const std::string key = oreg->key(mspace[s]);
      mspace[s] = oreg->value(key);
    }

    m2p[s] = (*pspace[s] << *mspace[s]);

    fmat[s] = r12eval_->fock(pspace[s], pspace[s], spin);
    opdm[s] = rdm1(spin);
  }
  const int nmo = pspace[Alpha]->rank();
  const int nocc = mspace[Alpha]->rank();
  assert(mspace[Alpha]->rank() == mspace[Beta]->rank());
  assert(pspace[Alpha]->rank() == pspace[Beta]->rank());

  for(int i=0; i<NSpinCases2; i++) {
    SpinCase2 S = static_cast<SpinCase2>(i);
    const SpinCase1 spin1 = case1(S);
    const SpinCase1 spin2 = case2(S);
    const Ref<OrbitalSpace>& space1 = rdm2_->orbs(spin1);
    const Ref<OrbitalSpace>& space2 = rdm2_->orbs(spin2);

    tpcm[i] = lambda2(S);
    g[i] = this->g(S, pspace[spin1], pspace[spin2], space1, space2);
  }

  // precompute intermediates
  RefSCMatrix K[NSpinCases1];   // K = < a_p^q F_N > = \eta f \gamma = f \gamma - \gamma f \gamma
  for(int i=0; i<NSpinCases1; i++) {
    K[i] = fmat[i].kit()->matrix(fmat[i].rowdim(), fmat[i].coldim());   // K is a non-symmetric non-block-diagonal matrix
    K[i].assign(0.0);
    for(int y=0; y<nmo; y++) {
      for(int x=0; x<nocc; x++) {
        const int xx = m2p[i][x];
        for(int z=0; z<nocc; z++) {
          const int zz = m2p[i][z];
          K[i].accumulate_element(y,xx,fmat[i].get_element(y, zz) * opdm[i].get_element(z, x));
        }
      }
    }
#if 1
    for(int y=0; y<nocc; y++) {
      const int yy = m2p[i][y];
      for(int x=0; x<nocc; x++) {
        const int xx = m2p[i][x];
        for(int z1=0; z1<nocc; z1++) {
          const int zz1 = m2p[i][z1];
          for(int z2=0; z2<nocc; z2++) {
            const int zz2 = m2p[i][z2];
            K[i].accumulate_element(yy, xx, -opdm[i].get_element(y, z1) *
                                             fmat[i].get_element(zz1, zz2) *
                                             opdm[i].get_element(z2, x));
          }
        }
      }
    }
#endif
  }

  RefSCMatrix M[NSpinCases1];   // M = < a_p^q W_N >
  M[Alpha] = K[Alpha].clone(); M[Alpha].assign(0.0);
  M[Beta] = K[Beta].clone(); M[Beta].assign(0.0);
  for(int P=0; P<nocc; ++P) {
    for(int Q=0; Q<nmo; ++Q) {
      for(int R=0; R<nocc; ++R) {
        const int RR = m2p[Alpha][R];

        int PR_aa, sign_PR=+1;
        if (P > R) {
          PR_aa = P*(P-1)/2 + R;
        }
        else {
          PR_aa = R*(R-1)/2 + P;
          sign_PR = -1;
        }
        const int PR_ab = P * nocc + R;
        const int RP_ab = R * nocc + P;

        int QR_aa, sign_QR=+1;
        if (Q > RR) {
          QR_aa = Q*(Q-1)/2 + RR;
        }
        else {
          QR_aa = RR*(RR-1)/2 + Q;
          sign_QR = -1;
        }
        const int QR_ab = Q * nmo + RR;
        const int RQ_ab = RR * nmo + Q;

        for (int U = 0; U < nocc; ++U) {
          for (int V = 0; V < nocc; ++V) {
            int UV_aa, sign_UV = +1;
            if (U > V) {
              UV_aa = U * (U - 1) / 2 + V;
            } else {
              UV_aa = V * (V - 1) / 2 + U;
              sign_UV = -1;
            }
            const int UV_ab = U * nocc + V;
            const int VU_ab = V * nocc + U;

            double M_QP_a = 0.0;
            double M_QP_b = 0.0;
            if (P != R && Q != RR && U != V) {
              M_QP_a += 0.5 * sign_PR * sign_QR
                  * g[AlphaAlpha].get_element(QR_aa, UV_aa)
                  * tpcm[AlphaAlpha].get_element(UV_aa, PR_aa);
              M_QP_b += 0.5 * sign_PR * sign_QR
                  * g[BetaBeta].get_element(QR_aa, UV_aa)
                  * tpcm[BetaBeta].get_element(UV_aa, PR_aa);
            }
            M_QP_a += 0.5 * g[AlphaBeta].get_element(QR_ab, UV_ab)
                * tpcm[AlphaBeta].get_element(UV_ab, PR_ab);
            M_QP_a += 0.5 * g[AlphaBeta].get_element(QR_ab, VU_ab)
                * tpcm[AlphaBeta].get_element(VU_ab, PR_ab);
            M_QP_b += 0.5 * g[AlphaBeta].get_element(RQ_ab, VU_ab)
                * tpcm[AlphaBeta].get_element(VU_ab, RP_ab);
            M_QP_b += 0.5 * g[AlphaBeta].get_element(RQ_ab, UV_ab)
                * tpcm[AlphaBeta].get_element(UV_ab, RP_ab);
            M[Alpha].accumulate_element(Q, m2p[Alpha][P], M_QP_a);
            M[Beta].accumulate_element(Q, m2p[Beta][P], M_QP_b);
          }
        }
      }
    }
  }

  for(int s=0; s<NSpinCases1; ++s) {
    const SpinCase1 spin = static_cast<SpinCase1>(s);
    K[s].print(prepend_spincase(spin,"K = eta . f . gamma").c_str());
    M[s].print(prepend_spincase(spin,"M = g . lambda").c_str());
    (K[s] + M[s]).print(prepend_spincase(spin,"BC = K + M").c_str());
    fmat[s].print(prepend_spincase(spin,"f").c_str());
  }
}



double
PT2R12::energy_recomputed_from_densities() {
  double twoparticle_energy[NSpinCases2];
  double oneparticle_energy[NSpinCases1];
  const int npurespincases2 = spin_polarized() ? 3 : 2;
  const int npurespincases1 = spin_polarized() ? 2 : 1;

  for(int spincase1=0; spincase1<npurespincases1; spincase1++) {
    const SpinCase1 spin = static_cast<SpinCase1>(spincase1);
    const RefSymmSCMatrix H = compute_obints<&Integral::hcore>(rdm1_->orbs(spin));
    RefSymmSCMatrix opdm = rdm1(spin);
    // H and opdm might use different SCMatrixKits -> copy H into another matrix with the same kit as opdm
    RefSymmSCMatrix hh = opdm.clone();
    hh->convert(H);
    oneparticle_energy[spin] = (hh * opdm)->trace();
  }

  for(int spincase2=0; spincase2<npurespincases2; spincase2++) {
    const SpinCase2 pairspin = static_cast<SpinCase2>(spincase2);
    const SpinCase1 spin1 = case1(pairspin);
    const SpinCase1 spin2 = case2(pairspin);
    const Ref<OrbitalSpace>& space1 = rdm2_->orbs(spin1);
    const Ref<OrbitalSpace>& space2 = rdm2_->orbs(spin2);

    const RefSymmSCMatrix tpdm = rdm2(pairspin);
    const RefSCMatrix G = g(pairspin, space1, space2);

    twoparticle_energy[pairspin] = (G * tpdm).trace();
  }

  if(!spin_polarized()) {
    twoparticle_energy[BetaBeta] = twoparticle_energy[AlphaAlpha];
    oneparticle_energy[Beta] = oneparticle_energy[Alpha];
  }

//#define ENERGY_CONVENTIONAL_PRINT_CONTRIBUTIONS

  double energy_hcore = 0.0;
  for(int i=0; i<NSpinCases1; i++) {
    const SpinCase1 spin = static_cast<SpinCase1>(i);
#ifdef ENERGY_CONVENTIONAL_PRINT_CONTRIBUTIONS
    ExEnv::out0() << ((spin==Alpha) ? "Alpha" : "Beta") << " hcore energy: "
                  << setprecision(12) << oneparticle_energy[i] << endl;
#endif
    energy_hcore += oneparticle_energy[i];
  }
#ifdef ENERGY_CONVENTIONAL_PRINT_CONTRIBUTIONS
  ExEnv::out0() << "hcore energy: " << setprecision(12) << energy_hcore << endl;
#endif
  double energy_twoelec = 0.0;
  for(int i=0; i<NSpinCases2; i++) {
#ifdef ENERGY_CONVENTIONAL_PRINT_CONTRIBUTIONS
    string pairspin_str = "";
    if(i==0) {
      pairspin_str = "AlphaBeta";
    }
    else if(i==1) {
      pairspin_str = "AlphaAlpha";
    }
    else if(i==2) {
      pairspin_str = "BetaBeta";
    }
    ExEnv::out0() << "pairspin " << pairspin_str << " energy: " << setprecision(12) << twoparticle_energy[i] << endl;
#endif
    energy_twoelec += twoparticle_energy[i];
  }
#ifdef ENERGY_CONVENTIONAL_PRINT_CONTRIBUTIONS
  ExEnv::out0() << "two-electron energy: " << setprecision(12) << energy_twoelec << endl;
#endif
  double energy = energy_hcore + energy_twoelec;
  energy += this->reference_->nuclear_repulsion_energy();

  return(energy);
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
