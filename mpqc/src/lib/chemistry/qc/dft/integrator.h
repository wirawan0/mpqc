//
// integrator.h --- definition of the electron density integrator
//
// Copyright (C) 1997 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
// Maintainer: LPS
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

#ifndef _chemistry_qc_dft_integrator_h
#define _chemistry_qc_dft_integrator_h

#ifdef __GNUC__
#pragma interface
#endif

#include <util/state/state.h>
#include <chemistry/qc/dft/functional.h>
#include <chemistry/qc/basis/extent.h>

/** An abstract base class for integrating the electron density. */
class DenIntegrator: virtual public SavableState {
#   define CLASSNAME DenIntegrator
#   include <util/state/stated.h>
#   include <util/class/classda.h>
  protected:
    RefWavefunction wfn_;
    RefShellExtent extent_;

    double value_;
    double accuracy_;

    int spin_polarized_;

    int ncontrib_;
    int *contrib_;
    int ncontrib_bf_;
    int *contrib_bf_;
    double *bs_values_;
    double *bsg_values_;
    double *bsh_values_;
    double *alpha_dmat_;
    double *beta_dmat_;
    double *dmat_bound_;
    double *alpha_vmat_; // lower triangle of xi_i(r) v(r) xi_j(r) integrals
    double *beta_vmat_; // lower triangle of xi_i(r) v(r) xi_j(r) integrals
    int need_density_; // specialization must set to 1 if it needs density_
    double density_;
    int nbasis_;
    int nshell_;
    int natom_;
    int compute_potential_integrals_; // 1 if potential integrals are needed

    int need_gradient_;
    int need_hessian_;

    int linear_scaling_;
    int use_dmat_bound_;

    void get_density(double *dmat, PointInputData::SpinData &d);
    void init_integration(const RefDenFunctional &func,
                          const RefSymmSCMatrix& densa,
                          const RefSymmSCMatrix& densb,
                          double *nuclear_gradient);
    void done_integration();
    double do_point(int acenter, const SCVector3 &r, const RefDenFunctional &,
                    double weight, double multiplier, double *nuclear_gradient,
                    double *f_gradient, double *w_gradient);
  public:
    /// Construct a new DenIntegrator.
    DenIntegrator();
    /// Construct a new DenIntegrator given the KeyVal input.
    DenIntegrator(const RefKeyVal &);
    /// Construct a new DenIntegrator given the StateIn data.
    DenIntegrator(StateIn &);
    ~DenIntegrator();
    void save_data_state(StateOut &);

    /// Returns the wavefunction used for the integration.
    RefWavefunction wavefunction() const { return wfn_; }
    /// Returns the result of the integration.
    double value() const { return value_; }

    /// Sets the accuracy to use in the integration.
    void set_accuracy(double a) { accuracy_ = a; }

    /** Call with non zero if the potential integrals are to be computed.
        They can be returned with the vmat() member. */
    void set_compute_potential_integrals(int);
    /** Returns the alpha potential integrals. Stored as
        the lower triangular, row-major format. */
    const double *alpha_vmat() const { return alpha_vmat_; }
    /** Returns the beta potential integrals. Stored as
        the lower triangular, row-major format. */
    const double *beta_vmat() const { return beta_vmat_; }

    /** Called before integrate.  Does not need to be called again
        unless the geometry changes or done is called. */
    virtual void init(const RefWavefunction &);
    /// Must be called between calls to init.
    virtual void done();
    /** Performs the integration of the given functional using the given
        alpha and beta density matrices.  The nuclear derivative
        contribution is placed in nuclear_grad, if it is non-null. */
    virtual void integrate(const RefDenFunctional &,
                           const RefSymmSCMatrix& densa =0,
                           const RefSymmSCMatrix& densb =0,
                           double *nuclear_grad = 0) = 0;
};
SavableState_REF_dec(DenIntegrator);

/** An abstract base class for computing grid weights. */
class IntegrationWeight: virtual public SavableState {
#   define CLASSNAME IntegrationWeight
#   include <util/state/stated.h>
#   include <util/class/classda.h>

  protected:

    RefMolecule mol_;
    double tol_;

    void fd_w(int icenter, SCVector3 &point, double *fd_grad_w);

  public:
    IntegrationWeight();
    IntegrationWeight(const RefKeyVal &);
    IntegrationWeight(StateIn &);
    ~IntegrationWeight();
    void save_data_state(StateOut &);

    void test(int icenter, SCVector3 &point);
    void test();

    /// Initialize the integration weight object.
    virtual void init(const RefMolecule &, double tolerance);
    /// Called when finished with the integration weight object.
    virtual void done();
    /** Computes the weight for a given center at a given point in space.
        Derivatives of the weigth with respect to nuclear coordinates are
        optionally returned in grad_w.  This must be called after init, but
        before done. */
    virtual double w(int center, SCVector3 &point, double *grad_w = 0) = 0;
};
SavableState_REF_dec(IntegrationWeight);

