//
// etrain.cc
//
// Copyright (C) 2011 Edward Valeev
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

#include <util/keyval/keyval.h>
#include <util/group/message.h>
#include <util/group/pregtime.h>
#include <util/group/thread.h>
#include <util/group/memory.h>
#include <util/misc/bug.h>
#include <util/misc/formio.h>
#include <util/state/stateio.h>
#include <util/state/state_bin.h>
#include <util/options/GetLongOpt.h>
#include <math/scmat/repl.h>
#include <math/scmat/dist.h>
#include <chemistry/qc/basis/integral.h>
#include <chemistry/qc/wfn/orbital.h>
#include <chemistry/qc/wfn/orbitalspace_utils.h>
#include <chemistry/qc/etrain/etrain.h>
#include <chemistry/qc/lcao/fockbuilder.h>

using namespace sc;

// This object maintains information about class ETraIn.
// NOTE: The last three arguments register default, KeyVal and StateIn constructors,
//       respectively. Hence only the second of the three arguments is not zero
static ClassDesc ETraIn_cd(typeid(ETraIn), "ETraIn", 1,
                                   "public Function",
                                   0, create<ETraIn>, 0);


ETraIn::ETraIn(const Ref<KeyVal>& keyval): Function(keyval)
{
  obwfn12_ << keyval->describedclassvalue("wfn12");
  obwfn1_ << keyval->describedclassvalue("wfn1");
  obwfn2_ << keyval->describedclassvalue("wfn2");
  grid_ << keyval->describedclassvalue("grid");

  //
  // Check wave functions. Must be for closed-shell systems and derived from OneBodyWavefunction
  //
  if (obwfn12_.null())
    throw InputError("wfn12 keyword not specified or has wrong type (must be derived from OneBodyWavefunction)", __FILE__, __LINE__);
  if (obwfn1_.null())
    throw InputError("wfn1 keyword not specified or has wrong type (must be derived from OneBodyWavefunction)", __FILE__, __LINE__);
  if (obwfn2_.null())
    throw InputError("wfn2 keyword not specified or has wrong type (must be derived from OneBodyWavefunction)", __FILE__, __LINE__);
  if (obwfn12_->nelectron()%2)
    throw InputError("wfn12 wave function must be of closed-shell type");
  if (obwfn1_->nelectron()%2)
    throw InputError("wfn1 wave function must be of closed-shell type");
  if (obwfn2_->nelectron()%2)
    throw InputError("wfn2 wave function must be of closed-shell type");

  // Must use canonical orthogonalization method since symmetric does not drop redundant combinations
  if (obwfn12_->orthog_method() != OverlapOrthog::Canonical ||
      obwfn1_->orthog_method() != OverlapOrthog::Canonical ||
      obwfn2_->orthog_method() != OverlapOrthog::Canonical) {
    throw InputError("all Wavefunctions must use canonical orthogonalization method",__FILE__,__LINE__);
  }

  // nocc_ and nuocc_ default to -1, which means to use all monomer orbitals
  nocc_  = keyval->intvalue("nocc",KeyValValueint(-1));
  nuocc_  = keyval->intvalue("nuocc",KeyValValueint(-1));
  debug_  = keyval->intvalue("debug",KeyValValueint(0));

  // check that monomer atoms are subsets of the n-mer and construct atom maps
  // since Molecule does not rotate atoms, the difference in coordinates can be only due to different
  // origins
  Ref<Molecule> mol1 = obwfn1_->molecule();
  Ref<Molecule> mol2 = obwfn2_->molecule();
  Ref<Molecule> mol12 = obwfn12_->molecule();
  // monomer 1
  {
    SCVector3 shift1 = mol12->ref_origin() - mol1->ref_origin();
    const int natom1 = mol1->natom();
    atom_map1_.resize(natom1);
    for (int a1 = 0; a1 < natom1; ++a1) {
      double r[3];
      for (int xyz = 0; xyz < 3; ++xyz)
        r[xyz] = mol1->r(a1, xyz) + shift1[xyz];
      int a12 = mol12->atom_at_position(r, 1e-4);
      if (a12 < 0) {
        mol1->print();
        mol12->print();
        std::ostringstream oss;
        oss << "atom " << a1 << " in monomer 1 not found in the n-mer";
        throw InputError(oss.str().c_str(), __FILE__, __LINE__);
      }
      atom_map1_[a1] = a12;
    }
  }
  // monomer 2
  {
    SCVector3 shift2 = mol12->ref_origin() - mol2->ref_origin();
    const int natom2 = mol2->natom();
    atom_map2_.resize(natom2);
    for (int a2 = 0; a2 < natom2; ++a2) {
      double r[3];
      for (int xyz = 0; xyz < 3; ++xyz)
        r[xyz] = mol2->r(a2, xyz) + shift2[xyz];
      int a12 = mol12->atom_at_position(r, 1e-4);
      if (a12 < 0) {
        mol2->print();
        mol12->print();
        std::ostringstream oss;
        oss << "atom " << a2 << " in monomer 2 not found in the n-mer";
        throw InputError(oss.str().c_str(), __FILE__, __LINE__);
      }
      atom_map2_[a2] = a12;
    }
  }
}

