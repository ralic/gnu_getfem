// -*- c++ -*- (enables emacs c++ mode)
//===========================================================================
//
// Copyright (C) 2011-2011 Yves Renard.
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
//===========================================================================
// $Id: gf_mesher_object_get.cc 3468 2010-02-24 20:12:15Z renard $

#include <getfemint_misc.h>
#include <getfemint_workspace.h>
#include <getfemint_mesher_object.h>
#include <getfem/getfem_mesher.h>

using namespace getfemint;

/*@GFDOC
    General function for querying information about mesher_object objects.
@*/


// Object for the declaration of a new sub-command.

struct sub_gf_mesher_object_get : virtual public dal::static_stored_object {
  int arg_in_min, arg_in_max, arg_out_min, arg_out_max;
  virtual void run(getfemint::mexargs_in& in,
		   getfemint::mexargs_out& out,
		   getfem::mesher_signed_distance *paf) = 0;
};

typedef boost::intrusive_ptr<sub_gf_mesher_object_get> psub_command;

// Function to avoid warning in macro with unused arguments.
template <typename T> static inline void dummy_func(T &) {}

#define sub_command(name, arginmin, arginmax, argoutmin, argoutmax, code) { \
    struct subc : public sub_gf_mesher_object_get {				\
      virtual void run(getfemint::mexargs_in& in,			\
		       getfemint::mexargs_out& out,			\
		       getfem::mesher_signed_distance *paf)		\
      { dummy_func(in); dummy_func(out); dummy_func(paf); code }	\
    };									\
    psub_command psubc = new subc;					\
    psubc->arg_in_min = arginmin; psubc->arg_in_max = arginmax;		\
    psubc->arg_out_min = argoutmin; psubc->arg_out_max = argoutmax;	\
    subc_tab[cmd_normalize(name)] = psubc;				\
  }                           



void gf_mesher_object_get(getfemint::mexargs_in& m_in,
			    getfemint::mexargs_out& m_out) {
  typedef std::map<std::string, psub_command > SUBC_TAB;
  static SUBC_TAB subc_tab;

  if (subc_tab.size() == 0) {
  

    /*@GET s = ('char')
      Output a (unique) string representation of the @tmo.

      This can be used to perform comparisons between two
      different @tmo objects.
      This function is to be completed.
      @*/
    sub_command
      ("char", 0, 0, 0, 1,
       GMM_ASSERT1(false, "Sorry, function to be done");
       // std::string s = ...;
       // out.pop().from_string(s.c_str());
       );


    /*@GET ('display')
      displays a short summary for a @tmo object.@*/
    sub_command
      ("display", 0, 0, 0, 0,
       infomsg() << "gfMesherObject object\n";
       );


  }



  if (m_in.narg() < 2)  THROW_BADARG( "Wrong number of input arguments");

  getfem::mesher_signed_distance *paf = m_in.pop().to_mesher_object();
  std::string init_cmd   = m_in.pop().to_string();
  std::string cmd        = cmd_normalize(init_cmd);

  
  SUBC_TAB::iterator it = subc_tab.find(cmd);
  if (it != subc_tab.end()) {
    check_cmd(cmd, it->first.c_str(), m_in, m_out, it->second->arg_in_min,
	      it->second->arg_in_max, it->second->arg_out_min,
	      it->second->arg_out_max);
    it->second->run(m_in, m_out, paf);
  }
  else bad_cmd(init_cmd);

}