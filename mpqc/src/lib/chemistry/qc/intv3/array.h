//
// array.h
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

#ifdef __GNUC__
#pragma interface
#endif

#ifndef _chemistry_qc_intv3_array_h
#define _chemistry_qc_intv3_array_h

#include <iostream.h>

class IntV3Arraydouble2 {
  private:
    int n1_, n2_;
    double **data_;
  public:
    IntV3Arraydouble2();
    ~IntV3Arraydouble2();
    void set_dim(int n1, int n2);
    double &operator()(int i,int j) { return data_[i][j]; }
    void print(ostream &);
};

class IntV3Arraydouble3 {
  private:
    int n1_, n2_, n3_;
    double ***data_;
  public:
    IntV3Arraydouble3();
    ~IntV3Arraydouble3();
    void set_dim(int n1, int n2, int n3);
    double *operator()(int i,int j) { return data_[i][j]; }
    double &operator()(int i,int j,int k) { return data_[i][j][k]; }
    void print(ostream &);
};

class IntV3Arraydoublep3 {
  private:
    int n1_, n2_, n3_;
    double ****data_;
  public:
    IntV3Arraydoublep3();
    ~IntV3Arraydoublep3();
    void set_dim(int n1, int n2, int n3);
    double *&operator()(int i,int j,int k) { return data_[i][j][k]; }
    void print(ostream &);
};

class IntV3Arraydoublep4 {
  private:
    int n1_, n2_, n3_, n4_;
    double *****data_;
  public:
    IntV3Arraydoublep4();
    ~IntV3Arraydoublep4();
    void set_dim(int n1, int n2, int n3, int n4);
    double *&operator()(int i,int j,int k,int l) { return data_[i][j][k][l]; }
    void print(ostream &);
};

class IntV3Arrayint3 {
  private:
    int n1_, n2_, n3_;
    int ***data_;
  public:
    IntV3Arrayint3();
    ~IntV3Arrayint3();
    void set_dim(int n1, int n2, int n3);
    int &operator()(int i,int j,int k) { return data_[i][j][k]; }
    void print(ostream &);
};

class IntV3Arrayint4 {
  private:
    int n1_, n2_, n3_, n4_;
    int ****data_;
  public:
    IntV3Arrayint4();
    ~IntV3Arrayint4();
    void set_dim(int n1, int n2, int n3, int n4);
    int &operator()(int i,int j,int k,int l) { return data_[i][j][k][l]; }
    void print(ostream &);
};

#endif

// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
