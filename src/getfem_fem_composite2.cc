// -*- c++ -*- (enables emacs c++ mode)
//========================================================================
//
// Library : GEneric Tool for Finite Element Methods (getfem)
// File    : getfem_fem_composite.cc : composite fem version 2
//           
// Date    : August 26, 2002.
// Author  : Yves Renard <Yves.Renard@insa-toulouse.fr>
//
//========================================================================
//
// Copyright (C) 2002-2006 Yves Renard
//
// This file is a part of GETFEM++
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//========================================================================


#include <getfem_poly_composite.h>
#include <getfem_integration.h>
#include <getfem_fem.h>
#include <dal_naming_system.h>

namespace getfem {

  // poly composite version 2

  class polynomial_composite2 {

  protected :
    const mesh_precomposite *mp;
    std::vector<bgeot::base_poly> polytab;

  public :
    
    template <class ITER> scalar_type eval(const ITER &it) const;
    scalar_type eval(const base_node &pt) const;
    void derivative(short_type k);
    base_poly &poly_of_subelt(size_type l) { return polytab[l]; }
    const base_poly &poly_of_subelt(size_type l) const { return polytab[l]; }
    

    polynomial_composite2(void) {}
    polynomial_composite2(const mesh_precomposite &m);

  };

  template <class ITER>
  scalar_type polynomial_composite2::eval(const ITER &it) const {
    base_node pt(mp->dim());
    std::copy(it, it + mp->dim(), pt.begin());
    return eval(pt);
  }

  scalar_type polynomial_composite2::eval(const base_node &pt) const {
    base_node p0(mp->dim()), p1(mp->dim());
    std::fill(mp->elt.begin(), mp->elt.end(), true);
    bgeot::mesh_structure::ind_cv_ct::const_iterator itc, itce;
    
    mesh_precomposite::PTAB::const_sorted_iterator
      it1 = mp->vertexes.sorted_ge(pt), it2 = it1;    
    size_type i1 = it1.index(), i2;

    --it2; i2 = it2.index();

    while (i1 != size_type(-1) || i2 != size_type(-1)) {
      if (i1 != size_type(-1)) {
	const bgeot::mesh_structure::ind_cv_ct &tc
	  = mp->linked_mesh().convex_to_point(i1);
	itc = tc.begin(); itce = tc.end();
	for (; itc != itce; ++itc) {
	  size_type ii = *itc;
	  if (mp->elt[ii]) {
	    mp->elt[ii] = false;
	    p0 = pt; p0 -= mp->orgs[ii];
	    gmm::mult(gmm::transposed(mp->gtrans[ii]), p0, p1);
	    if (mp->trans_of_convex(ii)->convex_ref()->is_in(p1) < 1E-10)
	      return  polytab[ii].eval(pt.begin());
	  }
	}
	++it1; i1 = it1.index();
      }
      if (i2 != size_type(-1)) {
	const bgeot::mesh_structure::ind_cv_ct &tc
	  = mp->linked_mesh().convex_to_point(i2);
	itc = tc.begin(); itce = tc.end();
	for (; itc != itce; ++itc) {
	  size_type ii = *itc;
	  if (mp->elt[ii]) {
	    mp->elt[ii] = false;
	    p0 = pt; p0 -= mp->orgs[ii];
	    gmm::mult(gmm::transposed(mp->gtrans[ii]), p0, p1);
	    if (mp->trans_of_convex(ii)->convex_ref()->is_in(p1) < 1E-10)
	      return  polytab[ii].eval(pt.begin());
	  }
	}
	--it2; i2 = it2.index();
      }
    }
    DAL_THROW(internal_error, "Element not found in composite polynomial: "
	      << pt);
  }


  polynomial_composite2::polynomial_composite2(const mesh_precomposite &m)
      : mp(&m), polytab(m.nb_convex()) {
    std::fill(polytab.begin(), polytab.end(), base_poly(m.dim(), 0));
  }

  void polynomial_composite2::derivative(short_type k) {
    for (size_type ic = 0; ic < mp->nb_convex(); ++ic)
      polytab[ic].derivative(k);
  }

  typedef dal::naming_system<virtual_fem>::param_list fem_param_list;

  /* ******************************************************************** */
  /*	Composite C1 P3 element on the triangle                           */
  /* ******************************************************************** */

  struct composite_C1_triangle__ : public fem<polynomial_composite2> {
    virtual void mat_trans(base_matrix &M, const base_matrix &G,
			   bgeot::pgeometric_trans pgt) const;
    mesh_precomposite mp;
    composite_C1_triangle__(void);
  };


  void composite_C1_triangle__::mat_trans(base_matrix &M, const base_matrix &G,
					  bgeot::pgeometric_trans pgt) const {

    static bgeot::pgeotrans_precomp pgp;
    static bgeot::pgeometric_trans pgt_stored = 0;
    static base_matrix K(2, 2);
    dim_type N = G.nrows();
    
    if (N != 2) DAL_THROW(failure_error, "Sorry, this version of hermite "
			  "element works only on dimension two.")
      if (pgt != pgt_stored)
	{ pgt_stored = pgt; pgp = bgeot::geotrans_precomp(pgt, node_tab(0)); }
    gmm::copy(gmm::identity_matrix(), M);
    
    gmm::mult(G, pgp->grad(0), K);
    for (size_type i = 0; i < 6; ++i) {
      if (i && !(pgt->is_linear())) gmm::mult(G, pgp->grad(i), K);
      M( 6+i, 6+i) = K(0,0); M( 6+i, 12+i) = K(0,1);
      M(12+i, 6+i) = K(1,0); M(12+i, 12+i) = K(1,1);
    }
  }

