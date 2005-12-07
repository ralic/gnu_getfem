// -*- c++ -*- (enables emacs c++ mode)
//========================================================================
//
// Library : GEneric Tool for Finite Elements Methods (getfem)
// File    : getfem_mesh_im.cc : integration methods on convex meshes.
//           
// Date    : January 26, 2005.
// Author  : Yves Renard <Yves.Renard@insa-toulouse.fr>
//
//========================================================================
//
// Copyright (C) 2005-2005 Yves Renard
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

#include <getfem_mesh_im.h>


namespace getfem {
  
  void mesh_im::receipt(const MESH_CLEAR &) { clear(); }
  void mesh_im::receipt(const MESH_DELETE &) {
    clear(); is_valid_ = false;
    sup_sender(linked_mesh_->lmsg_sender());
  }
  void mesh_im::receipt(const MESH_SUP_CONVEX &m) { 
    if (im_convexes[m.icv])
      { im_convexes[m.icv] = false; }
  }
  void mesh_im::receipt(const MESH_SWAP_CONVEX &m) { 
    im_convexes.swap(m.icv1, m.icv2);
    ims.swap(m.icv1, m.icv2);
  }
   
  void mesh_im::set_integration_method(size_type cv, pintegration_method pim) {
    if (pim == NULL)
      { if (im_convexes.is_in(cv)) { im_convexes.sup(cv); touch(); } }
    else if (!im_convexes.is_in(cv) || ims[cv] != pim) {
      if (pim->type() != IM_NONE && 
	  linked_mesh_->structure_of_convex(cv)->basic_structure() 
	  != pim->structure())
	DAL_THROW(std::logic_error,
		  "Incompatibility between integration method " << getfem::name_of_int_method(pim) << 
		  " and mesh element " << bgeot::name_of_geometric_trans(linked_mesh_->trans_of_convex(cv)));
      im_convexes.add(cv);
      ims[cv] = pim;
      touch();
    }
  }

  void mesh_im::set_integration_method(const dal::bit_vector &cvs, 
				       pintegration_method pim) { 
    for (dal::bv_visitor cv(cvs); !cv.finished(); ++cv)
      set_integration_method(cv, pim);
  }

  void mesh_im::set_integration_method(pintegration_method pim) { 
    set_integration_method(linked_mesh().convex_index(), pim);
  }
  
  void mesh_im::set_integration_method(const dal::bit_vector &cvs, 
				       dim_type im_degree) {
    if (im_degree == dim_type(-1)) DAL_THROW(dal::failure_error, "im_degree==-1");
    for (dal::bv_visitor cv(cvs); !cv.finished(); ++cv) {
      pintegration_method pim = 
	getfem::classical_approx_im(linked_mesh().trans_of_convex(cv), im_degree);
      set_integration_method(cv, pim);
    }
  }

  void mesh_im::clear(void) {
    ims.clear(); im_convexes.clear();
    touch();
  }

  mesh_im::mesh_im(getfem_mesh &me) {
    linked_mesh_ = &me; is_valid_ = true;
    this->add_dependency(me);
    add_sender(me.lmsg_sender(), *this,
	       lmsg::mask(MESH_CLEAR::ID) | lmsg::mask(MESH_SUP_CONVEX::ID) |
	       lmsg::mask(MESH_SWAP_CONVEX::ID) | lmsg::mask(MESH_DELETE::ID));
  }

  mesh_im::~mesh_im() {}


  void mesh_im::read_from_file(std::istream &ist) {
    dal::bit_vector npt;
    dal::dynamic_array<double> tmpv;
    char tmp[1024];
    ist.precision(16);
    clear();
    ist.seekg(0);ist.clear();
    ftool::read_until(ist, "BEGIN MESH_IM");

    while (true)
    {
      ist >> std::ws; ftool::get_token(ist, tmp, 1023);
      if (strcmp(tmp, "END")==0) {
	break;
      } else if (strcmp(tmp, "CONVEX")==0) {
	ftool::get_token(ist, tmp, 1023);
	size_type ic = atoi(tmp);
	if (!linked_mesh().convex_index().is_in(ic)) {
	  DAL_THROW(failure_error, "Convex " << ic <<
		    " does not exist, are you sure "
		    "that the mesh attached to this object is right one ?");
	}
	
	ftool::get_token(ist, tmp, 1023);
	getfem::pintegration_method pfi = getfem::int_method_descriptor(tmp);
	if (!pfi) DAL_THROW(failure_error,
	  "could not create the integration method '" << tmp << "'");
	
	set_integration_method(ic, pfi);
      } else if (strlen(tmp)) {
	DAL_THROW(failure_error, "Unexpected token '" << tmp <<
		  "' [pos=" << std::streamoff(ist.tellg()) << "]");
      } else if (ist.eof()) {
	DAL_THROW(failure_error, "Unexpected end of stream "
		  << "(missing BEGIN MESH_IM/END MESH_IM ?)");	
      }
    }
  }

  void mesh_im::read_from_file(const std::string &name)
  { 
    std::ifstream o(name.c_str());
    if (!o) DAL_THROW(file_not_found_error,
		      "mesh_im file '" << name << "' does not exist");
    read_from_file(o);
    o.close();
  }

  void mesh_im::write_to_file(std::ostream &ost) const
  {
    ost << '\n' << "BEGIN MESH_IM" << '\n' << '\n';
    for (dal::bv_visitor cv(convex_index()); !cv.finished(); ++cv) {
      ost << " CONVEX " << cv;
      ost << " " << name_of_int_method(int_method_of_element(cv));
      ost << '\n';
    }

    ost << "END MESH_IM" << '\n';
  }

  void mesh_im::write_to_file(const std::string &name, bool with_mesh) const
  {
    std::ofstream o(name.c_str());
    if (!o)
      DAL_THROW(failure_error, "impossible to open file '" << name << "'");
    o << "% GETFEM MESH_IM FILE " << '\n';
    o << "% GETFEM VERSION " << GETFEM_VERSION << '\n' << '\n' << '\n';
    if (with_mesh) linked_mesh().write_to_file(o);
    write_to_file(o);
    o.close();
  }
}  /* end of namespace getfem.                                             */



