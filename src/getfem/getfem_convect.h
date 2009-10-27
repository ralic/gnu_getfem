// -*- c++ -*- (enables emacs c++ mode)
//===========================================================================
//
// Copyright (C) 2009-2009 Yves Renard
//
// This file is a part of GETFEM++
//
// Getfem++  is  free software;  you  can  redistribute  it  and/or modify it
// under  the  terms  of the  GNU  Lesser General Public License as published
// by  the  Free Software Foundation;  either version 2.1 of the License,  or
// (at your option) any later version.
// This program  is  distributed  in  the  hope  that it will be useful,  but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or  FITNESS  FOR  A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.
// You  should  have received a copy of the GNU Lesser General Public License
// along  with  this program;  if not, write to the Free Software Foundation,
// Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
//
// As a special exception, you  may use  this file  as it is a part of a free
// software  library  without  restriction.  Specifically,  if   other  files
// instantiate  templates  or  use macros or inline functions from this file,
// or  you compile this  file  and  link  it  with other files  to produce an
// executable, this file  does  not  by itself cause the resulting executable
// to be covered  by the GNU Lesser General Public License.  This   exception
// does not  however  invalidate  any  other  reasons why the executable file
// might be covered by the GNU Lesser General Public License.
//
//===========================================================================

/**@file getfem_convect.h
   @author Yves Renard <Yves.Renard@insa-lyon.fr>,
   @date October 27, 2009.
   @brief Compute the convection of a quantity with respect to a vector field.
*/
#ifndef GETFEM_CONVECT_H__
#define GETFEM_CONVECT_H__

#include "getfem_mesh_fem.h"
#include "getfem_interpolation.h"
#include "gmm/gmm_dense_qr.h"

namespace getfem
{
  /** Compute the convection of a quantity on a getfem::mesh_fem with respect
      to a vector field
      @param mf the source mesh_fem. Should be of Lagrange type.
      @param U the source field.
      @param mf_v the mesh_fem on which the vector field is described
      @param V contains the vector field described on mf_v.
      @param nt number of time integration step.
  */
  template<class VECT1, class VECT2>
  void convect(const mesh_fem &mf, const mesh_fem &mf_v,
	       VECT1 &U, const VECT2 &V, size_type nt) {
    typedef typename gmm::linalg_traits<VECT1>::value_type T;

    GMM_ASSERT1(!(mf.is_reduced()),
		"This convection algorithm work only on pure Lagrange fems");
    /* test if mf is really of Lagrange type.         */
    for (dal::bv_visitor cv(mf.convex_index()); !cv.finished();++cv) {
      pfem pf_t = mf.fem_of_element(cv);
      GMM_ASSERT1(pf_t->target_dim() == 1 && pf_t->is_lagrange(),
		  "This convection algorithm work only on pure Lagrange fems");
    }

    // Get the nodes of mf
    getfem::mesh_trans_inv mti(msh, EPS);
    size_type qdim = mf.get_qdim();
    size_type nbpts = mf.nb_basic_dof() / qdim;
    for (size_type i = 0; i < nbpts; ++i)
      mti.add_point(mf.point_of_basic_dof(i * qdim));

    // Convect the nodes with respect to v
    size_type qqdimt = (gmm::vect_size(V) / mf_v.nb_dof()) * mf_v.get_qdim();
    std::vector<T> VI(nbpts*qqdimt);

    for (size_type i = 0; i < nt; ++i) {
      interpolation(mf_v, mti, V, VI, base_matrix(), 0, true /* extrapolation */);
      // + bouger les points du mti
    }

    // 3 interpolation finale
    std::vector<T> U(nbpts*qdim);
    interpolation(mf, mti, U, UI, base_matrix(), 0, true /* extrapolation */);
    gmm::copy(UI, U);
  }



#endif