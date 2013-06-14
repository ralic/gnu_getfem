/*===========================================================================
 
 Copyright (C) 2006-2012 Yves Renard, Julien Pommier.
 
 This file is a part of GETFEM++
 
 Getfem++  is  free software;  you  can  redistribute  it  and/or modify it
 under  the  terms  of the  GNU  Lesser General Public License as published
 by  the  Free Software Foundation;  either version 3 of the License,  or
 (at your option) any later version along with the GCC Runtime Library
 Exception either version 3.1 or (at your option) any later version.
 This program  is  distributed  in  the  hope  that it will be useful,  but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or  FITNESS  FOR  A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 License and GCC Runtime Library Exception for more details.
 You  should  have received a copy of the GNU Lesser General Public License
 along  with  this program;  if not, write to the Free Software Foundation,
 Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
 
===========================================================================*/


#include <getfem/getfem_assembling.h>
#include <getfem/getfem_level_set.h>
#include <getfemint_misc.h>
#include <getfemint_gsparse.h>
#include <getfem/getfem_interpolation.h>
#include <getfem/getfem_nonlinear_elasticity.h>
#include <getfem/getfem_fourth_order.h>
#include <getfem/getfem_Coulomb_friction.h>

#ifdef GETFEM_HAVE_METIS 
 extern "C" void METIS_PartGraphKway(int *, int *, int *, int *, int *, int *,
 			    int *, int *, int *, int *, int *);
 extern "C" void METIS_PartGraphRecursive(int *, int *, int *, int *, int *, int *,
			    int *, int *, int *, int *, int *);

 extern "C" void METIS_mCPartGraphKway(int *, int *, int *, int *, int *, int *, int *,
				      int *, int *, float *, int *, int *, int *);
 extern "C" void METIS_mCPartGraphRecursive(int *, int *, int *, int *, int *, int *, int *,
				      int *, int *, int *, int *, int *);
#endif

/********************************/
/*compute level set unit normal*/

template<typename VECT1> class level_set_unit_normal 
  : public getfem::nonlinear_elem_term {
  const getfem::mesh_fem &mf;
  std::vector<bgeot::scalar_type> U;
  bgeot::size_type N;
  bgeot::base_matrix gradU;
  bgeot::base_vector coeff;
  bgeot::multi_index sizes_;
public:
  level_set_unit_normal(const getfem::mesh_fem &mf_, const VECT1 &U_) 
    : mf(mf_), U(mf_.nb_basic_dof()), N(mf_.linked_mesh().dim()),
      gradU(1, N) {
    sizes_.resize(1); sizes_[0] = bgeot::short_type(N);
    mf.extend_vector(U_, U);
  }
  const bgeot::multi_index &sizes(getfem::size_type) const {  return sizes_; }
  virtual void compute(getfem::fem_interpolation_context& ctx,
		       bgeot::base_tensor &t) {
    bgeot::size_type cv = ctx.convex_num();
    coeff.resize(mf.nb_basic_dof_of_element(cv));
    gmm::copy
      (gmm::sub_vector(U,gmm::sub_index(mf.ind_basic_dof_of_element(cv))),
       coeff);
    ctx.pf()->interpolation_grad(ctx, coeff, gradU, 1);
    bgeot::scalar_type norm = gmm::vect_norm2(gmm::mat_row(gradU, 0));
    for (bgeot::size_type i = 0; i < N; ++i) t[i] = gradU(0, i) / norm;
  }
};



 template<class MAT>
void asm_lsneuman_matrix
(const MAT &RM_, const getfem::mesh_im &mim, const getfem::mesh_fem &mf, 
 const getfem::mesh_fem &mf_mult, getfem::level_set &ls,
 const getfem::mesh_region &rg = getfem::mesh_region::all_convexes()) {
  MAT &RM = const_cast<MAT &>(RM_);

  level_set_unit_normal<std::vector<bgeot::scalar_type> >
    nterm(ls.get_mesh_fem(), ls.values());

  getfem::generic_assembly assem("t=comp(Base(#2).Grad(#1).NonLin(#3));"
				 "M(#2, #1)+= t(:,:,i,i)");
  assem.push_mi(mim);
  assem.push_mf(mf);
  assem.push_mf(mf_mult);
  assem.push_mf(ls.get_mesh_fem());
  assem.push_mat(RM);
  assem.push_nonlinear_term(&nterm);
  assem.assembly(rg);
}

 
    /** 
	generic normal grad level set matrix (on the whole mesh level set or on the specified
	convex set level set or boundary level set) 
	
    */
  

template<typename MAT>  void asm_nlsgrad_matrix
(const MAT &RM_, const getfem::mesh_im &mim, const getfem::mesh_fem &mf1,
 const getfem::mesh_fem &mf2,  getfem::level_set &ls,
 const getfem::mesh_region &rg = getfem::mesh_region::all_convexes()) {
  MAT &RM = const_cast<MAT &>(RM_);
  
  level_set_unit_normal<std::vector<bgeot::scalar_type> >
    nterm(ls.get_mesh_fem(), ls.values());
  
  getfem::generic_assembly
    assem("t=comp(Grad(#1).NonLin(#3).Grad(#2).NonLin(#3));"
	  "M(#1, #2)+= sym(t(:,i,i,:,j,j))");
  assem.push_mi(mim);
  assem.push_mf(mf1);
    assem.push_mf(mf2);
    assem.push_mf(ls.get_mesh_fem());
    assem.push_mat(RM);
    assem.push_nonlinear_term(&nterm);
    assem.assembly(rg);
  }


/**************************************************************/
/* assembling patch vector                                     */
/**************************************************************/

template<class VEC>
void asm_patch_vector     
(const VEC &RM_, const getfem::mesh_im &mim, const getfem::mesh_fem &mf_mult,
 const getfem::mesh_region &rg = getfem::mesh_region::all_convexes()) {
  VEC &RM = const_cast<VEC &>(RM_);
    
  getfem::generic_assembly assem("t=comp(Base(#1)); V(#1)+= t(:);");
  assem.push_mi(mim);
  assem.push_mf(mf_mult);
  assem.push_vec(RM);
  // assem.set("RM(#1)+=comp(Base(#1))()");
  assem.assembly(rg);

}
/**************************************************************/
/* assembling patch matrix                                     */
/**************************************************************/