/** Implements Becke's integration weight scheme. */
class BeckeIntegrationWeight: public IntegrationWeight {
#   define CLASSNAME BeckeIntegrationWeight
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>

    int ncenters;
    SCVector3 *centers;
    double *bragg_radius;

    double **a_mat;
    double **oorab;

    void compute_grad_p(int gc, int ic, int wc, SCVector3&r, double p,
                           SCVector3&g);
    void compute_grad_nu(int gc, int bc, SCVector3 &point, SCVector3 &grad);

    double compute_t(int ic, int jc, SCVector3 &point);
    double compute_p(int icenter, SCVector3&point);

  public:
    BeckeIntegrationWeight();
    BeckeIntegrationWeight(const RefKeyVal &);
    BeckeIntegrationWeight(StateIn &);
    ~BeckeIntegrationWeight();
    void save_data_state(StateOut &);

    void init(const RefMolecule &, double tolerance);
    void done();
    double w(int center, SCVector3 &point, double *grad_w = 0);
};

/** An abstract base class for radial integrators. */
class RadialIntegrator: virtual public SavableState {
#   define CLASSNAME RadialIntegrator
#   include <util/state/stated.h>
#   include <util/class/classda.h>
  protected:
    int nr_;
  public:
    RadialIntegrator();
    RadialIntegrator(const RefKeyVal &);
    RadialIntegrator(StateIn &);
    ~RadialIntegrator();
    void save_data_state(StateOut &);

    void set_nr(int i);
    int get_nr(void) const;
    virtual double radial_value(int ir, int nr, double radii) = 0;
    virtual double radial_multiplier(int nr) = 0;
    virtual double get_dr_dq(void) const = 0;
    virtual double get_dr_dqr2(void) const = 0;
    virtual void set_dr_dq(double i) = 0;
    virtual void set_dr_dqr2(double i) = 0;
    void print(ostream & =cout) const;
};
SavableState_REF_dec(RadialIntegrator);

/** An abstract base class for angular integrators. */
class AngularIntegrator: virtual public SavableState{
#   define CLASSNAME AngularIntegrator
#   include <util/state/stated.h>
#   include <util/class/classda.h>
  protected:
  public:
    AngularIntegrator();
    AngularIntegrator(const RefKeyVal &);
    AngularIntegrator(StateIn &);
    ~AngularIntegrator();
    void save_data_state(StateOut &);

    virtual int num_angular_points(double r_value, int ir) = 0;
    virtual double angular_point_cartesian(int iangular, double r,
        SCVector3 &integration_point) const = 0;
    virtual void print(ostream & =cout) const = 0;
};
SavableState_REF_dec(AngularIntegrator);

/** An implementation of a radial integrator using the Euler-Maclaurin
    weights and grid points. */
class EulerMaclaurinRadialIntegrator: public RadialIntegrator {
#   define CLASSNAME EulerMaclaurinRadialIntegrator
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    double dr_dq_;
    double dr_dqr2_;
  public:
    EulerMaclaurinRadialIntegrator();
    EulerMaclaurinRadialIntegrator(const RefKeyVal &);
    EulerMaclaurinRadialIntegrator(StateIn &);
    ~EulerMaclaurinRadialIntegrator();
    void save_data_state(StateOut &);

    double radial_value(int ir, int nr, double radii);
    double radial_multiplier(int nr);
    double get_dr_dq(void) const;
    void set_dr_dq(double i);
    double get_dr_dqr2(void) const;
    void set_dr_dqr2(double i);
};

/** An implementation of a Lebedev angular integrator.  It uses code
    written by Dr. Dmitri N. Laikov.

    This can generate grids with the following numbers of points:
       6,  14,  26,  38,  50,  74,  86, 110, 146, 170, 194, 230, 266, 302,
     350, 386, 434, 482, 530, 590, 650, 698, 770, 830, 890, 974,1046,1118,1202,
    1274,1358,1454,1538,1622,1730,1814,1910,2030,2126,2222,2354,2450,2558,2702,
    2810,2930,3074,3182,3314,3470,3590,3722,3890,4010,4154,4334,4466,4610,4802,
    4934,5090,5294,5438,5606, and 5810.

    V.I. Lebedev, and D.N. Laikov
    "A quadrature formula for the sphere of the 131st
     algebraic order of accuracy"
    Doklady Mathematics, Vol. 59, No. 3, 1999, pp. 477-481.
   
    V.I. Lebedev
    "A quadrature formula for the sphere of 59th algebraic
     order of accuracy"
    Russian Acad. Sci. Dokl. Math., Vol. 50, 1995, pp. 283-286.
   
    V.I. Lebedev, and A.L. Skorokhodov
    "Quadrature formulas of orders 41, 47, and 53 for the sphere"
    Russian Acad. Sci. Dokl. Math., Vol. 45, 1992, pp. 587-592.
   
    V.I. Lebedev
    "Spherical quadrature formulas exact to orders 25-29"
    Siberian Mathematical Journal, Vol. 18, 1977, pp. 99-107.
   
    V.I. Lebedev
    "Quadratures on a sphere"
    Computational Mathematics and Mathematical Physics, Vol. 16,
    1976, pp. 10-24.
   
    V.I. Lebedev
    "Values of the nodes and weights of ninth to seventeenth
     order Gauss-Markov quadrature formulae invariant under the
     octahedron group with inversion"
    Computational Mathematics and Mathematical Physics, Vol. 15,
       1975, pp. 44-51.

 */
