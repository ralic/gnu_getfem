/* -*- c++ -*- (enables emacs c++ mode)                                    */
/* *********************************************************************** */
/*                                                                         */
/* Library :  Generic Matrix Methods  (gmm)                                */
/* File    :  gmm_tri_solve.h : from M.T.L.                                */
/*     									   */
/* Date : October 13, 2002.                                                */
/* Author : Yves Renard, Yves.Renard@gmm.insa-tlse.fr                      */
/*                                                                         */
/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2002  Yves Renard.                                        */
/*                                                                         */
/* This file is a part of GETFEM++                                         */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify    */
/* it under the terms of the GNU Lesser General Public License as          */
/* published by the Free Software Foundation; version 2.1 of the License.  */
/*                                                                         */
/* This program is distributed in the hope that it will be useful,         */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/* GNU Lesser General Public License for more details.                     */
/*                                                                         */
/* You should have received a copy of the GNU Lesser General Public        */
/* License along with this program; if not, write to the Free Software     */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,  */
/* USA.                                                                    */
/*                                                                         */
/* *********************************************************************** */
//
// Copyright 1997, 1998, 1999 University of Notre Dame.
// Authors: Andrew Lumsdaine, Jeremy G. Siek, Lie-Quan Lee
//
// This file is part of the Matrix Template Library
//
// You should have received a copy of the License Agreement for the
// Matrix Template Library along with the software;  see the
// file LICENSE.  If not, contact Office of Research, University of Notre
// Dame, Notre Dame, IN  46556.
//
// Permission to modify the code and to distribute modified code is
// granted, provided the text of this NOTICE is retained, a notice that
// the code was modified is included with the above COPYRIGHT NOTICE and
// with the COPYRIGHT NOTICE in the LICENSE file, and that the LICENSE
// file is distributed with the modified code.
//
// LICENSOR MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.
// By way of example, but not limitation, Licensor MAKES NO
// REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED SOFTWARE COMPONENTS
// OR DOCUMENTATION WILL NOT INFRINGE ANY PATENTS, COPYRIGHTS, TRADEMARKS
// OR OTHER RIGHTS.
//
//===========================================================================

#ifndef __GMM_TRI_SOLVE_H
#define __GMM_TRI_SOLVE_H

#include <gmm_solvers.h>

namespace gmm {

  template <class TriMatrix, class VecX>
  inline void upper_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				col_major, abstract_sparse) {
    typename linalg_traits<TriMatrix>::value_type x_j;
    for (int j = k - 1; j >= 0; --j) {
      typedef typename linalg_traits<TriMatrix>::const_sub_col_type COL;
      COL c = mat_const_col(T, j);
      typename linalg_traits<COL>::const_iterator 
	it = vect_const_begin(c), ite = vect_const_end(c);
      x[j] /= c[j];
      for (x_j = x[j]; it != ite ; ++it)
	if (it.index() < j) x[it.index()] -= x_j * (*it);
    }    
  }

  template <class TriMatrix, class VecX>
  inline void upper_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				col_major, abstract_plain) {
    typename linalg_traits<TriMatrix>::value_type x_j;
    for (int j = k - 1; j >= 0; --j) {
      typedef typename linalg_traits<TriMatrix>::const_sub_col_type COL;
      COL c = mat_const_col(T, j);
      typename linalg_traits<COL>::const_iterator
	it = vect_const_begin(c), ite = it + j;
      typename linalg_traits<VecX>::iterator itx = vect_begin(x);
      x[j] /= c[j];
      for (x_j = x[j]; it != ite ; ++it, ++itx) *itx -= x_j * (*it);
    }
  }

  template <class TriMatrix, class VecX>
  inline void lower_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				col_major, abstract_sparse) {
    typename linalg_traits<TriMatrix>::value_type x_j;
    for (int j = 0; j < k; ++j) {
      typedef typename linalg_traits<TriMatrix>::const_sub_col_type COL;
      COL c = mat_const_col(T, j);
      typename linalg_traits<COL>::const_iterator 
	it = vect_const_begin(c), ite = vect_const_end(c);
      x[j] /= c[j];
      for (x_j = x[j]; it != ite ; ++it)
	if (it.index() > j && it.index() < k) x[it.index()] -= x_j * (*it);
    }    
  }
  
  template <class TriMatrix, class VecX>
  inline void lower_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				col_major, abstract_plain) {
    typename linalg_traits<TriMatrix>::value_type x_j;
    for (int j = 0; j < k; ++j) {
      typedef typename linalg_traits<TriMatrix>::const_sub_col_type COL;
      COL c = mat_const_col(T, j);
      typename linalg_traits<COL>::const_iterator 
	it = vect_const_begin(c) + (j+1), ite = vect_const_begin(c) + k;
      typename linalg_traits<VecX>::iterator itx = vect_begin(x) + (j+1);
      x[j] /= c[j];
      for (x_j = x[j]; it != ite ; ++it, ++itx) *itx -= x_j * (*it);
    }    
  }
  

  template <class TriMatrix, class VecX>
  inline void upper_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				row_major, abstract_sparse) {
    typename linalg_traits<TriMatrix>::value_type t;
   
    for (int i = k - 1; i >= 0; --i) {
      typedef typename linalg_traits<TriMatrix>::const_sub_row_type ROW;
      ROW c = mat_const_row(T, i);
      typename linalg_traits<ROW>::const_iterator 
	it = vect_const_begin(c), ite = vect_const_end(c);

      for (t = x[i]; it != ite; ++it)
	if (it.index() > i && it.index() < k) t -= (*it) * x[it.index()];
      x[i] = t / c[i];      
    }    
  }

  template <class TriMatrix, class VecX>
  inline void upper_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				row_major, abstract_plain) {
    typename linalg_traits<TriMatrix>::value_type t;
   
    for (int i = k - 1; i >= 0; --i) {
      typedef typename linalg_traits<TriMatrix>::const_sub_row_type ROW;
      ROW c = mat_const_row(T, i);
      typename linalg_traits<ROW>::const_iterator 
	it = vect_const_begin(c) + (i + 1), ite = vect_const_begin(c) + k;
      typename linalg_traits<VecX>::iterator itx = vect_begin(x) + (i+1);
      
      for (t = x[i]; it != ite; ++it, ++itx) t -= (*it) * (*itx);
      x[i] = t / c[i];    
    }    
  }

  template <class TriMatrix, class VecX>
  inline void lower_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				row_major, abstract_sparse) {
    typename linalg_traits<TriMatrix>::value_type t;
   
    for (int i = 0; i < k; ++i) {
      typedef typename linalg_traits<TriMatrix>::const_sub_row_type ROW;
      ROW c = mat_const_row(T, i);
      typename linalg_traits<ROW>::const_iterator 
	it = vect_const_begin(c), ite = vect_const_end(c);

      for (t = x[i]; it != ite; ++it)
	if (it.index() < i) t -= (*it) * x[it.index()];
      x[i] = t / c[i];  
    }    
  }

  template <class TriMatrix, class VecX>
  inline void lower_tri_solve__(const TriMatrix& T, VecX& x, size_t k,
				row_major, abstract_plain) {
    typename linalg_traits<TriMatrix>::value_type t;
   
    for (int i = 0; i < k; ++i) {
      typedef typename linalg_traits<TriMatrix>::const_sub_row_type ROW;
      ROW c = mat_const_row(T, i);
      typename linalg_traits<ROW>::const_iterator 
	it = vect_const_begin(c), ite = it + i;
      typename linalg_traits<VecX>::iterator itx = vect_begin(x);

      for (t = x[i]; it != ite; ++it, ++itx) t -= (*it) * (*itx);
      x[i] = t / c[i];  
    }
  }