  composite_C1_triangle__::composite_C1_triangle__(void) {

    getfem::mesh m;
    size_type i0 = m.add_point(base_node(1.0/3.0, 1.0/3.0));
    size_type i1 = m.add_point(base_node(0.0, 0.0));
    size_type i2 = m.add_point(base_node(0.0, 1.0));
    size_type i3 = m.add_point(base_node(1.0, 0.0));
    m.add_triangle(i0, i2, i3);
    m.add_triangle(i0, i3, i1);
    m.add_triangle(i0, i1, i2);
    mp = mesh_precomposite(m);
    
    std::stringstream s
      ("1 - 3*x^2 - 6*x*y - 3*y^2 + 2*x^3 + 6*x^2*y + 6*x*y^2 + 2*y^3;"
       "0;"
       "0;"
       "0;"
       "3*x^2 + 13*x*y - 2*x^3 - 13*x^2*y - 13*x*y^2;"
       "-7*x*y + 3*y^2 + 7*x^2*y + 7*x*y^2 - 2*y^3;"
       "0;"
       "1 - 3*x^2 - 6*x*y - 3*y^2 + 2*x^3 + 6*x^2*y + 6*x*y^2 + 2*y^3;"
       "0;"
       "0;"
       "0;"
       "3*x^2 - 2*x^3;"
       "13*x*y + 3*y^2 - 13*x^2*y - 13*x*y^2 - 2*y^3;"
       "3*x^2 - 7*x*y - 2*x^3 + 7*x^2*y + 7*x*y^2;"
       "1 - 3*x^2 - 6*x*y - 3*y^2 + 2*x^3 + 6*x^2*y + 6*x*y^2 + 2*y^3;"
       "0;"
       "7*x*y - 7*x^2*y - 7*x*y^2;"
       "1 - 3*x^2 - 13*x*y - 3*y^2 + 2*x^3 + 13*x^2*y + 13*x*y^2 + 2*y^3;"
       "3*x^2 + 6*x*y - 2*x^3 - 6*x^2*y - 6*x*y^2;"
       "6*x*y + 3*y^2 - 6*x^2*y - 6*x*y^2 - 2*y^3;"
       "0;"
       "0;"
       "3*y^2 - 2*y^3;"
       "0;"
       "x - 2*x^2 - 2*x*y + x^3 + 2*x^2*y + x*y^2;"
       "0;"
       "0;"
       "0;"
       "-x^2 + 2*x*y + x^3 - 2*x^2*y - 2*x*y^2;"
       "-2*x*y + 1*y^2 + 2*x^2*y + 2*x*y^2 - 1*y^3;"
       "0;"
       "1*x - 2*x^2 - 4*x*y + x^3 + 4*x^2*y + 3*x*y^2;"
       "0;"
       "0;"
       "0;"
       "-1*x^2 + 1*x^3;"
       "-1*x*y + 1*x^2*y + 2*x*y^2;"
       "1*x*y - 2*x^2*y - 1*x*y^2;"
       "x - 2*x^2 + 1*x^3 - 1*x*y^2;"
       "0;"
       "-1*x*y + 1*x^2*y + 1*x*y^2;"
       "-y + 3*x*y + 2*y^2 - 2*x^2*y - 3*x*y^2 - 1*y^3;"
       "-1*x^2 + 1*x^3;"
       "-2*x*y + 2*x^2*y + 3*x*y^2;"
       "0;"
       "0;"
       "x*y^2;"
       "0;"
       "y - 2*x*y - 2*y^2 + x^2*y + 2*x*y^2 + y^3;"
       "0;"
       "0;"
       "0;"
       "-1*x*y + 2*x^2*y + 1*x*y^2;"
       "1*x*y - 1*x^2*y - 2*x*y^2;"
       "0;"
       "y - 2*y^2 - x^2*y + 1*y^3;"
       "0;"
       "0;"
       "0;"
       "x^2*y;"
       "2*x*y - y^2 - 2*x^2*y - 2*x*y^2 + y^3;"
       "1*x^2 - 2*x*y - 1*x^3 + 2*x^2*y + 2*x*y^2;"
       "1*y - 4*x*y - 2*y^2 + 3*x^2*y + 4*x*y^2 + y^3;"
       "0;"
       "-x*y + x^2*y + x*y^2;"
       "-x + 2*x^2 + 3*x*y - 1*x^3 - 3*x^2*y - 2*x*y^2;"
       "-2*x*y + 3*x^2*y + 2*x*y^2;"
       "-y^2 + y^3;"
       "0;"
       "0;"
       "-y^2 + y^3;"
       "0;"
       "-27*x*y + 27*x^2*y + 27*x*y^2;"
       "27*x*y - 27*x^2*y - 27*x*y^2;"
       "0;"
       "0;");

    bgeot::pconvex_ref cr = bgeot::simplex_of_reference(2);
    pmesh pm;
    pmesh_precomposite pmp;
    structured_mesh_for_convex(cr, 2, pm, pmp);

    pm->write_to_file(cout);

    mref_convex() = cr;
    dim() = cr->structure()->dim();
    is_polynomialcomp() = true;
    is_equivalent() = false;
    is_polynomial() = false;
    is_lagrange() = false;
    estimated_degree() = 3;
    init_cvs_node();

    base()=std::vector<polynomial_composite2>(19,polynomial_composite2(*pmp));
    for (size_type k = 0; k < 19; ++k)
      for (size_type ic = 0; ic < 4; ++ic) {
	base()[k].poly_of_subelt(ic) = bgeot::read_base_poly(2, s);
	cout << "poly read : " << base()[k].poly_of_subelt(ic) << endl;
      }
    pdof_description pdof = lagrange_dof(2);
    for (size_type i = 0; i < 3; ++i){
      if (i == 1) pdof = derivative_dof(1, 0);
      if (i == 2) pdof = derivative_dof(1, 1);
      add_node(pdof, base_node(0.0, 0.0));
      add_node(pdof, base_node(0.5, 0.0));
      add_node(pdof, base_node(1.0, 0.0));
      add_node(pdof, base_node(0.0, 0.5));
      add_node(pdof, base_node(0.5, 0.5));
      add_node(pdof, base_node(0.0, 1.0));
    }
    add_node(lagrange_dof(2), base_node(1.0/3.0, 1.0/3.0));
  }


  pfem composite_C1_triangle_fem
  (fem_param_list &params,
   std::vector<dal::pstatic_stored_object> &dependencies) {
    if (params.size() != 0)
      DAL_THROW(failure_error, "Bad number of parameters : " << params.size()
		<< " should be 0.");
    virtual_fem *p = new composite_C1_triangle__;
    dependencies.push_back(p->ref_convex(0));
    dependencies.push_back(p->node_tab(0));
    return p;
  }








  
}  /* end of namespace getfem.                                            */