class LebedevLaikovIntegrator: public AngularIntegrator {
#   define CLASSNAME LebedevLaikovIntegrator
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    int npoints_;
    double *x_, *y_, *z_, *w_;
    
    void init(int n);
  public:
    LebedevLaikovIntegrator();
    LebedevLaikovIntegrator(const RefKeyVal &);
    LebedevLaikovIntegrator(StateIn &);
    ~LebedevLaikovIntegrator();
    void save_data_state(StateOut &);

    int num_angular_points(double r_value, int ir);
    double angular_point_cartesian(int iangular, double r,
                                   SCVector3 &integration_point) const;
    void print(ostream & =cout) const;
};

/** An implementation of an angular integrator using the Gauss-Legendre
    weights and grid points. */
class GaussLegendreAngularIntegrator: public AngularIntegrator {
#   define CLASSNAME GaussLegendreAngularIntegrator
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    int ntheta_;
    int nphi_;
    int Ktheta_;
    int ntheta_r_;
    int nphi_r_;
    int Ktheta_r_;
    double *theta_quad_weights_;
    double *theta_quad_points_;
  public:
    GaussLegendreAngularIntegrator();
    GaussLegendreAngularIntegrator(const RefKeyVal &);
    GaussLegendreAngularIntegrator(StateIn &);
    ~GaussLegendreAngularIntegrator();
    void save_data_state(StateOut &);
    
    int get_ntheta(void) const;
    void set_ntheta(int i);
    int get_nphi(void) const;
    void set_nphi(int i);
    int get_Ktheta(void) const;
    void set_Ktheta(int i);
    int get_ntheta_r(void) const;
    void set_ntheta_r(int i);
    int get_nphi_r(void) const;
    void set_nphi_r(int i);
    int get_Ktheta_r(void) const;
    void set_Ktheta_r(int i);
    int num_angular_points(double r_value, int ir);
    double angular_point_cartesian(int iangular, double r,
        SCVector3 &integration_point) const;
    double sin_theta(SCVector3 &point) const;
    void gauleg(double x1, double x2, int n);    
    void print(ostream & =cout) const;
};

/** An implementation of an integrator using any combination of
    a RadianIntegrator and an AngularIntegrator. */
class RadialAngularIntegrator: public DenIntegrator {
#   define CLASSNAME RadialAngularIntegrator
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    RefRadialIntegrator RadInt_;
    RefAngularIntegrator AngInt_;
    RefIntegrationWeight weight_;
  public:
    RadialAngularIntegrator();
    RadialAngularIntegrator(const RefKeyVal &);
    RadialAngularIntegrator(StateIn &);
    ~RadialAngularIntegrator();
    void save_data_state(StateOut &);

    void integrate(const RefDenFunctional &,
                   const RefSymmSCMatrix& densa =0,
                   const RefSymmSCMatrix& densb =0,
                   double *nuclear_gradient = 0);

    void print(ostream & =cout) const;
};
    
/** An implementation of an integrator based on C.W. Murray, et
    al. Mol. Phys. 78, No. 4, 997-1014, 1993. */
class Murray93Integrator: public DenIntegrator {
#   define CLASSNAME Murray93Integrator
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    int nr_;
    int ntheta_;
    int nphi_;
    int Ktheta_;

    RefIntegrationWeight weight_;
    
  public:
    Murray93Integrator();
    Murray93Integrator(const RefKeyVal &);
    Murray93Integrator(StateIn &);
    ~Murray93Integrator();
    void save_data_state(StateOut &);

    void integrate(const RefDenFunctional &,
                   const RefSymmSCMatrix& densa =0,
                   const RefSymmSCMatrix& densb =0,
                   double *nuclear_gradient = 0);

    void print(ostream & =cout) const;
};

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