template<class MAT>
void asm_stabilization_patch_matrix
(const MAT &RM_, const getfem::mesh &mesh, const getfem::mesh_fem &mf_mult, const getfem::mesh_im &mimbounddown,
 bgeot::scalar_type ratio_size, bgeot::scalar_type h ){
  MAT &M1 = const_cast<MAT &>(RM_);
  
  /****************************************************/
  /*        " select patch "                          */
  /****************************************************/
  
  
  
  // assemby patch vector
  const getfem::mesh_fem &mf_P0 = getfem::classical_mesh_fem(mesh, 0);
  bgeot::size_type nbe = mf_P0.nb_dof();
  int ne = 0;
  double size_of_crack = 0;
  getfem::modeling_standard_plain_vector Patch_Vector(nbe);
  asm_patch_vector(Patch_Vector, mimbounddown, mf_P0);
  // cout<<"patch_vectot="<< Patch_Vector<<endl;
  dal::bit_vector  Patch_element_list, Patch_dof_ind;
  for (bgeot::size_type i = 0; i < nbe; ++i) {
    if (Patch_Vector[i] != bgeot::scalar_type(0)){
      bgeot::size_type cv = mf_P0.first_convex_of_basic_dof(i);
      Patch_element_list.add(cv);
      Patch_dof_ind.add(i);
      ne++;
      size_of_crack=size_of_crack + Patch_Vector[i];
    }
  }
  // std::cout<<"Path_element_list="<< Patch_element_list << std::endl;
  // std::cout<<"Path_dof_ind="<< Patch_dof_ind << std::endl;
  std::cout << "number of element in patch=" << ne << std::endl;
  std::vector<int> xadj(ne+1), adjncy, numelt(ne), part(ne);
  std::vector<int> vwgt(ne), indelt(mesh.convex_index().last_true()+1);
  std::vector<double> vwgtt(ne);
  int j = 0, k = 0;
  for (dal::bv_visitor ic(Patch_element_list); !ic.finished(); ++ic, j++) {
    numelt[j] = int(ic);
    indelt[ic] = j;
  }
  j = 0;
  for (dal::bv_visitor ic(Patch_element_list); !ic.finished(); ++ic, j++) {
    bgeot::size_type ind_dof_patch = mf_P0.ind_basic_dof_of_element(ic)[0];
    vwgt[indelt[ic]] = int(1000000*Patch_Vector[ind_dof_patch]);
    vwgtt[indelt[ic]] = Patch_Vector[ind_dof_patch];
    xadj[j] = k;
    bgeot::mesh_structure::ind_set s;
    mesh.neighbours_of_convex(ic, s);
    for (bgeot::mesh_structure::ind_set::iterator it = s.begin(); it != s.end(); ++it) {
      if (Patch_element_list.is_in(*it)) { adjncy.push_back(indelt[*it]); ++k; }
    }
  }
  
  xadj[j] = k;
  std::vector<int> adjwgt(k);
  // std::cout<<"xadj="<<xadj<<std::endl;
  //std::cout<<"adjncy="<<adjncy<<std::endl;
  //std::cout<<"vwgt="<<vwgt<<std::endl;
  
  std::cout << "ratio size beween mesh and coarse mesh= " << ratio_size
	    << std::endl;
  
  int wgtflag = 2, edgecut, nparts=int(size_of_crack/(ratio_size*h)), numflag = 0;
      // float ubvec[1] = {1.03f};
  int  options[5] = {0,0,0,0,0};
  #ifdef GETFEM_HAVE_METIS 
  //METIS_mCPartGraphKway(&ne, &ncon, &(xadj[0]), &(adjncy[0]), &(vwgt[0]), &(adjwgt[0]), &wgtflag,
  //		    &numflag, &nparts, &(ubvec[0]),  options, &edgecut, &(part[0]));
  // METIS_mCPartGraphRecursive(&ne, &ncon, &(xadj[0]), &(adjncy[0]), &(vwgt[0]), &(adjwgt[0]), &wgtflag,
  //			 &numflag, &nparts,  options, &edgecut, &(part[0]));
  //METIS_PartGraphKway(&ne, &(xadj[0]), &(adjncy[0]), &(vwgt[0]), &(adjwgt[0]), &wgtflag,
  //	  &numflag, &nparts, options, &edgecut, &(part[0]));
  METIS_PartGraphRecursive(&ne, &(xadj[0]), &(adjncy[0]), &(vwgt[0]), &(adjwgt[0]), &wgtflag,
			   &numflag, &nparts, options, &edgecut, &(part[0]));
  #else
    GMM_ASSERT1(false, "Metis not present ...");
  #endif
  //std::cout<<"size_of_mesh="<<h<<std::endl;
  //std::cout<<"size_of_crack="<< size_of_crack <<std::endl;
  std::cout<<"nb_partition="<<nparts<<std::endl;
  // std::cout<<"partition="<<part<<std::endl;
  //std::cout<<"edgecut="<<edgecut<<std::endl;
  

  /**************************************************************/
  /*        Assembly matrices                                   */
  /**************************************************************/
  
  
  std::vector<double> size_patch(nparts);
  bgeot::size_type nb_dof_mult=mf_mult.nb_dof();
  getfem::modeling_standard_sparse_matrix M0(nb_dof_mult, nbe);
  getfem::asm_mass_matrix(M0, mimbounddown, mf_mult, mf_P0);
  
  for (bgeot::size_type i=0; i < bgeot::size_type(ne); i++) {
    size_patch[part[i]]= size_patch[part[i]] + vwgtt[i];	  
  }
  
  //std::cout<<"size_patch="<<size_patch<<std::endl;
  
  gmm::row_matrix<getfem::modeling_standard_sparse_vector> MAT_aux(nparts, nb_dof_mult);
  for (bgeot::size_type r=0; r < nbe; r++) {
    bgeot::size_type cv = mf_P0.first_convex_of_basic_dof(r);
    gmm::add(gmm::mat_col(M0, r), gmm::mat_row(MAT_aux, part[indelt[cv]]));
  }
  
  gmm::row_matrix<getfem::modeling_standard_sparse_vector> MAT_proj(nbe, nb_dof_mult);
  
  for (bgeot::size_type r=0; r < nbe; r++) {
    bgeot::size_type cv = mf_P0.first_convex_of_basic_dof(r);
    int p=part[indelt[cv]];
    gmm::copy(gmm::scaled(gmm::mat_row(MAT_aux, p), 1./size_patch[p]),
	      gmm::mat_row(MAT_proj, r));
  }
  
  gmm::mult(M0, MAT_proj, M1);
  
}


using namespace getfemint;
namespace getfemint {
  struct darray_with_gfi_array : public darray {
    gfi_array *mx;
    darray_with_gfi_array(const bgeot::tensor_ranges& r) {
      size_type siz = 1; for (size_type i=0; i < r.size(); ++i) siz *= r[i];
      if (siz == 0) ASM_THROW_TENSOR_ERROR("can't create a vector of size " << r);
      std::vector<int> tab(r.size());
      std::copy(r.begin(), r.end(), tab.begin());
      mx = checked_gfi_array_create(int(r.size()), &(tab.begin()[0]),GFI_DOUBLE);
      assign(mx);
    }
  };
}
namespace gmm {
  template<> struct linalg_traits<getfemint::darray_with_gfi_array> {
    typedef getfemint::darray_with_gfi_array this_type;
    typedef linalg_false is_reference;
    typedef abstract_vector linalg_type;
    typedef double value_type;
    typedef value_type origin_type;
    typedef double& reference;
    typedef this_type::iterator iterator;
    typedef this_type::const_iterator const_iterator;
    typedef abstract_dense storage_type;
    typedef linalg_true index_sorted;
    static size_type size(const this_type &v) { return v.size(); }
    static iterator begin(this_type &v) { return v.begin(); }
    static const_iterator begin(const this_type &v) { return v.begin(); }
    static iterator end(this_type &v) { return v.end(); }
    static const_iterator end(const this_type &v) { return v.end(); }
    static const origin_type* origin(const this_type &v) { return &v[0]; }
    static origin_type* origin(this_type &v) { return &v[0]; }
    static void clear(origin_type* , const iterator &it, const iterator &ite)
    { std::fill(it, ite, value_type(0)); }
    static void do_clear(this_type &v) { std::fill(v.begin(), v.end(), 0.); }
    static value_type access(const origin_type *, const const_iterator &it,
                             const const_iterator &, size_type i)
    { return it[i]; }
    static reference access(origin_type *, const iterator &it,
                            const iterator &, size_type i)
    { return it[i]; }
  };
}



namespace getfem {
  template<> class vec_factory<darray_with_gfi_array> :
    public base_vec_factory, private std::deque<asm_vec<getfemint::darray_with_gfi_array> > {
  public:
    base_asm_vec* create_vec(const tensor_ranges& r) {
      asm_vec<getfemint::darray_with_gfi_array> v(new getfemint::darray_with_gfi_array(r));
      push_back(v); return &this->back();
    }
    ~vec_factory() {
      for (size_type i=0; i < this->size(); ++i) {
        delete (*this)[i].vec(); // but it does not deallocate the gfi_array !! that's fine
      }
    }
  };
}

static void
do_generic_assembly(mexargs_in& in, mexargs_out& out, bool on_boundary)
{
  getfem::mesh_region rg = getfem::mesh_region::all_convexes();
  if (!on_boundary) {
    if (in.remaining() && !in.front().is_string()) {
      rg = in.pop().to_mesh_region();
    }
  } else rg = getfem::mesh_region(in.pop().to_integer());

  std::string s = in.pop().to_string();
  getfem::generic_assembly assem(s);
  /* stores the mesh_fem identifiers */
  while (in.remaining() && in.front().is_mesh_im()) {
    assem.push_mi(*in.pop().to_mesh_im());
  }
  if (assem.im().size() == 0)
    THROW_BADARG("generic assembly without any mesh_im has no sense!");
  /* stores the mesh_fem identifiers */
  while (in.remaining() && in.front().is_mesh_fem()) {
    assem.push_mf(*in.pop().to_const_mesh_fem());
  }

  /* store the data vectors */
  std::deque<darray> vtab;
  while (in.remaining()) {
    vtab.push_back(in.pop().to_darray());
  }
  /* DON't do that in the loop above, since push_back may invalidate every pointer on vtab[i] */
  for (size_type i=0; i < vtab.size(); ++i) assem.push_data(vtab[i]);

  /* set the kind of matrix/vector to build */
  getfem::mat_factory<gf_real_sparse_by_col> mat_fact;
  getfem::vec_factory<darray_with_gfi_array> vec_fact;
  assem.set_mat_factory(&mat_fact);
  assem.set_vec_factory(&vec_fact);

  assem.assembly(rg);
  // get the matrix back
  for (size_type i=0; out.remaining() && i < assem.mat().size(); ++i) {
    if (assem.mat()[i] != 0) {
      getfem::base_asm_mat *BM = assem.mat()[i];
      getfem::asm_mat<gf_real_sparse_by_col> * M =
        static_cast<getfem::asm_mat<gf_real_sparse_by_col>*>(BM);
      out.pop().from_sparse(*M->mat());
    }
  }

  if (out.remaining()) {
    for (size_type i=0; out.remaining() && i < assem.vec().size(); ++i) {
      if (assem.vec()[i] != 0) {
        getfem::base_asm_vec *BV = assem.vec()[i];
        getfem::asm_vec<darray_with_gfi_array> *V =
          static_cast<getfem::asm_vec<darray_with_gfi_array> *>(BV);
        mexarg_out mo = out.pop(); mo.arg = V->vec()->mx;
      }
    }
  }
}


template<typename T> static void
gf_dirichlet(getfemint::mexargs_out& out,
             const getfem::mesh_im &mim,
             const getfem::mesh_fem &mf_u,
             const getfem::mesh_fem &mf_d,
             mexarg_in in_h, mexarg_in in_r, int boundary_num, T)
{
  unsigned q_dim = mf_u.get_qdim();

  garray<T> h = in_h.to_garray(T());
  if (h.ndim() == 2) in_h.check_dimensions(h, q_dim* q_dim,int(mf_d.nb_dof()));
  else               in_h.check_dimensions(h, q_dim, q_dim,int(mf_d.nb_dof()));
  garray<T> r = in_r.to_garray(q_dim, int(mf_d.nb_dof()), T());
  gmm::col_matrix<gmm::wsvector<T> > H(mf_u.nb_dof(), mf_u.nb_dof());
  mexarg_out out_H    = out.pop();
  garray<T> R         = out.pop().create_array_v(unsigned(mf_u.nb_dof()), T());
  getfem::asm_generalized_dirichlet_constraints(H, R, mim, mf_u, mf_d, mf_d, h, r, boundary_num);
  out_H.from_sparse(H/*,threshold*/);
}

void interpolate_or_extrapolate(mexargs_in &in, mexargs_out &out, int extrapolate) {
  const getfem::mesh_fem *mf1 = in.pop().to_const_mesh_fem();
  const getfem::mesh_fem *mf2 = in.pop().to_const_mesh_fem();
  gmm::row_matrix<getfem::model_real_sparse_vector> Maux(mf2->nb_dof(), mf1->nb_dof());
  getfem::interpolation(*mf1, *mf2, Maux, extrapolate);
  gf_real_sparse_by_col M(mf2->nb_dof(), mf1->nb_dof());
  gmm::copy(Maux, M);
  out.pop().from_sparse(M);
}

static const getfem::mesh_im *get_mim(mexargs_in &in) {
  if (!in.front().is_mesh_im()) {
    THROW_BADARG("Since release 2.0 of getfem, all assembly functions"
                 " expect a mesh_im as their second argument");
  }
  return in.pop().to_const_mesh_im();
}

void assemble_source(size_type boundary_num,
                     mexargs_in &in, mexargs_out &out) {
  const getfem::mesh_im *mim = get_mim(in);
  const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
  const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
  unsigned q_dim = mf_u->get_qdim() / mf_d->get_qdim();
  if (!in.front().is_complex()) {
    
    darray g               = in.pop().to_darray(q_dim, int(mf_d->nb_dof()));
    darray F               = out.pop().create_darray_v(unsigned(mf_u->nb_dof()));
    getfem::asm_source_term(F, *mim, *mf_u, *mf_d, g, boundary_num);
  } else {
    carray g               = in.pop().to_carray(q_dim, int(mf_d->nb_dof()));
    carray F               = out.pop().create_carray_v(unsigned(mf_u->nb_dof()));
    getfem::asm_source_term(F, *mim, *mf_u, *mf_d, g, boundary_num);
  }
}
 
 
  
/*@GFDOC

  General assembly function.

  Many of the functions below use more than one mesh_fem: the main
  mesh_fem (mf_u) used for the main unknown, and data mesh_fem (mf_d)
  used for the data. It is always assumed that the Qdim of mf_d is
  equal to 1: if mf_d is used to describe vector or tensor data, you
  just have to "stack" (in fortran ordering) as many scalar fields as
  necessary.
@*/





// Object for the declaration of a new sub-command.

struct sub_gf_asm : virtual public dal::static_stored_object {
  int arg_in_min, arg_in_max, arg_out_min, arg_out_max;
  virtual void run(getfemint::mexargs_in& in,
                   getfemint::mexargs_out& out) = 0;
};

typedef boost::intrusive_ptr<sub_gf_asm> psub_command;

// Function to avoid warning in macro with unused arguments.
template <typename T> static inline void dummy_func(T &) {}

#define sub_command(name, arginmin, arginmax, argoutmin, argoutmax, code) { \
    struct subc : public sub_gf_asm {                                   \
      virtual void run(getfemint::mexargs_in& in,                       \
                       getfemint::mexargs_out& out)                     \
      { dummy_func(in); dummy_func(out); code }                         \
    };                                                                  \
    psub_command psubc = new subc;                                      \
    psubc->arg_in_min = arginmin; psubc->arg_in_max = arginmax;         \
    psubc->arg_out_min = argoutmin; psubc->arg_out_max = argoutmax;     \
    subc_tab[cmd_normalize(name)] = psubc;                              \
  }








void gf_asm(getfemint::mexargs_in& m_in, getfemint::mexargs_out& m_out) {
  typedef std::map<std::string, psub_command > SUBC_TAB;
  static SUBC_TAB subc_tab;

  if (subc_tab.size() == 0) {
    
    
    /*@FUNC M = ('mass matrix', @tmim mim, @tmf mf1[, @tmf mf2[, boundary_num]])
    Assembly of a mass matrix.
    
    Return a @tsp object.
    @*/
    sub_command
      ("mass matrix", 2, 4, 0, 1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u1 = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_u2 = in.remaining() ? in.pop().to_const_mesh_fem() : mf_u1;
       size_type nbound = in.remaining() ? in.pop().to_integer():size_type(-1);
       gf_real_sparse_by_col M(mf_u1->nb_dof(), mf_u2->nb_dof());
       getfem::asm_mass_matrix(M, *mim, *mf_u1, *mf_u2, nbound);
       out.pop().from_sparse(M);
       );
    
    /*@FUNC M = ('lsneuman matrix', @tmim mim, @tmf mf1, @tmf mf2, @tls ls)
      Assembly of a level set Neuman  matrix.
      
      Return a @tsp object.
      @*/
    sub_command
      ("lsneuman matrix", 3, 4, 0, 1, 
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u1 = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_u2 = in.pop().to_const_mesh_fem();
       getfem::level_set *ls1= in.pop().to_levelset();
       gf_real_sparse_by_col M(mf_u2->nb_dof(), mf_u1->nb_dof());
       asm_lsneuman_matrix(M, *mim, *mf_u1, *mf_u2, *ls1);
       out.pop().from_sparse(M);
       );
    
    /*@FUNC M = ('nlsgrad matrix', @tmim mim, @tmf mf1, @tmf mf2, @tls ls)
      Assembly of a nlsgrad matrix.
      
      Return a @tsp object.
      @*/
    sub_command
      ("nlsgrad matrix", 3, 4, 0, 1, 
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u1 = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_u2 = in.pop().to_const_mesh_fem();
       getfem::level_set *ls1= in.pop().to_levelset();
       gf_real_sparse_by_col M(mf_u1->nb_dof(), mf_u2->nb_dof());
       asm_nlsgrad_matrix(M, *mim, *mf_u1, *mf_u2, *ls1);
       out.pop().from_sparse(M);
       );
    
    /*@FUNC M = ('stabilization patch matrix', @tm mesh, @tmf mf,  @tmim mim, @real ratio, @real h)
      Assembly of stabilization patch matrix .
      
      Return a @tsp object.
      @*/
    sub_command
      ("stabilization patch matrix", 5, 5, 0, 1, 
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh *mesh = in.pop().to_const_mesh();
       const getfem::mesh_fem *mf_mult = in.pop().to_const_mesh_fem();
       double ratio_size= in.pop().to_scalar();
       double h= in.pop().to_scalar();
       gf_real_sparse_by_col M(mf_mult->nb_dof(), mf_mult->nb_dof());
       asm_stabilization_patch_matrix(M, *mesh,* mf_mult, *mim, ratio_size, h);
       out.pop().from_sparse(M);
       );
    
    
    /*@FUNC L = ('laplacian', @tmim mim, @tmf mf_u, @tmf mf_d, @dvec a)
      Assembly of the matrix for the Laplacian problem.
      
      :math:`\nabla\cdot(a(x)\nabla u)`  with `a` a scalar.
      
      Return a @tsp object.
      @*/
    sub_command
      ("laplacian", 4, 4,0, 1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       darray A               = in.pop().to_darray(int(mf_d->nb_dof()));
       gf_real_sparse_by_col M(mf_u->nb_dof(), mf_u->nb_dof());
       getfem::asm_stiffness_matrix_for_laplacian(M, *mim, *mf_u, *mf_d, A);
       out.pop().from_sparse(M);
       );

    /*@FUNC Le = ('linear elasticity', @tmim mim, @tmf mf_u, @tmf mf_d, @dvec lambda_d, @dvec mu_d)
    Assembles of the matrix for the linear (isotropic) elasticity problem.

    :math:`\nabla\cdot(C(x):\nabla u)`
    with :math:`C` defined via `lambda_d` and `mu_d`.
    
    Return a @tsp object.
    @*/
    sub_command
      ("linear elasticity", 5, 5, 0, 1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       darray lambda          = in.pop().to_darray(int(mf_d->nb_dof()));
       darray mu              = in.pop().to_darray(int(mf_d->nb_dof()));
       gf_real_sparse_by_col M(mf_u->nb_dof(), mf_u->nb_dof());
       getfem::asm_stiffness_matrix_for_linear_elasticity(M, *mim, *mf_u, *mf_d, lambda, mu);
       out.pop().from_sparse(M);
       );
    
    
    /*@FUNC TRHS = ('nonlinear elasticity', @tmim mim, @tmf mf_u, @dvec U, @str law, @tmf mf_d, @dmat params, {'tangent matrix'|'rhs'|'incompressible tangent matrix', @tmf mf_p, @dvec P|'incompressible rhs', @tmf mf_p, @dvec P})
      Assembles terms (tangent matrix and right hand side) for nonlinear elasticity.
      
      The solution `U` is required at the current time-step. The `law`
      may be choosen among:
      
      - 'SaintVenant Kirchhoff':
        Linearized law, should be avoided). This law has the two usual
        Lame coefficients as parameters, called lambda and mu.
      - 'Mooney Rivlin':
        This law has three parameters, called C1, C2 and D1.
        Can be preceded with the words 'compressible' or 'incompressible' to force
        a specific version. By default, the incompressible version is considered
        which requires only the first two material coefficients.
      - 'neo Hookean':
        A special case of the 'Mooney Rivlin' law that requires one material
        coefficient less (C2 = 0). By default, its compressible version is used.
      - 'Ciarlet Geymonat':
        This law has 3 parameters, called lambda, mu and gamma, with
        gamma chosen such that gamma is in ]-lambda/2-mu, -mu[.
      
    The parameters of the material law are described on the @tmf `mf_d`.
    The matrix `params` should have `nbdof(mf_d)` columns, each row
    correspounds to a parameter.

    The last argument selects what is to be built: either the tangent
    matrix, or the right hand side. If the incompressibility is
    considered, it should be followed by a @tmf `mf_p`, for the
    pression.

    Return a @tsp object (tangent matrix), @dcvec object (right hand
    side), tuple of @tsp objects (incompressible tangent matrix), or
    tuple of @dcvec objects (incompressible right hand side).
    @*/
    sub_command
      ("nonlinear elasticity", 3,-1,0,-1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       darray U = in.pop().to_darray(int(mf_u->nb_dof()));
       std::string lawname = in.pop().to_string();
       /* a refaire , pas bon, le terme incompressible se passe de loi */
       const getfem::abstract_hyperelastic_law &law
       = abstract_hyperelastic_law_from_name(lawname,
                                             mf_u->linked_mesh().dim());
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       darray param = in.pop().to_darray(int(law.nb_params()),
                                         int(mf_d->nb_dof()));
       while (in.remaining()) {
         std::string what = in.pop().to_string();
         if (cmd_strmatch(what, "tangent matrix")) {
           gf_real_sparse_by_col  K(mf_u->nb_dof(), mf_u->nb_dof());
           getfem::asm_nonlinear_elasticity_tangent_matrix(K, *mim, *mf_u, U,
                                                           mf_d, param, law);
           out.pop().from_sparse(K);
         } else if (cmd_strmatch(what, "rhs")) {
           darray B = out.pop().create_darray_v(unsigned(mf_u->nb_dof()));
           getfem::asm_nonlinear_elasticity_rhs(B, *mim, *mf_u, U, mf_d,
                                                param, law);
         } else if (cmd_strmatch(what, "incompressible tangent matrix")) {
           const getfem::mesh_fem *mf_p = in.pop().to_const_mesh_fem();
           darray P = in.pop().to_darray(int(mf_p->nb_dof()));
           gf_real_sparse_by_col  K(mf_u->nb_dof(), mf_u->nb_dof());
           gf_real_sparse_by_col  B(mf_u->nb_dof(), mf_p->nb_dof());
           getfem::asm_nonlinear_incomp_tangent_matrix(K, B, *mim, *mf_u,
                                                       *mf_p, U, P);
           out.pop().from_sparse(K);
           out.pop().from_sparse(B);
         } else if (cmd_strmatch(what, "incompressible rhs")) {
           const getfem::mesh_fem *mf_p = in.pop().to_const_mesh_fem();
           darray P = in.pop().to_darray(int(mf_p->nb_dof()));
           darray RU = out.pop().create_darray_v(unsigned(mf_u->nb_dof()));
           darray RB = out.pop().create_darray_v(unsigned(mf_p->nb_dof()));
           getfem::asm_nonlinear_incomp_rhs(RU, RB, *mim, *mf_u, *mf_p, U, P);
         } else {
           THROW_BADARG("expecting 'tangent matrix' or 'rhs', or "
                        "'incomp tangent matrix' or 'incomp rhs', got '"
                        << what << "'");
         }
       }
       if (in.remaining())
         THROW_BADARG("too much arguments for asm(nonlinear_elasticity)");
       );


    /*@FUNC @CELL{K, B} = ('stokes', @tmim mim, @tmf mf_u, @tmf mf_p, @tmf mf_d, @dvec nu)
    Assembly of matrices for the Stokes problem.

    :math:`-\nu(x)\Delta u + \nabla p = 0`
    :math:`\nabla\cdot u  = 0`
    with :math:`\nu` (`nu`), the fluid's dynamic viscosity.

    On output, `K` is the usual linear elasticity stiffness matrix with
    :math:`\lambda = 0` and
    :math:`2\mu = \nu`. `B` is a matrix
    corresponding to :math:`\int p\nabla\cdot\phi`.

    `K` and `B` are @tsp object's.
    @*/
    sub_command
      ("stokes", 5, 5, 0, 2,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_p = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       darray           vec_d = in.pop().to_darray(int(mf_d->nb_dof()));
       gf_real_sparse_by_col  K(mf_u->nb_dof(), mf_u->nb_dof());
       gf_real_sparse_by_col  B(mf_u->nb_dof(), mf_p->nb_dof());
       getfem::asm_stokes(K, B, *mim, *mf_u, *mf_p, *mf_d, vec_d);
       out.pop().from_sparse(K);
       out.pop().from_sparse(B);
       );


    /*@FUNC A = ('helmholtz', @tmim mim, @tmf mf_u, @tmf mf_d, @cvec k)
    Assembly of the matrix for the Helmholtz problem.

    :math:`\Delta u + k^2 u` = 0,  with `k` complex scalar.

    Return a @tsp object.
    @*/
    sub_command
      ("helmholtz", 4, 4, 0, 1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       carray           wn = in.pop().to_carray(int(mf_d->nb_dof()));
       std::vector<complex_type> WN(wn.size());
       for (size_type i=0; i < wn.size(); ++i) WN[i] = gmm::sqr(wn[i]);
       gf_cplx_sparse_by_col  A(mf_u->nb_dof(), mf_u->nb_dof());
       getfem::asm_Helmholtz(A, *mim, *mf_u, *mf_d, WN);
       out.pop().from_sparse(A);
       );


    /*@FUNC A = ('bilaplacian', @tmim mim, @tmf mf_u, @tmf mf_d, @dvec a)
      Assembly of the matrix for the Bilaplacian problem.

      :math:`\Delta(a(x)\Delta u) = 0`   with `a` scalar.

      Return a @tsp object.
      @*/
    sub_command
      ("bilaplacian", 4, 4, 0, 1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       darray           a = in.pop().to_darray(int(mf_d->nb_dof()));
       gf_real_sparse_by_col  A(mf_u->nb_dof(), mf_u->nb_dof());
       getfem::asm_stiffness_matrix_for_bilaplacian(A, *mim, *mf_u, *mf_d, a);
       out.pop().from_sparse(A);
       );

    /*@FUNC A = ('bilaplacian KL', @tmim mim, @tmf mf_u, @tmf mf_d, @dvec a, @dvec nu)
      Assembly of the matrix for the Bilaplacian problem with Kirchoff-Love formulation.

      :math:`\Delta(a(x)\Delta u) = 0`   with `a` scalar.

      Return a @tsp object.
      @*/
    sub_command
      ("bilaplacian KL", 5, 5, 0, 1,
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       darray           a = in.pop().to_darray(int(mf_d->nb_dof()));
       darray           nu = in.pop().to_darray(int(mf_d->nb_dof()));
       gf_real_sparse_by_col  A(mf_u->nb_dof(), mf_u->nb_dof());
       getfem::asm_stiffness_matrix_for_bilaplacian_KL(A, *mim, *mf_u, *mf_d, a, nu);
       out.pop().from_sparse(A);
       );


    /*@FUNC V = ('volumic source', @tmim mim, @tmf mf_u, @tmf mf_d, @dcvec fd)
    Assembly of a volumic source term.

    Output a vector `V`, assembled on the @tmf `mf_u`, using the data
    vector `fd` defined on the data @tmf `mf_d`. `fd` may be real or
    complex-valued.

    Return a @dcvec object.
    @*/
    sub_command
      ("volumic source", 4, 4, 1, 1,
       assemble_source(size_type(-1), in, out);
       );


    /*@FUNC B = ('boundary source', @int bnum, @tmim mim, @tmf mf_u, @tmf mf_d, @dvec G)
    Assembly of a boundary source term.

    `G` should be a [Qdim x N] matrix, where N is the number of dof
    of `mf_d`, and Qdim is the dimension of the unkown u (that is set
    when creating the @tmf).

    Return a @dcvec object.
    @*/
    sub_command
      ("boundary source", 5, 5, 0, 1,
       int boundary_num = in.pop().to_integer();
       assemble_source(boundary_num, in, out);
       );


    /*@FUNC @CELL{HH, RR} = ('dirichlet', @int bnum, @tmim mim, @tmf mf_u, @tmf mf_d, @dmat H, @dvec R [, threshold])
    Assembly of Dirichlet conditions of type `h.u = r`.

    Handle `h.u = r` where h is a square matrix (of any rank) whose
    size is equal to the dimension of the unkown u. This matrix is
    stored in `H`, one column per dof in `mf_d`, each column containing
    the values of the matrix h stored in fortran order:

    .. math::

      `H(:,j) = [h11(x_j) h21(x_j) h12(x_j) h22(x_j)]`

    if u is a 2D vector field.

    Of course, if the unknown is a scalar field, you just have to set
    `H = ones(1, N)`, where N is the number of dof of `mf_d`.

    This is basically the same than calling ::ASM('boundary qu term')
    for `H` and calling ::ASM('neumann') for `R`, except that this
    function tries to produce a 'better' (more diagonal) constraints
    matrix (when possible).

    See also SPMAT:GET('Dirichlet_nullspace').@*/
    sub_command
      ("dirichlet", 6, 7, 2, 2,
       int boundary_num       = in.pop().to_integer();
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();
       mexarg_in in_h = in.pop();
       mexarg_in in_r = in.pop();
       double threshold = 1e-8;
       if (in.remaining()) {
         threshold = in.pop().to_scalar();
         if (threshold < 0 || threshold > 1e10) THROW_BADARG("wrong threshold\n");
       }

       if (in_h.is_complex() || in_r.is_complex())
         gf_dirichlet(out, *mim, *mf_u, *mf_d, in_h, in_r, boundary_num, complex_type());
       else gf_dirichlet(out, *mim, *mf_u, *mf_d, in_h, in_r, boundary_num, scalar_type());
       );


    /*@FUNC Q = ('boundary qu term',@int boundary_num, @tmim mim, @tmf mf_u, @tmf mf_d, @dmat q)
    Assembly of a boundary qu term.

    `q` should be be a [Qdim x Qdim x N] array, where N is the number
    of dof of `mf_d`, and Qdim is the dimension of the unkown u (that
    is set when creating the @tmf).

    Return a @tsp object.
    @*/
    sub_command
      ("boundary qu term", 5, 5, 0, 1,
       int boundary_num       = in.pop().to_integer();
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_d = in.pop().to_const_mesh_fem();

       unsigned q_dim = mf_u->get_qdim();
       if (!in.front().is_complex()) {
         darray q            = in.pop().to_darray();
         if (q.ndim() == 2) in.last_popped().check_dimensions(q, q_dim* q_dim, int(mf_d->nb_dof()));
         else               in.last_popped().check_dimensions(q, q_dim, q_dim, int(mf_d->nb_dof()));
         gf_real_sparse_by_col Q(mf_u->nb_dof(), mf_u->nb_dof());
         getfem::asm_qu_term(Q, *mim, *mf_u, *mf_d, q, boundary_num);
         out.pop().from_sparse(Q);
       } else {
         carray q            = in.pop().to_carray();
         if (q.ndim() == 2) in.last_popped().check_dimensions(q, q_dim* q_dim, int(mf_d->nb_dof()));
         else               in.last_popped().check_dimensions(q, q_dim, q_dim, int(mf_d->nb_dof()));
         gf_cplx_sparse_by_col Q(mf_u->nb_dof(), mf_u->nb_dof());
         getfem::asm_qu_term(Q, *mim, *mf_u, *mf_d, q, boundary_num);
         out.pop().from_sparse(Q);
       }
       );


    /*@FUNC @CELL{...} = ('volumic' [,CVLST], expr [, mesh_ims, mesh_fems, data...])
      Generic assembly procedure for volumic assembly.

      The expression `expr` is evaluated over the @tmf's listed in the
      arguments (with optional data) and assigned to the output arguments.
      For details about the syntax of assembly expressions, please refer
      to the getfem user manual (or look at the file getfem_assembling.h
      in the getfem++ sources).

      For example, the L2 norm of a field can be computed with::

        ::COMPUTE('L2 norm') or with:

        ::ASM('volumic','u=data(#1); V()+=u(i).u(j).comp(Base(#1).Base(#1))(i,j)',mim,mf,U)

      The Laplacian stiffness matrix can be evaluated with::

        ::ASM('laplacian',mim, mf, A) or equivalently with:

        ::ASM('volumic','a=data(#2);M(#1,#1)+=sym(comp(Grad(#1).Grad(#1).Base(#2))(:,i,:,i,j).a(j))', mim,mf, A);@*/
    sub_command
      ("volumic", 2, -1, 0, -1,
       do_generic_assembly(in, out, false);
       );


    /*@FUNC @CELL{...} = ('boundary', @int bnum, @str expr [, @tmim mim, @tmf mf, data...])
      Generic boundary assembly.

      See the help for ::ASM('volumic').@*/
    sub_command
      ("boundary", 3, -1, 0, -1,
       do_generic_assembly(in, out, true);
       );


    /*@FUNC Mi = ('interpolation matrix', @tmf mf, @tmf mfi)
    Build the interpolation matrix from a @tmf onto another @tmf.

    Return a matrix `Mi`, such that `V = Mi.U` is equal to
    ::COMPUTE('interpolate_on',mfi). Useful for repeated interpolations.
    Note that this is just interpolation, no elementary integrations
    are involved here, and `mfi` has to be lagrangian. In the more
    general case, you would have to do a L2 projection via the mass
    matrix.

    `Mi` is a @tsp object.
    @*/
    sub_command
      ("interpolation matrix", 2, 2, 0, 1,
       interpolate_or_extrapolate(in, out, 0);
       );


    /*@FUNC Me = ('extrapolation matrix',@tmf mf, @tmf mfe)
    Build the extrapolation matrix from a @tmf onto another @tmf.

    Return a matrix `Me`, such that `V = Me.U` is equal to
    ::COMPUTE('extrapolate_on',mfe). Useful for repeated
    extrapolations.

    `Me` is a @tsp object.
    @*/
    sub_command
      ("extrapolation matrix", 2, 2, 0, 1,
       interpolate_or_extrapolate(in, out, 2);
       );


    /*@FUNC B = ('integral contact Uzawa projection', @int bnum, @tmim mim, @tmf mf_u, @dvec U, @tmf mf_lambda, @dvec vec_lambda, @tmf mf_obstacle, @dvec obstacle, @scalar r [, {@scalar coeff | @tmf mf_coeff, @dvec coeff} [, @int option[, @scalar alpha, @dvec W]]])
    Specific assembly procedure for the use of an Uzawa algorithm to solve
      contact problems. Projects the term $-(\lambda - r (u_N-g))_-$ on the
      finite element space of $\lambda$.

    Return a @dcvec object.
    @*/
    sub_command
      ("integral contact Uzawa projection", 9, 13, 0, 1,
       int boundary_num = in.pop().to_integer();
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       // unsigned q_dim = mf_u->get_qdim();
       darray u = in.pop().to_darray();
       in.last_popped().check_trailing_dimension(int(mf_u->nb_dof()));
       const getfem::mesh_fem *mf_lambda = in.pop().to_const_mesh_fem();
       darray vec_lambda = in.pop().to_darray();
       in.last_popped().check_trailing_dimension(int(mf_lambda->nb_dof()));
       const getfem::mesh_fem *mf_obs = in.pop().to_const_mesh_fem();
       darray obs = in.pop().to_darray();
       in.last_popped().check_trailing_dimension(int(mf_obs->nb_dof()));
       double r = in.pop().to_scalar();

       darray F = out.pop().create_darray_v(unsigned(mf_lambda->nb_dof()));
       if (in.remaining()) { // with friction
           const getfem::mesh_fem *mf_coeff = 0;
           darray vec_coeff;
           mexarg_in argin = in.pop();
           if (argin.is_mesh_fem()) {
             mf_coeff = argin.to_const_mesh_fem();
             vec_coeff = in.pop().to_darray();
             in.last_popped().check_trailing_dimension(int(mf_coeff->nb_dof()));
           } else
             vec_coeff = darray(new double(argin.to_scalar()), 1);
           int option = in.remaining() ? in.pop().to_integer() : 1;
           double alpha =  in.remaining() ? in.pop().to_scalar() : 1;
           darray vec_W;
           if (in.remaining()) {
             vec_W = in.pop().to_darray();
             in.last_popped().check_trailing_dimension(int(mf_u->nb_dof()));
           }
           getfem::asm_integral_contact_Uzawa_proj
             (F, *mim, *mf_u, u, *mf_obs, obs, *mf_lambda, vec_lambda,
              mf_coeff, vec_coeff, &vec_W, r, alpha, boundary_num, option);
       } else // without friction
           getfem::asm_integral_contact_Uzawa_proj
             (F, *mim, *mf_u, u, *mf_obs, obs, *mf_lambda, vec_lambda,
              r, boundary_num);
       );


    /*@FUNC B = ('level set normal source term', @int bnum, @tmim mim, @tmf mf_u, @tmf mf_lambda, @dvec vec_lambda, @tmf mf_levelset, @dvec levelset)
    Performs an assembly of the source term represented by `vec_lambda`
    on `mf_lambda` considered to be a component in the direction of the
    gradient of a levelset function (normal to the levelset) of a vector
    field defined on `mf_u` on the boundary `bnum`.

    Return a @dcvec object.
    @*/
    sub_command
      ("level set normal source term", 7, 7, 0, 1,
       int boundary_num = in.pop().to_integer();
       const getfem::mesh_im *mim = get_mim(in);
       const getfem::mesh_fem *mf_u = in.pop().to_const_mesh_fem();
       const getfem::mesh_fem *mf_lambda = in.pop().to_const_mesh_fem();
       darray vec_lambda = in.pop().to_darray();
       in.last_popped().check_trailing_dimension(int(mf_lambda->nb_dof()));
       const getfem::mesh_fem *mf_obs = in.pop().to_const_mesh_fem();
       darray obs = in.pop().to_darray();
       in.last_popped().check_trailing_dimension(int(mf_obs->nb_dof()));
       darray F = out.pop().create_darray_v(unsigned(mf_u->nb_dof()));
       getfem::asm_level_set_normal_source_term
         (F, *mim, *mf_u, *mf_obs, obs, *mf_lambda, vec_lambda, boundary_num);
       
       );

  }

  if (m_in.narg() < 1)  THROW_BADARG( "Wrong number of input arguments");

  std::string init_cmd   = m_in.pop().to_string();
  std::string cmd        = cmd_normalize(init_cmd);

  SUBC_TAB::iterator it = subc_tab.find(cmd);
  if (it != subc_tab.end()) {
    check_cmd(cmd, it->first.c_str(), m_in, m_out, it->second->arg_in_min,
              it->second->arg_in_max, it->second->arg_out_min,
              it->second->arg_out_max);
    it->second->run(m_in, m_out);
  }
  else bad_cmd(init_cmd);

}

/*@MATLABEXT
  if (nargin>=1 & strcmpi(varargin{1},'pdetool boundary conditions')),
    [varargout{1:nargout}]=gf_asm_pdetoolbc(varargin{[1 3:nargin]}); return;
  end;
  @*/
/*@MATLABFUNC @CELL{Q, G, H, R, F} = ('pdetool boundary conditions', mf_u, mf_d, b, e[, f_expr])
  Assembly of pdetool boundary conditions.

  `B` is the boundary matrix exported by pdetool, and `E` is the
  edges array. `f_expr` is an optionnal expression (or vector) for
  the volumic term. On return `Q, G, H, R, F` contain the assembled
  boundary conditions (`Q` and `H` are matrices), similar to the
  ones returned by the function ASSEMB from PDETOOL.
  @*/