void
ETraIn::obsolete() {
  Function::obsolete();
  obwfn12_->obsolete();
  obwfn1_->obsolete();
  obwfn2_->obsolete();
}

void
ETraIn::run(void) {
  this->compute();
}

void
ETraIn::compute(void)
{
  if(gradient_needed()) {
    ExEnv::out0() << "No gradients can be provided for this object" << std::endl;
    abort();
  }

  obwfn12_->set_desired_value_accuracy(desired_value_accuracy());
  obwfn1_->set_desired_value_accuracy(desired_value_accuracy());
  obwfn2_->set_desired_value_accuracy(desired_value_accuracy());
  double energy12 = obwfn12_->energy();
  double energy1 = obwfn1_->energy();
  double energy2 = obwfn2_->energy();
  compute_train();

  set_value(0.0);
  set_actual_value_accuracy(0.0);
}

void
ETraIn::compute_train()
{
  // Effective Hailtonian for SCF and other (ExtendedHuckel) wave functions is recovered via different members
  // thus need to determine which wave function we have
  Ref<CLSCF> obwfn12_clscf; obwfn12_clscf << obwfn12_;

  if (! obwfn12_->integral()->equiv(obwfn1_->integral()) ||
      ! obwfn12_->integral()->equiv(obwfn2_->integral()) )
    throw InputError("Integral factories must match for all calculations",__FILE__,__LINE__);
  const Ref<Integral>& integral = obwfn12_->integral();

  RefSCMatrix vec12_so = obwfn12_->eigenvectors();
  RefSCMatrix vec1_so = obwfn1_->eigenvectors();
  RefSCMatrix vec2_so = obwfn2_->eigenvectors();
  Ref<PetiteList> plist12 = obwfn12_->integral()->petite_list();
  Ref<PetiteList> plist1 = obwfn1_->integral()->petite_list();
  Ref<PetiteList> plist2 = obwfn2_->integral()->petite_list();
  RefSCMatrix vec12 = plist12->evecs_to_AO_basis(vec12_so);
  RefSCMatrix vec1 = plist1->evecs_to_AO_basis(vec1_so);
  RefSCMatrix vec2 = plist2->evecs_to_AO_basis(vec2_so);

  // map vec1 and vec2 to the n-mer basis. Because Molecule does not rotate the frame, as long as
  // monomers are given in the same frame as the n-mer, there is no need to rotate the basis functions.
  // we thank ye, MPQC gods!
  //
  // because frames may be shifted, however, can't just say bs12 << bs1
  Ref<GaussianBasisSet> bs1 = obwfn1_->basis();
  Ref<GaussianBasisSet> bs2 = obwfn2_->basis();
  Ref<GaussianBasisSet> bs12 = obwfn12_->basis();
  std::vector<unsigned int> basis_map1(bs1->nbasis());  // maps basis functions of bs1 to bs12
  std::vector<unsigned int> basis_map2(bs2->nbasis());  // etc.
  {  // map bs1 to bs12
    const int natom = atom_map1_.size();
    for(int a=0; a<natom; ++a) {
      const int a12 = atom_map1_[a];

      const int nshell = bs1->nshell_on_center(a);
      for(int s=0; s<nshell; ++s) {
        const int S = bs1->shell_on_center(a, s);
        int s12;
        try {
          s12 = ishell_on_center(a12, bs12, bs1->shell(S));
          const int nbf = bs1->shell(S).nfunction();
          const int offset = bs1->shell_to_function(S);
          const int offset12 = bs12->shell_to_function(s12);
          for(int f=0; f<nbf; ++f)
            basis_map1[f + offset] = f + offset12;
        }
        catch (ProgrammingError&) {
          std::ostringstream oss; oss << "shell " << S << " in basis set of scf1 is not found in that of scf12";
          throw InputError(oss.str().c_str(),
                           __FILE__,__LINE__);
        }
      }
    }
  }
  {  // map bs2 to bs12
    const int natom = atom_map2_.size();
    for(int a=0; a<natom; ++a) {
      const int a12 = atom_map2_[a];

      const int nshell = bs2->nshell_on_center(a);
      for(int s=0; s<nshell; ++s) {
        const int S = bs2->shell_on_center(a, s);
        int s12;
        try {
          s12 = ishell_on_center(a12, bs12, bs2->shell(S));
          const int nbf = bs2->shell(S).nfunction();
          const int offset = bs2->shell_to_function(S);
          const int offset12 = bs12->shell_to_function(s12);
          for(int f=0; f<nbf; ++f)
            basis_map2[f + offset] = f + offset12;
        }
        catch (ProgrammingError&) {
          std::ostringstream oss; oss << "shell " << S << " in basis set of scf2 is not found in that of scf12";
          throw InputError(oss.str().c_str(),
                           __FILE__,__LINE__);
        }
      }
    }
  }
  // use basis function maps to map coefficients to the n-mer basis
  RefSCMatrix vec1_12 = vec1.kit()->matrix(vec12.rowdim(), vec1.coldim());  vec1_12.assign(0.0);
  RefSCMatrix vec2_12 = vec2.kit()->matrix(vec12.rowdim(), vec2.coldim());  vec2_12.assign(0.0);
  { // vec1 -> vec1_12
    const int nbasis = vec1.rowdim().n();
    const int nmo = vec1.coldim().n();
    for(int f=0; f<nbasis; ++f) {
      const int f12 = basis_map1[f];
      for(int mo=0; mo<nmo; ++mo)
        vec1_12.set_element(f12, mo, vec1.get_element(f, mo));
    }
  }
  { // vec2 -> vec2_12
    const int nbasis = vec2.rowdim().n();
    const int nmo = vec2.coldim().n();
    for(int f=0; f<nbasis; ++f) {
      const int f12 = basis_map2[f];
      for(int mo=0; mo<nmo; ++mo)
        vec2_12.set_element(f12, mo, vec2.get_element(f, mo));
    }
  }
  vec1 = 0;
  vec2 = 0;

  // Compute how many monomer orbitals to consider for the transfer matrices
  int  nomo_omit1, nomo_omit2;
  int  numo_omit1, numo_omit2;
  const int nocc1 = obwfn1_->nelectron()/2;
  const int nocc2 = obwfn2_->nelectron()/2;
  if (nocc_ == -1) {
    nomo_omit1 = 0;
    nomo_omit2 = 0;
  }
  else {
    nomo_omit1 = nocc1 - nocc_;
    nomo_omit2 = nocc2 - nocc_;
  }
  if (nuocc_ == -1) {
    numo_omit1 = 0;
    numo_omit2 = 0;
  }
  else {
    numo_omit1 = vec1_12.coldim().n() - nocc1 - nuocc_;
    numo_omit2 = vec2_12.coldim().n() - nocc2 - nuocc_;
  }
  if (nomo_omit1 < 0) nomo_omit1 = 0;
  if (nomo_omit2 < 0) nomo_omit2 = 0;
  if (numo_omit1 < 0) numo_omit1 = 0;
  if (numo_omit2 < 0) numo_omit2 = 0;
  // select only the requested HOMOs and LUMOs
  Ref<OrbitalSpace> dspace =  new OrbitalSpace("D", "n-mer basis set space", vec12, obwfn12_->basis(), obwfn12_->integral(), obwfn12_->eigenvalues(), 0, 0);
  Ref<OrbitalSpace> m1space = new OrbitalSpace("m1", "Monomer 1 active MO space", vec1_12, obwfn12_->basis(), obwfn1_->integral(), obwfn1_->eigenvalues(), nomo_omit1, numo_omit1);
  Ref<OrbitalSpace> m2space = new OrbitalSpace("m2", "Monomer 2 active MO space", vec2_12, obwfn12_->basis(), obwfn2_->integral(), obwfn2_->eigenvalues(), nomo_omit2, numo_omit2);
  vec1_12 = 0; vec2_12 = 0;

  if (debug_ > 0) {
    RefSCMatrix S11 = compute_overlap_ints(m1space,m1space);
    S11.print("Original S11 matrix");
    Ref<OrbitalSpace> proj11 = gen_project(m1space,m1space,"m1->m1", "Monomer 1 MO space projected on itself",1e-10);
    S11 = compute_overlap_ints(m1space,proj11);
    S11.print("S11 matrix after projection");
  }

  if (debug_ > 0) {
    RefSCMatrix S22 = compute_overlap_ints(m2space,m2space);
    S22.print("Original S22 matrix");
    Ref<OrbitalSpace> proj22 = gen_project(m2space,m2space,"m2->m2", "Monomer 2 MO space projected on itself",1e-10);
    S22 = compute_overlap_ints(m2space,proj22);
    S22.print("S22 matrix after projection");
  }

  // Get monomer MOs projected on the n-mer basis (M1d and M2d)
#if 0
  Ref<OrbitalSpace> proj1 = gen_project(m1space,dspace,"m1->D", "m1->D space",1e-10);
  Ref<OrbitalSpace> proj2 = gen_project(m2space,dspace,"m2->D", "m2->D space",1e-10);
  RefSCMatrix C1projT = proj1->coefs().t();
  RefSCMatrix C2proj = proj2->coefs();
#else
  RefSCMatrix C1projT = m1space->coefs().t();
  RefSCMatrix C2proj  = m2space->coefs();
#endif

  // Compute the n-mer Fock matrix in SO basis -- must transform from MO basis to SO basis
  RefSymmSCMatrix fock12_so(obwfn12_->so_dimension(), obwfn12_->basis_matrixkit());
  fock12_so.assign(0.0);
  RefSCMatrix sobymo = obwfn12_->mo_to_so();
  // SCF Hamiltonian is obtained with effective_fock()
#if 1
  if (obwfn12_clscf.nonnull()) {
    fock12_so.accumulate_transform(obwfn12_->mo_to_so(), obwfn12_clscf->effective_fock());
  }
  // Other (incl. Huckel) Hamiltonian are assumed to be simply the eigenvalues
  else {
#else
  {
#endif
    RefSymmSCMatrix fock12_mo(obwfn12_->oso_dimension(), obwfn12_->basis_matrixkit());
    const int nmo = obwfn12_->oso_dimension().n();
    RefDiagSCMatrix fock12_evals = obwfn12_->eigenvalues();
    fock12_mo.assign(0.0);
    for(int i=0; i<nmo; i++)
      fock12_mo.set_element(i,i,fock12_evals(i));
    fock12_so.accumulate_transform(obwfn12_->mo_to_so(), fock12_mo);
  }

  // now transform the n-mer Fock matrix to AO basis
  RefSymmSCMatrix fock12_ao = plist12->to_AO_basis(fock12_so);

  // Compute n-mer Fock matrix between M1d and M2d
  RefSCMatrix F12 = C1projT * fock12_ao * C2proj;
#if 0 // need to look at other one-electron operators?
  F12 = C1projT * plist12->to_AO_basis(sc::detail::onebodyint<&Integral::hcore>(m1space->basis(), Integral::get_default_integral())) * C2proj;
#endif
  F12.print("Transfer Fock matrix");

  // Compute overlap matrix between M1d and M2d
#if 0
  RefSCMatrix S12 = compute_overlap_ints(proj1,proj2);
#else
  RefSCMatrix S12 = compute_overlap_ints(m1space, m2space);
#endif
  S12.print("Overlap matrix");

  // print out monomer MO energies
  m1space->evals().print("Monomer 1 orbital energies");
  m2space->evals().print("Monomer 2 orbital energies");
  dspace->evals().print("n-mer orbital energies");

  // Compute n-mer Fock matrix in monomer MO bases
  RefSCMatrix F11 = C1projT * fock12_ao * C1projT.t();
  RefSCMatrix F22 = C2proj.t() * fock12_ao * C2proj;
  F11.print("n-mer Fock matrix in monomer 1 basis");
  F22.print("n-mer Fock matrix in monomer 2 basis");

  // print out overlap between monomer and n-mer orbitals to help map monomer HOMOs/LUMOs to the n-mer orbitals
  if (debug_ > 0) {
    RefSCMatrix S1D = compute_overlap_ints(m1space,dspace);  S1D.print("Overlap between monomer 1 and n-mer orbitals");
    RefSCMatrix S2D = compute_overlap_ints(m2space,dspace);  S2D.print("Overlap between monomer 1 and n-mer orbitals");
  }

  // optional: print orbitals on the grid
  if (grid_.nonnull()) {
    // merge the two spaces together
    Ref<OrbitalSpace> m12space = new OrbitalSpaceUnion("m1+m2", "Monomers 1+2 active MO space",
                                                       *m1space, *m2space, true);
    // create labels for mos: fragment 1 will be negative, fragment 2 positive
    std::vector<int> labels;
    for(int o=0; o<m1space->rank(); ++o)
      labels.push_back(- (o + nomo_omit1 + 1));
    for(int o=0; o<m2space->rank(); ++o)
      labels.push_back(  (o + nomo_omit2 + 1));
    Ref<WriteOrbitals> wrtorbs = new WriteOrbitals(m12space, labels,
                                                   grid_,
                                                   std::string("gaussian_cube"),
                                                   std::string("mo.cube"));
    wrtorbs->run();
  }

  obwfn12_->print();
  obwfn1_->print();
  obwfn2_->print();
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End: