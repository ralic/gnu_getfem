/* -*- c++ -*- (enables emacs c++ mode)                                    */
//=======================================================================
// Copyright (C) 1997-2001
// Authors: Andrew Lumsdaine <lums@osl.iu.edu> 
//          Lie-Quan Lee     <llee@osl.iu.edu>
//
// You should have received a copy of the License Agreement for the
// Iterative Template Library along with the software;  see the
// file LICENSE.  
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
//=======================================================================
/* *********************************************************************** */
/*                                                                         */
/* Library :  Generic Matrix Methods  (gmm)                                */
/* File    :  gmm_precond_ilu.h : modified version from I.T.L.             */
/*     									   */
/* Date : June 5, 2003.                                                    */
/* Author : Yves Renard, Yves.Renard@gmm.insa-tlse.fr                      */
/*                                                                         */
/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2003  Yves Renard.                                        */
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
#ifndef GMM_PRECOND_ILU_H
#define GMM_PRECOND_ILU_H

//  Incomplete LU without fill-in Preconditioner.
//
// Notes: The idea under a concrete Preconditioner such 
//        as Incomplete LU is to create a Preconditioner
//        object to use in iterative methods. 
//

#include <gmm_solvers.h>
#include <gmm_tri_solve.h>
#include <gmm_interface.h>

namespace gmm {

  template <class Matrix>
  class ilu_precond {

  public :
    typedef typename linalg_traits<Matrix>::value_type value_type;
    typedef csr_matrix_ref<value_type *, size_type *, size_type *, 0> tm_type;

    tm_type U, L;
    bool invert;
  protected :
    std::vector<value_type> L_val, U_val;
    std::vector<size_type> L_ind, U_ind, L_ptr, U_ptr;
 
    template<class M> void do_ilu(const M& A, row_major);
    void do_ilu(const Matrix& A, col_major);

  public:
    
    size_type nrows(void) const { return mat_nrows(L); }
    size_type ncols(void) const { return mat_ncols(U); }
    
    ilu_precond(const Matrix& A) :
      invert(false), L_ptr(mat_nrows(A)+1), U_ptr(mat_nrows(A)+1) { 
      if (!is_sparse(A))
	DAL_THROW(failure_error,
		  "Matrix should be sparse for incomplete ilu");
      do_ilu(A, typename principal_orientation_type<typename
	     linalg_traits<Matrix>::sub_orientation>::potype());
    }
  };

  template <class Matrix> template <class M>
  void ilu_precond<Matrix>::do_ilu(const M& A, row_major) {
    size_type L_loc = 0, U_loc = 0, n = mat_nrows(A), i, j;
    L_ptr[0] = 0; U_ptr[0] = 0;

    for (int count = 0; count < 2; ++count) {
      if (count) { 
	L_val.resize(L_loc); L_ind.resize(L_loc);
	U_val.resize(U_loc); U_ind.resize(U_loc);
      }
      L_loc = U_loc = 0;
      for (i = 0; i < n; ++i) {
	typedef typename linalg_traits<M>::const_sub_row_type row_type;
	row_type row = mat_const_row(A, i);
	typename linalg_traits<row_type>::const_iterator
	  it = vect_const_begin(row), ite = vect_const_end(row);
	for (; it != ite; ++it) {
	  if (it.index() < i) {
	    if (count) { L_val[L_loc] = *it; L_ind[L_loc] = it.index(); }
	    L_loc++;
	  }
	  else {
	    if (count) { U_val[U_loc] = *it; U_ind[U_loc] = it.index(); }
	    U_loc++;
	  }
	}
        L_ptr[i+1] = L_loc; U_ptr[i+1] = U_loc;
      }
    }
    
    size_type qn, pn, rn; 
    for (i = 1; i < n; i++) {
      for (j = L_ptr[i]; j < L_ptr[i+1]; j++) {
	pn = U_ptr[L_ind[j]];
	
	value_type multiplier = (L_val[j] /= U_val[pn]);
	
	qn = j + 1;
	rn = U_ptr[i];
	
	for (pn++; U_ind[pn] < i && pn < U_ptr[L_ind[j]+1]; pn++) {
	  while (L_ind[qn] < U_ind[pn] && qn < L_ptr[i+1])
	    qn++;
	  if (U_ind[pn] == L_ind[qn] && qn < L_ptr[i+1])
	    L_val[qn] -= multiplier * U_val[pn];
	}
	for (; pn < U_ptr[L_ind[j]+1]; pn++) {
	  while (U_ind[rn] < U_ind[pn] && rn < U_ptr[i+1])
	    rn++;
	  if (U_ind[pn] == U_ind[rn] && rn < U_ptr[i+1])
	    U_val[rn] -= multiplier * U_val[pn];
	}
      }
    }

    L = tm_type(&(L_val[0]), &(L_ind[0]), &(L_ptr[0]), n, mat_ncols(A));
    U = tm_type(&(U_val[0]), &(U_ind[0]), &(U_ptr[0]), n, mat_ncols(A));
  }
  
  template <class Matrix>
  void ilu_precond<Matrix>::do_ilu(const Matrix& A, col_major) {
    do_ilu(gmm::transposed(A), row_major());
    invert = true;
  }

  template <class Matrix, class V1, class V2> inline
  void mult(const ilu_precond<Matrix>& P, const V1 &v1, V2 &v2) {
    gmm::copy(v1, v2);
    if (P.invert) {
      gmm::lower_tri_solve(gmm::transposed(P.U), v2, false);
      gmm::upper_tri_solve(gmm::transposed(P.L), v2, true);
    }
    else {
      gmm::lower_tri_solve(P.L, v2, true);
      gmm::upper_tri_solve(P.U, v2, false);
    }
  }

  template <class Matrix, class V1, class V2> inline
  void transposed_mult(const ilu_precond<Matrix>& P,const V1 &v1,V2 &v2) {
    gmm::copy(v1, v2);
    if (P.invert) {
      gmm::lower_tri_solve(P.L, v2, true);
      gmm::upper_tri_solve(P.U, v2, false);
    }
    else {
      gmm::lower_tri_solve(gmm::transposed(P.U), v2, false);
      gmm::upper_tri_solve(gmm::transposed(P.L), v2, true);
    }
  }

  template <class Matrix, class V1, class V2> inline
  void left_mult(const ilu_precond<Matrix>& P, const V1 &v1, V2 &v2) {
    copy(v1, v2);
    if (P.invert) gmm::lower_tri_solve(gmm::transposed(P.U), v2, false);
    else gmm::lower_tri_solve(P.L, v2, true);
  }

  template <class Matrix, class V1, class V2> inline
  void right_mult(const ilu_precond<Matrix>& P, const V1 &v1, V2 &v2) {
    copy(v1, v2);
    if (P.invert) gmm::upper_tri_solve(gmm::transposed(P.L), v2, true);
    else gmm::upper_tri_solve(P.U, v2, false);
  }

  template <class Matrix, class V1, class V2> inline
  void transposed_left_mult(const ilu_precond<Matrix>& P, const V1 &v1,
			    V2 &v2) {
    copy(v1, v2);
    if (P.invert) gmm::upper_tri_solve(P.U, v2, false);
    else gmm::upper_tri_solve(gmm::transposed(P.L), v2, true);
  }

  template <class Matrix, class V1, class V2> inline
  void transposed_right_mult(const ilu_precond<Matrix>& P, const V1 &v1,
			     V2 &v2) {
    copy(v1, v2);
    if (P.invert) gmm::lower_tri_solve(P.L, v2, true);
    else gmm::lower_tri_solve(gmm::transposed(P.U), v2, false);
  }


}

#endif 