//: Triangular Solve:  <tt>x <- T^{-1} * x</tt>
//
//  To use with a sparse matrix, the sparse matrix must be wrapped with
//  a triangle adaptor. You must specify "packed" in the triangle
//  adaptor. The sparse matrix must only have elements in the correct
//  side.
//

  template <class TriMatrix, class VecX>
  void upper_tri_solve(const TriMatrix& T, VecX &x_) {
    VecX& x = const_cast<VecX&>(x_);
    if ((mat_nrows(T) > vect_size(x)) || (mat_ncols(T) > vect_size(x))
	|| (mat_nrows(T) != mat_ncols(T)) || is_sparse(x_))
      DAL_THROW(dimension_error, "dimensions mismatch");
    upper_tri_solve__(T, x, mat_nrows(T), 
		      typename principal_orientation_type<typename
		      linalg_traits<TriMatrix>::sub_orientation>::potype(),
		      typename linalg_traits<TriMatrix>::storage_type());
  }
  
  template <class TriMatrix, class VecX>
  void lower_tri_solve(const TriMatrix& T, VecX &x_) {
    VecX& x = const_cast<VecX&>(x_);
    if ((mat_nrows(T) > vect_size(x)) || (mat_ncols(T) > vect_size(x))
	|| (mat_nrows(T) != mat_ncols(T)) || is_sparse(x_))
      DAL_THROW(dimension_error, "dimensions mismatch");
    lower_tri_solve__(T, x, mat_nrows(T), 
		      typename principal_orientation_type<typename
		      linalg_traits<TriMatrix>::sub_orientation>::potype(),
		      typename linalg_traits<TriMatrix>::storage_type());
  }

  template <class TriMatrix, class VecX>
  void upper_tri_solve(const TriMatrix& T, VecX &x_, size_t k) {
    VecX& x = const_cast<VecX&>(x_);
    if ((mat_nrows(T) < k) || (vect_size(x) < k)
	|| (mat_ncols(T) < k) || is_sparse(x_))
      DAL_THROW(dimension_error, "dimensions mismatch");
    upper_tri_solve__(T, x, k, 
		      typename principal_orientation_type<typename
		      linalg_traits<TriMatrix>::sub_orientation>::potype(),
		      typename linalg_traits<TriMatrix>::storage_type());
  }
  
  template <class TriMatrix, class VecX>
  void lower_tri_solve(const TriMatrix& T, VecX &x_, size_t k) {
    VecX& x = const_cast<VecX&>(x_);
    if ((mat_nrows(T) < k) || (vect_size(x) < k)
	|| (mat_ncols(T) < k) || is_sparse(x_))
      DAL_THROW(dimension_error, "dimensions mismatch");
    lower_tri_solve__(T, x, k, 
		      typename principal_orientation_type<typename
		      linalg_traits<TriMatrix>::sub_orientation>::potype(),
		      typename linalg_traits<TriMatrix>::storage_type());
  }


 



}


#endif //  __GMM_TRI_SOLVE_H
