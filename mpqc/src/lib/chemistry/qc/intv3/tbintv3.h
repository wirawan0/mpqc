//
// tbintv3.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@ca.sandia.gov>
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

#ifndef _chemistry_qc_intv3_tbintv3_h
#define _chemistry_qc_intv3_tbintv3_h

#include <chemistry/qc/basis/tbint.h>
#include <chemistry/qc/intv3/int2e.h>

class TwoBodyIntV3 : public TwoBodyInt {
  protected:
    RefInt2eV3 int2ev3_;

  public:
    TwoBodyIntV3(const RefGaussianBasisSet&b1,
                 const RefGaussianBasisSet&b2,
                 const RefGaussianBasisSet&b3,
                 const RefGaussianBasisSet&b4,
                 int storage);
    ~TwoBodyIntV3();

    int log2_shell_bound(int,int,int,int);
    void compute_shell(int,int,int,int);
};

class TwoBodyDerivIntV3 : public TwoBodyDerivInt {
  protected:
    RefInt2eV3 int2ev3_;

  public:
    TwoBodyDerivIntV3(const RefGaussianBasisSet&b1,
                      const RefGaussianBasisSet&b2,
                      const RefGaussianBasisSet&b3,
                      const RefGaussianBasisSet&b4,
                      int storage);
    ~TwoBodyDerivIntV3();

    int log2_shell_bound(int,int,int,int);
    void compute_shell(int,int,int,int,DerivCenters&);
};

#endif

// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
