// -*- c++ -*- (enables emacs c++ mode)
//===========================================================================
//
// Copyright (C) 2002-2008 Yves Renard, Julien Pommier.
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
  
/**
 * Linear Elastostatic problem with a crack.
 *
 * This program is used to check that getfem++ is working. This is also 
 * a good example of use of Getfem++.
 */

#include "getfem/getfem_assembling.h" /* import assembly methods (and norms comp.) */
#include "getfem/getfem_export.h"   /* export functions (save solution in a file)  */
#include "getfem/getfem_derivatives.h"
#include "getfem/getfem_regular_meshes.h"
#include "getfem/getfem_model_solvers.h"
#include "getfem/getfem_mesh_im_level_set.h"
#include "getfem/getfem_mesh_fem_level_set.h"
#include "getfem/getfem_mesh_fem_product.h"
#include "getfem/getfem_mesh_fem_global_function.h"
#include "getfem/getfem_spider_fem.h"
#include "getfem/getfem_mesh_fem_sum.h"
#include "getfem/getfem_crack_sif.h"
#include "gmm/gmm.h"
#include "gmm/gmm_inoutput.h"

/* some Getfem++ types that we will be using */
using bgeot::base_small_vector; /* special class for small (dim<16) vectors */
using bgeot::base_node;  /* geometrical nodes(derived from base_small_vector)*/
using bgeot::scalar_type; /* = double */
using bgeot::size_type;   /* = unsigned long */
using bgeot::dim_type; 
using bgeot::short_type; 
using bgeot::base_matrix; /* small dense matrix. */

/* definition of some matrix/vector types. These ones are built
 * using the predefined types in Gmm++
 */
typedef getfem::modeling_standard_sparse_vector sparse_vector;
typedef getfem::modeling_standard_sparse_matrix sparse_matrix;
typedef getfem::modeling_standard_plain_vector  plain_vector;
typedef gmm::dense_matrix<scalar_type>  dense_matrix;

/**************************************************************************/
/*  Exact solution.                                                       */
/**************************************************************************/

#define VALIDATE_XFEM

#ifdef VALIDATE_XFEM

/* returns sin(theta/2) where theta is the angle
   of 0-(x,y) with the axis Ox */
scalar_type sint2(scalar_type x, scalar_type y) {
  scalar_type r = sqrt(x*x+y*y);
  if (r == 0) return 0;
  else return (y<0 ? -1:1) * sqrt(gmm::abs(r-x)/(2*r));
  // sometimes (gcc3.3.2 -O3), r-x < 0 ....
}
scalar_type cost2(scalar_type x, scalar_type y) {
  scalar_type r = sqrt(x*x+y*y);
  if (r == 0) return 0;
  else return sqrt(gmm::abs(r+x)/(2*r));
}
/* analytical solution for a semi-infinite crack [-inf,a] in an
   infinite plane submitted to +sigma above the crack
   and -sigma under the crack. (The crack is directed along the x axis).
   
   nu and E are the poisson ratio and young modulus
   
   solution taken from "an extended finite elt method with high order
   elts for curved cracks", Stazi, Budyn,Chessa, Belytschko
*/

void elasticite2lame(const scalar_type young_modulus,
		     const scalar_type poisson_ratio, 
		     scalar_type& lambda, scalar_type& mu) {
  mu = young_modulus/(2*(1+poisson_ratio));
  lambda = 2*mu*poisson_ratio/(1-poisson_ratio);
}

/* plane stress */
scalar_type young_modulus(scalar_type lambda, scalar_type mu){
  return 4*mu*(lambda + mu)/(lambda+2*mu);
}

void sol_ref_infinite_plane(scalar_type nu, scalar_type E, scalar_type sigma,
			    scalar_type a, scalar_type xx, scalar_type y,
			    base_small_vector& U, int mode,
			    base_matrix *pgrad) {
  scalar_type x  = xx-a; /* the eq are given relatively to the crack tip */
  //scalar_type KI = sigma*sqrt(M_PI*a);
  scalar_type r = std::max(sqrt(x*x+y*y),1e-16);
  scalar_type sqrtr = sqrt(r), sqrtr3 = sqrtr*sqrtr*sqrtr;
  scalar_type cost = x/r, sint = y/r;
  scalar_type theta = atan2(y,x);
  scalar_type s2 = sin(theta/2); //sint2(x,y);
  scalar_type c2 = cos(theta/2); //cost2(x,y);
  // scalar_type c3 = cos(3*theta/2); //4*c2*c2*c2-3*c2; /* cos(3*theta/2) */
  // scalar_type s3 = sin(3*theta/2); //4*s2*c2*c2-s2;  /* sin(3*theta/2) */

  scalar_type lambda, mu;
  elasticite2lame(E,nu,lambda,mu);

  U.resize(2);
  if (pgrad) (*pgrad).resize(2,2);
  scalar_type C= 1./E * (mode == 1 ? 1. : (1+nu));
  if (mode == 1) {
    scalar_type A=2+2*mu/(lambda+2*mu);
    scalar_type B=-2*(lambda+mu)/(lambda+2*mu);
    U[0] = sqrtr/sqrt(2*M_PI) * C * c2 * (A + B*cost);
    U[1] = sqrtr/sqrt(2*M_PI) * C * s2 * (A + B*cost);
    if (pgrad) {
      (*pgrad)(0,0) = C/(2.*sqrt(2*M_PI)*sqrtr)
	* (cost*c2*A-cost*cost*c2*B+sint*s2*A+sint*s2*B*cost+2*c2*B);
      (*pgrad)(1,0) = -C/(2*sqrt(2*M_PI)*sqrtr)
	* (-sint*c2*A+sint*c2*B*cost+cost*s2*A+cost*cost*s2*B);
      (*pgrad)(0,1) = C/(2.*sqrt(2*M_PI)*sqrtr)
	* (cost*s2*A-cost*cost*s2*B-sint*c2*A-sint*c2*B*cost+2*s2*B);
      (*pgrad)(1,1) = C/(2.*sqrt(2*M_PI)*sqrtr)
	* (sint*s2*A-sint*s2*B*cost+cost*c2*A+cost*cost*c2*B);
    }
  } else if (mode == 2) {
    scalar_type C1 = (lambda+3*mu)/(lambda+mu);
    U[0] = sqrtr/sqrt(2*M_PI) * C * s2 * (C1 + 2 + cost);
    U[1] = sqrtr/sqrt(2*M_PI) * C * c2 * (C1 - 2 + cost) * (-1.);
    if (pgrad) {
      (*pgrad)(0,0) = C/(2.*sqrt(2*M_PI)*sqrtr)
	* (cost*s2*C1+2*cost*s2-cost*cost*s2-sint*c2*C1
	   -2*sint*c2-sint*cost*c2+2*s2);
      (*pgrad)(1,0) = C/(2.*sqrt(2*M_PI)*sqrtr)
	* (sint*s2*C1+2*sint*s2-sint*s2*cost+cost*c2*C1
	   +2*cost*c2+cost*cost*c2);
      (*pgrad)(0,1) = -C/(2.*sqrt(2*M_PI)*sqrtr)
	* (cost*c2*C1-2*cost*c2-cost*cost*c2+sint*s2*C1
	   -2*sint*s2+sint*s2*cost+2*c2);
      (*pgrad)(1,1) =  C/(2.*sqrt(2*M_PI)*sqrtr)
	* (-sint*c2*C1+2*sint*c2+sint*cost*c2+cost*s2*C1
	   -2*cost*s2+cost*cost*s2);
    }
  } else if (mode == 100) {
    U[0] = - sqrtr3 * (c2 + 4./3 *(7*mu+3*lambda)/(lambda+mu)*c2*s2*s2
		       -1./3*(7*mu+3*lambda)/(lambda+mu)*c2);
    U[1] = - sqrtr3 * (s2+4./3*(lambda+5*mu)/(lambda+mu)*s2*s2*s2
		       -(lambda+5*mu)/(lambda+mu)*s2);
    if (pgrad) {
      (*pgrad)(0,0) = 2*sqrtr*(-6*cost*c2*mu+7*cost*c2*c2*c2*mu
			       -3*cost*c2*lambda+3*cost*c2*c2*c2*lambda
			       -2*sint*s2*mu
			       +7*sint*s2*c2*c2*mu-sint*s2*lambda
			       +3*sint*s2*c2*c2*lambda)/(lambda+mu);
      (*pgrad)(1,0) = -2*sqrtr*(6*sint*c2*mu-7*sint*c2*c2*c2*mu
				+3*sint*c2*lambda-3*sint*c2*c2*c2*lambda
				-2*cost*s2*mu
				+7*cost*s2*c2*c2*mu-cost*s2*lambda
				+3*cost*s2*c2*c2*lambda)/(lambda+mu);
      (*pgrad)(0,1) = 2*sqrtr*(-2*cost*s2*mu-cost*s2*lambda
			       +cost*s2*c2*c2*lambda+5*cost*s2*c2*c2*mu
			       +4*sint*c2*mu
			       +sint*c2*lambda-sint*c2*c2*c2*lambda
			       -5*sint*c2*c2*c2*mu)/(lambda+mu);
      (*pgrad)(1,1) = 2*sqrtr*(-2*sint*s2*mu-sint*s2*lambda
			       +sint*s2*c2*c2*lambda+5*sint*s2*c2*c2*mu
			       -4*cost*c2*mu
			       -cost*c2*lambda+cost*c2*c2*c2*lambda
			       +5*cost*c2*c2*c2*mu)/(lambda+mu);
    }
  } else if (mode == 101) {
    U[0] = -4*sqrtr3*s2*(-lambda-2*mu+7*lambda*c2*c2
			 +11*mu*c2*c2)/(3*lambda-mu);
    U[1] = -4*sqrtr3*c2*(-3*lambda+3*lambda*c2*c2-mu*c2*c2)/(3*lambda-mu);
    if (pgrad) {
      (*pgrad)(0,0) = -6*sqrtr*(-cost*s2*lambda-2*cost*s2*mu
				+7*cost*s2*lambda*c2*c2
				+11*cost*s2*mu*c2*c2+5*sint*c2*lambda
				+8*sint*c2*mu-7*sint*c2*c2*c2*lambda
				-11*sint*c2*c2*c2*mu)/(3*lambda-mu);
      (*pgrad)(1,0) = -6*sqrtr*(-sint*s2*lambda-2*sint*s2*mu
				+7*sint*s2*lambda*c2*c2
				+11*sint*s2*mu*c2*c2-5*cost*c2*lambda
				-8*cost*c2*mu+7*cost*c2*c2*c2*lambda
				+11*cost*c2*c2*c2*mu)/(3*lambda-mu);
      (*pgrad)(0,1) = -6*sqrtr*(-3*cost*c2*lambda+3*cost*c2*c2*c2*lambda
				-cost*c2*c2*c2*mu-sint*s2*lambda
				+3*sint*s2*lambda*c2*c2
				-sint*s2*mu*c2*c2)/(3*lambda-mu);
      (*pgrad)(1,1) = 6*sqrtr*(3*sint*c2*lambda
			       -3*sint*c2*c2*c2*lambda+sint*c2*c2*c2*mu
			       -cost*s2*lambda+3*cost*s2*lambda*c2*c2
			       -cost*s2*mu*c2*c2)/(3*lambda-mu);
    }

  } else if (mode == 10166666) {

    U[0] = 4*sqrtr3*s2*(-lambda+lambda*c2*c2-3*mu*c2*c2)/(lambda-3*mu);
    U[1] = 4*sqrtr3*c2*(-3*lambda-6*mu+5*lambda*c2*c2+9*mu*c2*c2)/(lambda-3*mu);
    if (pgrad) {
      (*pgrad)(0,0) = 6*sqrtr*(-cost*s2*lambda+cost*s2*lambda*c2*c2-
			       3*cost*s2*mu*c2*c2-2*sint*c2*mu+sint*c2*lambda-
			       sint*c2*c2*c2*lambda
			       +3*sint*c2*c2*c2*mu)/(lambda-3*mu);
      (*pgrad)(1,0) = 6*sqrtr*(-sint*s2*lambda+sint*s2*lambda*c2*c2-
			       3*sint*s2*mu*c2*c2+2*cost*c2*mu-cost*c2*lambda+
			       cost*c2*c2*c2*lambda
			       -3*cost*c2*c2*c2*mu)/(lambda-3*mu);
      (*pgrad)(0,1) = 6*sqrtr*(-3*cost*c2*lambda-6*cost*c2*mu
			       +5*cost*c2*c2*c2*lambda+
			       9*cost*c2*c2*c2*mu-sint*s2*lambda-2*sint*s2*mu+
			       5*sint*s2*lambda*c2*c2
			       +9*sint*s2*mu*c2*c2)/(lambda-3*mu);
      (*pgrad)(1,1) = -6*sqrtr*(3*sint*c2*lambda+6*sint*c2*mu
				-5*sint*c2*c2*c2*lambda-
				9*sint*c2*c2*c2*mu-cost*s2*lambda-2*cost*s2*mu+
				5*cost*s2*lambda*c2*c2
				+9*cost*s2*mu*c2*c2)/(lambda-3*mu);
    }
  } else GMM_ASSERT1(false, "Unvalid mode");
  if (isnan(U[0]))
    cerr << "raaah not a number ... nu=" << nu << ", E=" << E << ", sig="
	 << sigma << ", a=" << a << ", xx=" << xx << ", y=" << y << ", r="
	 << r << ", sqrtr=" << sqrtr << ", cost=" << cost << ", U=" << U[0]
	 << "," << U[1] << endl;
  assert(!isnan(U[0]));
  assert(!isnan(U[1]));
}

struct exact_solution {
  getfem::mesh_fem_global_function mf;
  getfem::base_vector U;

  exact_solution(getfem::mesh &me) : mf(me) {}
  
  void init(int mode, scalar_type lambda, scalar_type mu,
	    getfem::level_set &ls) {
    std::vector<getfem::pglobal_function> cfun(4);
    for (unsigned j=0; j < 4; ++j) {
      getfem::crack_singular_xy_function *s = 
	new getfem::crack_singular_xy_function(j);
      cfun[j] = getfem::global_function_on_level_set(ls, *s);
    }

    mf.set_functions(cfun);
    
    mf.set_qdim(1);
    
    U.resize(8); assert(mf.nb_dof() == 4);
    getfem::base_vector::iterator it = U.begin();
    scalar_type coeff=0.;
    switch(mode) {
      case 1: {
	scalar_type A=2+2*mu/(lambda+2*mu), B=-2*(lambda+mu)/(lambda+2*mu);
	/* "colonne" 1: ux, colonne 2: uy */
	*it++ = 0;       *it++ = A-B; /* sin(theta/2) */
	*it++ = A+B;     *it++ = 0;   /* cos(theta/2) */
	*it++ = -B;      *it++ = 0;   /* sin(theta/2)*sin(theta) */ 
	*it++ = 0;       *it++ = B;   /* cos(theta/2)*cos(theta) */
	coeff = 1/sqrt(2*M_PI);
      } break;
      case 2: {
	scalar_type C1 = (lambda+3*mu)/(lambda+mu); 
	*it++ = C1+2-1;   *it++ = 0;
	*it++ = 0;      *it++ = -(C1-2+1);
	*it++ = 0;      *it++ = 1;
	*it++ = 1;      *it++ = 0;
	coeff = 2*(mu+lambda)/(lambda+2*mu)/sqrt(2*M_PI);
      } break;
      default:
	GMM_ASSERT1(false, "Unvalid mode");
	break;
    }
    gmm::scale(U, coeff/young_modulus(lambda,mu));
  }
};

base_small_vector sol_f(const base_node &x) {
  int N = x.size();
  base_small_vector res(N);
  return res;
}

#else

base_small_vector sol_f(const base_node &x) {
  int N = x.size();
  base_small_vector res(N); res[N-1] = x[N-1];
  return res;
}

#endif

/**************************************************************************/
/*  Structure for the crack problem.                                      */
/**************************************************************************/

struct crack_problem {

  enum { DIRICHLET_BOUNDARY_NUM = 0, NEUMANN_BOUNDARY_NUM = 1, MORTAR_BOUNDARY_IN=42, MORTAR_BOUNDARY_OUT=43 };
  getfem::mesh mesh;  /* the mesh */
  getfem::mesh_level_set mls;       /* the integration methods.              */
  getfem::mesh_im_level_set mim;    /* the integration methods.              */
  getfem::mesh_fem mf_pre_u, mf_pre_mortar;
  getfem::mesh_fem mf_mult;
  getfem::mesh_fem_level_set mfls_u, mfls_mortar; 
  getfem::mesh_fem_global_function mf_sing_u;
  getfem::mesh_fem mf_partition_of_unity;
  getfem::mesh_fem_product mf_product;
  getfem::mesh_fem_sum mf_u_sum;
  getfem::mesh_fem mf_us;

  getfem::mesh_fem& mf_u() { return mf_u_sum; }
  
  scalar_type lambda, mu;    /* Lame coefficients.                */
  getfem::mesh_fem mf_rhs;   /* mesh_fem for the right hand side (f(x),..)   */
  getfem::mesh_fem mf_p;     /* mesh_fem for the pressure for mixed form     */
#ifdef VALIDATE_XFEM
  exact_solution exact_sol;
#endif
  
  getfem::level_set ls;      /* The two level sets defining the crack.       */
  getfem::level_set ls2, ls3; /* The two level-sets defining the add. cracks.*/

  dal::bit_vector pm_convexes; /* convexes inside the enrichment 
				  area when point-wise matching is used.*/

  base_small_vector translation;

  struct spider_param {
    getfem::spider_fem *fem;
    scalar_type theta0;
    scalar_type radius;
    unsigned Nr;
    unsigned Ntheta;
    int K;
  };
  spider_param spider;

  scalar_type residual;      /* max residual for the iterative solvers      */
  bool mixed_pressure, add_crack;
  unsigned dir_with_mult;
  int mode;
  size_type ind_first_global_dof;
  
  scalar_type enr_area_radius;
  struct cutoff_param {
    scalar_type radius, radius1, radius0;
    size_type fun_num;
  };
  cutoff_param cutoff;

  typedef enum { NO_ENRICHMENT=0, 
		 FIXED_ZONE=1, 
		 GLOBAL_WITH_MORTAR=2,
		 GLOBAL_WITH_CUTOFF=3,
		 SPIDER_FEM_ALONE=4,
		 SPIDER_FEM_ENRICHMENT=5 } enrichment_option_enum;
  enrichment_option_enum enrichment_option;
  bool vectorial_enrichment;
  dense_matrix Qsing;

  std::string datafilename;
  
  std::string GLOBAL_FUNCTION_MF, GLOBAL_FUNCTION_U;

  bgeot::md_param PARAM;

  bool solve(plain_vector &U);
  void compute_sif(const plain_vector &U);
  void init(void);
  crack_problem(void) : mls(mesh), mim(mls), 
			mf_pre_u(mesh), mf_pre_mortar(mesh), mf_mult(mesh),
			mfls_u(mls, mf_pre_u), mfls_mortar(mls, mf_pre_mortar),
			mf_sing_u(mesh),
			mf_partition_of_unity(mesh),
			mf_product(mf_partition_of_unity, mf_sing_u),

			mf_u_sum(mesh), mf_us(mesh), mf_rhs(mesh), mf_p(mesh),
#ifdef VALIDATE_XFEM
			exact_sol(mesh), 
#endif
			ls(mesh, 1, true), ls2(mesh, 1, true),
			ls3(mesh, 1, true), Qsing(8,8) {}

};


std::string name_of_dof(getfem::pdof_description dof) {
  char s[200];
  sprintf(s, "UnknownDof[%p]", (void*)dof);
  for (dim_type d = 0; d < 4; ++d) {
    if (dof == getfem::lagrange_dof(d)) {
      sprintf(s, "Lagrange[%d]", d); goto found;
    }
    if (dof == getfem::normal_derivative_dof(d)) {
      sprintf(s, "D_n[%d]", d); goto found;
    }
    if (dof == getfem::global_dof(d)) {
      sprintf(s, "GlobalDof[%d]", d);
    }
    if (dof == getfem::mean_value_dof(d)) {
      sprintf(s, "MeanValue[%d]", d);
    }
    if (getfem::dof_xfem_index(dof) != 0) {
      sprintf(s, "Xfem[idx:%d]", int(dof_xfem_index(dof)));
    }
    
    for (dim_type r = 0; r < d; ++r) {
      if (dof == getfem::derivative_dof(d, r)) {
	sprintf(s, "D_%c[%d]", "xyzuvw"[r], d); goto found;
      }
      for (dim_type t = 0; t < d; ++t) {
	if (dof == getfem::second_derivative_dof(d, r, t)) {
	  sprintf(s, "D2%c%c[%d]", "xyzuvw"[r], "xyzuvw"[t], d); 
	  goto found;
	}
      }
    }
  }
 found:
  return s;
}


/* Read parameters from the .param file, build the mesh, set finite element
 * and integration methods and selects the boundaries.
 */
void crack_problem::init(void) {
  std::string MESH_TYPE = PARAM.string_value("MESH_TYPE","Mesh type ");
  std::string FEM_TYPE  = PARAM.string_value("FEM_TYPE","FEM name");
  std::string INTEGRATION = PARAM.string_value("INTEGRATION",
					       "Name of integration method");
  std::string SIMPLEX_INTEGRATION = PARAM.string_value("SIMPLEX_INTEGRATION",
					 "Name of simplex integration method");
  std::string SINGULAR_INTEGRATION = PARAM.string_value("SINGULAR_INTEGRATION");

  add_crack = (PARAM.int_value("ADDITIONAL_CRACK", "An additional crack ?") != 0);
  enrichment_option = 
    enrichment_option_enum(PARAM.int_value("ENRICHMENT_OPTION",
			   "Enrichment option"));
  vectorial_enrichment = (PARAM.int_value("VECTORIAL_ENRICHMENT",
					  "Vectorial enrichment option") != 0);
  cout << "MESH_TYPE=" << MESH_TYPE << "\n";
  cout << "FEM_TYPE="  << FEM_TYPE << "\n";
  cout << "INTEGRATION=" << INTEGRATION << "\n";

  spider.radius = PARAM.real_value("SPIDER_RADIUS","spider_radius");
  spider.Nr = unsigned(PARAM.int_value("SPIDER_NR","Spider_Nr "));
  spider.Ntheta = unsigned(PARAM.int_value("SPIDER_NTHETA","Ntheta "));
  spider.K = int(PARAM.int_value("SPIDER_K","K "));
  spider.theta0 =0;

  translation.resize(2); 
  translation[0] =0.5;
  translation[1] =0.;
  
  /* First step : build the mesh */
  bgeot::pgeometric_trans pgt = 
    bgeot::geometric_trans_descriptor(MESH_TYPE);
  size_type N = pgt->dim();
  std::vector<size_type> nsubdiv(N);
  std::fill(nsubdiv.begin(),nsubdiv.end(),
	    PARAM.int_value("NX", "Nomber of space steps "));
  getfem::regular_unit_mesh(mesh, nsubdiv, pgt,
			    PARAM.int_value("MESH_NOISED") != 0);
  base_small_vector tt(N); tt[1] = -0.5;
  mesh.translation(tt); 
  
  datafilename = PARAM.string_value("ROOTFILENAME","Base name of data files.");
  residual = PARAM.real_value("RESIDUAL");
  if (residual == 0.) residual = 1e-10;
  enr_area_radius = PARAM.real_value("RADIUS_ENR_AREA",
				     "radius of the enrichment area");
  
  mu = PARAM.real_value("MU", "Lame coefficient mu");
  lambda = PARAM.real_value("LAMBDA", "Lame coefficient lambda");

  cutoff.fun_num = PARAM.int_value("CUTOFF_FUNC", "cutoff function");
  cutoff.radius = PARAM.real_value("CUTOFF", "Cutoff");
  cutoff.radius1 = PARAM.real_value("CUTOFF1", "Cutoff1");
  cutoff.radius0 = PARAM.real_value("CUTOFF0", "Cutoff0");
  mf_u().set_qdim(dim_type(N));

  /* set the finite element on the mf_u */
  getfem::pfem pf_u = 
    getfem::fem_descriptor(FEM_TYPE);
  getfem::pintegration_method ppi = 
    getfem::int_method_descriptor(INTEGRATION);
  getfem::pintegration_method simp_ppi = 
    getfem::int_method_descriptor(SIMPLEX_INTEGRATION);
  getfem::pintegration_method sing_ppi = (SINGULAR_INTEGRATION.size() ?
		getfem::int_method_descriptor(SINGULAR_INTEGRATION) : 0);
  
  mim.set_integration_method(mesh.convex_index(), ppi);
  mls.add_level_set(ls);
  if (add_crack) { mls.add_level_set(ls2); mls.add_level_set(ls3); }

  mim.set_simplex_im(simp_ppi, sing_ppi);
  mf_pre_u.set_finite_element(mesh.convex_index(), pf_u);
  mf_pre_mortar.set_finite_element(mesh.convex_index(), 
				   getfem::fem_descriptor(PARAM.string_value("MORTAR_FEM_TYPE")));
  mf_mult.set_finite_element(mesh.convex_index(), pf_u);
  mf_mult.set_qdim(dim_type(N));
  mf_partition_of_unity.set_classical_finite_element(1);
  
  mixed_pressure =
    (PARAM.int_value("MIXED_PRESSURE","Mixed version or not.") != 0);
  mode = int(PARAM.int_value("MODE","Mode for the reference solution"));
  dir_with_mult = unsigned(PARAM.int_value("DIRICHLET_VERSINO"));
  if (mixed_pressure) {
    std::string FEM_TYPE_P  = PARAM.string_value("FEM_TYPE_P","FEM name P");
    mf_p.set_finite_element(mesh.convex_index(),
			    getfem::fem_descriptor(FEM_TYPE_P));
  }

  /* set the finite element on mf_rhs (same as mf_u is DATA_FEM_TYPE is
     not used in the .param file */
  std::string data_fem_name = PARAM.string_value("DATA_FEM_TYPE");
  if (data_fem_name.size() == 0) {
    if (!pf_u->is_lagrange()) {
      GMM_ASSERT1(false, "You are using a non-lagrange FEM. "
		  << "In that case you need to set "
		  << "DATA_FEM_TYPE in the .param file");
    }
    mf_rhs.set_finite_element(mesh.convex_index(), pf_u);
  } else {
    mf_rhs.set_finite_element(mesh.convex_index(), 
			      getfem::fem_descriptor(data_fem_name));
  }
  
  /* set boundary conditions
   * (Neuman on the upper face, Dirichlet elsewhere) */
  cout << "Selecting Neumann and Dirichlet boundaries\n";
  getfem::mesh_region border_faces;
  getfem::outer_faces_of_mesh(mesh, border_faces);
  for (getfem::mr_visitor i(border_faces); !i.finished(); ++i) {
    
    base_node un = mesh.normal_of_face_of_convex(i.cv(), i.f());
    un /= gmm::vect_norm2(un);
#ifdef VALIDATE_XFEM
    mesh.region(DIRICHLET_BOUNDARY_NUM).add(i.cv(), i.f());
#else
    base_node un = mesh.normal_of_face_of_convex(i.cv(), i.f());
    un /= gmm::vect_norm2(un);
    if (un[0] - 1.0 < -1.0E-7) { // new Neumann face
      mesh.region(NEUMANN_BOUNDARY_NUM).add(i.cv(), i.f());
    } else {
      cout << "normal = " << un << endl;
      mesh.region(DIRICHLET_BOUNDARY_NUM).add(i.cv(), i.f());
    }
#endif
  }
  
  
  
#ifdef VALIDATE_XFEM
  exact_sol.init(mode, lambda, mu, ls);
#endif
}


base_small_vector ls_function(const base_node P, int num = 0) {
  scalar_type x = P[0], y = P[1];
  base_small_vector res(2);
  switch (num) {
    case 0: {
      res[0] = y;
      res[1] = -.5 + x;
    } break;
    case 1: {
      res[0] = gmm::vect_dist2(P, base_node(0.5, 0.)) - .25;
      res[1] = gmm::vect_dist2(P, base_node(0.25, 0.0)) - 0.27;
    } break;
    case 2: {
      res[0] = x - 0.25;
      res[1] = gmm::vect_dist2(P, base_node(0.25, 0.0)) - 0.35;
    } break;
    default: assert(0);
  }
  return res;
}

bool crack_problem::solve(plain_vector &U) {
  size_type nb_dof_rhs = mf_rhs.nb_dof();
  size_type N = mesh.dim();
  ls.reinit();  
  cout << "ls.get_mesh_fem().nb_dof() = " << ls.get_mesh_fem().nb_dof() << "\n";
  for (size_type d = 0; d < ls.get_mesh_fem().nb_dof(); ++d) {
    ls.values(0)[d] = ls_function(ls.get_mesh_fem().point_of_dof(d), 0)[0];
    ls.values(1)[d] = ls_function(ls.get_mesh_fem().point_of_dof(d), 0)[1];
  }
  ls.touch();

  if (add_crack) {
    ls2.reinit();
    for (size_type d = 0; d < ls2.get_mesh_fem().nb_dof(); ++d) {
      ls2.values(0)[d] = ls_function(ls2.get_mesh_fem().point_of_dof(d), 1)[0];
      ls2.values(1)[d] = ls_function(ls2.get_mesh_fem().point_of_dof(d), 1)[1];
    }
    ls2.touch();
    
    ls3.reinit();
    for (size_type d = 0; d < ls3.get_mesh_fem().nb_dof(); ++d) {
      ls3.values(0)[d] = ls_function(ls2.get_mesh_fem().point_of_dof(d), 2)[0];
      ls3.values(1)[d] = ls_function(ls2.get_mesh_fem().point_of_dof(d), 2)[1];
    }
    ls3.touch(); 
  }

  mls.adapt();
  mim.adapt();
  mfls_u.adapt();
  mfls_mortar.adapt(); mfls_mortar.set_qdim(2);

  cout << "Setting up the singular functions for the enrichment\n";
  std::vector<getfem::pglobal_function> vfunc(4);
  for (size_type i = 0; i < vfunc.size(); ++i) {
    /* use the singularity */
    getfem::abstract_xy_function *s = 
      new getfem::crack_singular_xy_function(unsigned(i));
    if (enrichment_option != FIXED_ZONE && 
	enrichment_option != GLOBAL_WITH_MORTAR) {
      /* use the product of the singularity function
	 with a cutoff */
      getfem::abstract_xy_function *c = 
	new getfem::cutoff_xy_function(int(cutoff.fun_num),
				       cutoff.radius, 
				       cutoff.radius1,
				       cutoff.radius0);
      s = new getfem::product_of_xy_functions(*s, *c);
    }
    vfunc[i] = getfem::global_function_on_level_set(ls, *s);
  }
  
  mf_sing_u.set_functions(vfunc);


  if (enrichment_option == SPIDER_FEM_ALONE || 
      enrichment_option == SPIDER_FEM_ENRICHMENT) {
    spider.fem = new getfem::spider_fem(spider.radius, mim, spider.Nr,
					spider.Ntheta, spider.K, translation,
					spider.theta0, int(0), scalar_type(0));
    mf_us.set_finite_element(mesh.convex_index(),spider.fem->get_pfem());
    for (dal::bv_visitor_c i(mf_us.convex_index()); !i.finished(); ++i) {
      if (mf_us.fem_of_element(i)->nb_dof(i) == 0) {
	mf_us.set_finite_element(i,0);
      }
    }
    spider.fem->check();
  }

  switch (enrichment_option) {


    case FIXED_ZONE: {
      dal::bit_vector enriched_dofs;
      plain_vector X(mf_partition_of_unity.nb_dof());
      plain_vector Y(mf_partition_of_unity.nb_dof());
      getfem::interpolation(ls.get_mesh_fem(), mf_partition_of_unity,
			    ls.values(1), X);
      getfem::interpolation(ls.get_mesh_fem(), mf_partition_of_unity,
			    ls.values(0), Y);
      for (size_type j = 0; j < mf_partition_of_unity.nb_dof(); ++j) {
	if (gmm::sqr(X[j]) + gmm::sqr(Y[j]) <= gmm::sqr(enr_area_radius))
	  enriched_dofs.add(j);
      }
      if (enriched_dofs.card() < 3)
	GMM_WARNING0("There is " << enriched_dofs.card() <<
		     " enriched dofs for the crack tip");
      mf_product.set_enrichment(enriched_dofs);
      mf_u_sum.set_mesh_fems(mf_product, mfls_u);
    } break;

    case GLOBAL_WITH_MORTAR: {
      // Selecting the element in the enriched domain

      dal::bit_vector cvlist_in_area;
      dal::bit_vector cvlist_out_area;
      for (dal::bv_visitor cv(mesh.convex_index()); 
	   !cv.finished(); ++cv) {
	bool in_area = true;
	/* For each element, we test all of its nodes. 
	   If all the nodes are inside the enrichment area,
	   then the element is completly inside the area too */ 
	for (unsigned j=0; j < mesh.nb_points_of_convex(cv); ++j) {
	  if (gmm::sqr(mesh.points_of_convex(cv)[j][0] - translation[0]) + 
	      gmm::sqr(mesh.points_of_convex(cv)[j][1] - translation[1]) > 
	      gmm::sqr(enr_area_radius)) {
	    in_area = false; break;
	  }
	}

	/* "remove" the global function on convexes outside the enrichment
	   area */
	if (!in_area) {
	  cvlist_out_area.add(cv);
	  mf_sing_u.set_finite_element(cv, 0);
	  mf_u().set_dof_partition(cv, 1);
	} else cvlist_in_area.add(cv);
      }

      /* extract the boundary of the enrichment area, from the
	 "inside" point-of-view, and from the "outside"
	 point-of-view */
      getfem::mesh_region r_border, r_enr_out;
      getfem::outer_faces_of_mesh(mesh, r_border);

      getfem::outer_faces_of_mesh(mesh, cvlist_in_area, 
				  mesh.region(MORTAR_BOUNDARY_IN));
      getfem::outer_faces_of_mesh(mesh, cvlist_out_area, 
				  mesh.region(MORTAR_BOUNDARY_OUT));
      for (getfem::mr_visitor v(r_border); !v.finished(); ++v) {
	mesh.region(MORTAR_BOUNDARY_OUT).sup(v.cv(), v.f());
      }
      mf_u_sum.set_mesh_fems(mf_sing_u, mfls_u);
    } break;

    case GLOBAL_WITH_CUTOFF :{
      if(cutoff.fun_num == getfem::cutoff_xy_function::EXPONENTIAL_CUTOFF)
	cout<<"Using exponential Cutoff..."<<endl;
      else
	cout<<"Using Polynomial Cutoff..."<<endl;
      mf_u_sum.set_mesh_fems(mf_sing_u, mfls_u);
    } break;

    case SPIDER_FEM_ALONE: {
      mf_u_sum.set_mesh_fems(mf_us); 
    } break;
  
    case SPIDER_FEM_ENRICHMENT: {
      mf_u_sum.set_mesh_fems(mf_us, mfls_u); 
    } break;

    case NO_ENRICHMENT: {
      mf_u_sum.set_mesh_fems(mfls_u); 
    } break;
  
  }


  gmm::clear(Qsing); gmm::resize(Qsing, 8, 8);
  ind_first_global_dof = size_type(-1);
  
  if (enrichment_option == GLOBAL_WITH_MORTAR
      || enrichment_option == GLOBAL_WITH_CUTOFF) {
    // compute a base to the orthogonal to the mode I and mode II in the
    // linear combination of singular function in order to reduce the problem
    // on a vectorial enrichment with only two dofs.

    exact_solution es1(mesh), es2(mesh);
    es1.init(1, lambda, mu, ls);
    es2.init(2, lambda, mu, ls);

    gmm::copy(gmm::identity_matrix(), Qsing);
    gmm::copy(es1.U, gmm::mat_col(Qsing, 0));
    gmm::copy(es2.U, gmm::mat_col(Qsing, 1));
    gmm::lu_inverse(Qsing);

    // Search the position of the singular enrichment dofs.

    size_type Qdim = mf_u().get_qdim();
    for (dal::bv_visitor cv(mesh.convex_index());
	 !cv.finished() && (ind_first_global_dof == size_type(-1)); ++cv) {
      getfem::pfem pf = mf_u().fem_of_element(cv);
      for (size_type i = 0; i < pf->nb_dof(cv); ++i) {
	// cout << "type of dof : " << name_of_dof(pf->dof_types()[i]) << endl;
	if (pf->dof_types()[i] == getfem::global_dof(mesh.dim())) {
	  if (ind_first_global_dof == size_type(-1))
	    ind_first_global_dof =  mf_u().ind_dof_of_element(cv)[i*Qdim];
	}
      }
    }
      
    cout << "first global dof = " << ind_first_global_dof << endl;
    GMM_ASSERT1(ind_first_global_dof != size_type(-1), "internal error");
  }

  

  U.resize(mf_u().nb_dof());

  if (mixed_pressure) cout << "Number of dof for P: " << mf_p.nb_dof() << endl;
  cout << "Number of dof for u: " << mf_u().nb_dof() << endl;

  unsigned Q = mf_u().get_qdim();
  if (0) {
    for (unsigned d=0; d < mf_u().nb_dof(); d += Q) {
      printf("dof %4d @ %+6.2f:%+6.2f: ", d, 
	     mf_u().point_of_dof(d)[0], mf_u().point_of_dof(d)[1]);

      const getfem::mesh::ind_cv_ct cvs = mf_u().convex_to_dof(d);
      for (unsigned i=0; i < cvs.size(); ++i) {
	unsigned cv = unsigned(cvs[i]);
	//if (pm_cvlist.is_in(cv)) flag1 = true; else flag2 = true;

	getfem::pfem pf = mf_u().fem_of_element(cv);
	unsigned ld = unsigned(-1);
	for (unsigned dd = 0; dd < mf_u().nb_dof_of_element(cv); dd += Q) {
	  if (mf_u().ind_dof_of_element(cv)[dd] == d) {
	    ld = dd/Q; break;
	  }
	}
	if (ld == unsigned(-1)) {
	  cout << "DOF " << d << "NOT FOUND in " << cv << " BUG BUG\n";
	} else {
	  printf(" %3d:%.16s", cv, name_of_dof(pf->dof_types().at(ld)).c_str());
	}
      }
      printf("\n");
    }
  }
  
  // Linearized elasticity brick.
  
  
  getfem::mdbrick_isotropic_linearized_elasticity<>
    ELAS(mim, mf_u(), mixed_pressure ? 0.0 : lambda, mu);
  
  //gmm::HarwellBoeing_IO::write("K.hb", ELAS.get_K());

  getfem::mdbrick_abstract<> *pINCOMP;
  if (mixed_pressure) {
    getfem::mdbrick_linear_incomp<> *incomp
      = new getfem::mdbrick_linear_incomp<>(ELAS, mf_p);
    incomp->penalization_coeff().set(1.0/lambda);
    pINCOMP = incomp;
  }
  else pINCOMP = &ELAS;


  getfem::mdbrick_abstract<> *pVECTCONS;
  if (vectorial_enrichment && (enrichment_option == GLOBAL_WITH_MORTAR
			       || enrichment_option == GLOBAL_WITH_CUTOFF)) {

    getfem::mdbrick_constraint<> *vectcons
      = new getfem::mdbrick_constraint<>(*pINCOMP);
    sparse_matrix BB(6, mf_u().nb_dof());

    gmm::copy(gmm::sub_matrix(Qsing, gmm::sub_interval(2,6),
			      gmm::sub_interval(0,8)),
	      gmm::sub_matrix(BB, gmm::sub_interval(0,6),
			      gmm::sub_interval(ind_first_global_dof, 8)));
    vectcons->set_constraints(BB, plain_vector(6));
    pVECTCONS = vectcons;

  }
  else pVECTCONS = pINCOMP;


  // Defining the volumic source term.
  plain_vector F(nb_dof_rhs * N);
  for (size_type i = 0; i < nb_dof_rhs; ++i)
      gmm::copy(sol_f(mf_rhs.point_of_dof(i)),
		gmm::sub_vector(F, gmm::sub_interval(i*N, N)));
  
  // Volumic source term brick.
  getfem::mdbrick_source_term<> VOL_F(*pVECTCONS, mf_rhs, F);

  // Defining the Neumann condition right hand side.
  gmm::clear(F);
  
  // Neumann condition brick.
        
  getfem::mdbrick_source_term<>  NEUMANN(VOL_F, mf_rhs, F,NEUMANN_BOUNDARY_NUM);        
   
 //toto_solution toto(mf_rhs.linked_mesh()); toto.init();
 //assert(toto.mf.nb_dof() == 1);
   
 // Dirichlet condition brick.
  getfem::mdbrick_Dirichlet<> DIRICHLET(NEUMANN, DIRICHLET_BOUNDARY_NUM, mf_mult);
  
#ifdef VALIDATE_XFEM
  DIRICHLET.rhs().set(exact_sol.mf,exact_sol.U);
#endif   
  DIRICHLET.set_constraints_type(getfem::constraints_type(dir_with_mult));

  getfem::mdbrick_abstract<> *final_model = &DIRICHLET;

  if (enrichment_option == GLOBAL_WITH_MORTAR) {
    /* add a constraint brick for the mortar junction between
       the enriched area and the rest of the mesh */
    /* we use mfls_u as the space of lagrange multipliers */
    getfem::mesh_fem &mf_mortar = mfls_mortar; 
    /* adjust its qdim.. this is just evil and dangerous
       since mf_u() is built upon mfls_u.. it would be better
       to use a copy. */
    mf_mortar.set_qdim(2); // EVIL 
    getfem::mdbrick_constraint<> &mortar = 
      *(new getfem::mdbrick_constraint<>(DIRICHLET,0));

    cout << "Handling mortar junction\n";

    /* list of dof of mf_mortar for the mortar condition */
    std::vector<size_type> ind_mortar;
    /* unfortunately , dof_on_region sometimes returns too much dof
       when mf_mortar is an enriched one so we have to filter them */
    sparse_matrix M(mf_mortar.nb_dof(), mf_mortar.nb_dof());
    getfem::asm_mass_matrix(M, mim, mf_mortar, MORTAR_BOUNDARY_OUT);
    for (dal::bv_visitor_c d(mf_mortar.dof_on_region(MORTAR_BOUNDARY_OUT)); 
	 !d.finished(); ++d) {
      if (M(d,d) > 1e-8) ind_mortar.push_back(d);
      else cout << "  removing non mortar dof" << d << "\n";
    }

    cout << ind_mortar.size() << " dof for the lagrange multiplier)\n";

    sparse_matrix H0(mf_mortar.nb_dof(), mf_u().nb_dof()), 
      H(ind_mortar.size(), mf_u().nb_dof());


    gmm::sub_index sub_i(ind_mortar);
    gmm::sub_interval sub_j(0, mf_u().nb_dof());
    
    /* build the mortar constraint matrix -- note that the integration
       method is conformal to the crack
     */
    getfem::asm_mass_matrix(H0, mim, mf_mortar, mf_u(), 
			    MORTAR_BOUNDARY_OUT);
    gmm::copy(gmm::sub_matrix(H0, sub_i, sub_j), H);

    gmm::clear(H0);
    getfem::asm_mass_matrix(H0, mim, mf_mortar, mf_u(), 
			    MORTAR_BOUNDARY_IN);
    gmm::add(gmm::scaled(gmm::sub_matrix(H0, sub_i, sub_j), -1), H);


    /* because of the discontinuous partition of mf_u(), some levelset
       enriched functions do not contribute any more to the
       mass-matrix (the ones which are null on one side of the
       levelset, when split in two by the mortar partition, may create
       a "null" dof whose base function is all zero..
    */
    sparse_matrix M2(mf_u().nb_dof(), mf_u().nb_dof());
    getfem::asm_mass_matrix(M2, mim, mf_u(), mf_u());

    for (size_type d = 0; d < mf_u().nb_dof(); ++d) {
      if (M2(d,d) < 1e-10) {
	cout << "  removing null mf_u() dof " << d << "\n";
	size_type n = gmm::mat_nrows(H);
	gmm::resize(H, n+1, gmm::mat_ncols(H));
	H(n, d) = 1;
      }
    }
    
    
    
    getfem::base_vector R(gmm::mat_nrows(H));
    mortar.set_constraints(H,R);

    final_model = &mortar;
  }

   
  // Generic solve.
  cout << "Total number of variables : " << final_model->nb_dof() << endl;
  getfem::standard_model_state MS(*final_model);
  gmm::iteration iter(residual, 1, 40000);
  getfem::standard_solve(MS, *final_model, iter);
  
  // Solution extraction
  gmm::copy(ELAS.get_solution(MS), U);
  
  return (iter.converged());
}

void crack_problem::compute_sif(const plain_vector &U) {
  cout << "Computing stress intensity factors\n";
  base_node tip; base_small_vector T, N;
  getfem::get_crack_tip_and_orientation(ls, tip, T, N);
  cout << "crack tip is : " << tip << ", T=" << T << ", N=" << N << "\n";
  cout << "young modulus: " << young_modulus(lambda, mu) << "\n";
  scalar_type ring_radius = 0.2;
  
  scalar_type KI, KII;
  estimate_crack_stress_intensity_factors(ls, mf_u(), U, 
                                          young_modulus(lambda, mu), 
                                          KI, KII, 1e-2);
  cout << "estimation of crack SIF: " << KI << ", " << KII << "\n";
  
  compute_crack_stress_intensity_factors(ls, mim, mf_u(), U, ring_radius, 
                                         lambda, mu, 
                                         young_modulus(lambda, mu), 
                                         KI, KII);
  cout << "computation of crack SIF: " << KI << ", " << KII << "\n";


  if (enrichment_option == GLOBAL_WITH_CUTOFF
      || enrichment_option == GLOBAL_WITH_MORTAR) {
    /* Compare the computed coefficients of the global functions with
     * the exact one.
     */
    plain_vector diff(8);
    gmm::copy(gmm::sub_vector(U,gmm::sub_interval(ind_first_global_dof, 8)),
	      diff);
    cout << "GLOBAPPROX = " << diff << endl;
    cout << "GLOBEXACT = " << exact_sol.U << endl;
    
    if (!vectorial_enrichment) {
      gmm::add(gmm::scaled(exact_sol.U, -1.0),diff);
      cout << "euclidean error %: " << 100.0*gmm::vect_norm2(diff)/gmm::vect_norm2(exact_sol.U) << endl;
    }
    else {
      plain_vector rr(8);
      gmm::mult(Qsing, diff, rr);
      cout << "KIh = " << rr[0] << "  KIIh = " << rr[1] << endl;
    }
  }
}

/**************************************************************************/
/*  main program.                                                         */
/**************************************************************************/

int main(int argc, char *argv[]) {

  GMM_SET_EXCEPTION_DEBUG; // Exceptions make a memory fault, to debug.
  FE_ENABLE_EXCEPT;        // Enable floating point exception for Nan.

  //getfem::getfem_mesh_level_set_noisy();


  try {
    crack_problem p;
    p.PARAM.read_command_line(argc, argv);
    p.init();
    p.mesh.write_to_file(p.datafilename + ".mesh");

    plain_vector U(p.mf_u().nb_dof());
    if (!p.solve(U)) GMM_ASSERT1(false, "Solve has failed");

    p.compute_sif(U);
    
    { 
      getfem::mesh mcut;
      p.mls.global_cut_mesh(mcut);
      dim_type Q = p.mf_u().get_qdim();
      getfem::mesh_fem mf(mcut, Q);
      mf.set_classical_discontinuous_finite_element(2, 0.001);
      // mf.set_finite_element
      //	(getfem::fem_descriptor("FEM_PK_DISCONTINUOUS(2, 2, 0.0001)"));
      plain_vector V(mf.nb_dof());

      /*for (unsigned i=0; i < p.mf_u().nb_dof(); ++i) {
	cout << "dof " << i << ": " << p.mf_u().point_of_dof(i);
      }
      gmm::fill_random(U);*/

      getfem::interpolation(p.mf_u(), mf, U, V);

      getfem::stored_mesh_slice sl;
      getfem::mesh mcut_refined;

      unsigned NX = unsigned(p.PARAM.int_value("NX")), nn;
      if (NX < 6) nn = 8;
      else if (NX < 12) nn = 8;
      else if (NX < 30) nn = 3;
      else nn = 1;

      /* choose an adequate slice refinement based on the distance to the crack tip */
      std::vector<bgeot::short_type> nrefine(mcut.convex_index().last_true()+1);
      for (dal::bv_visitor cv(mcut.convex_index()); !cv.finished(); ++cv) {
	scalar_type dmin=0, d;
	base_node Pmin,P;
	for (unsigned i=0; i < mcut.nb_points_of_convex(cv); ++i) {
	  P = mcut.points_of_convex(cv)[i];
	  d = gmm::vect_norm2(ls_function(P));
	  if (d < dmin || i == 0) { dmin = d; Pmin = P; }
	}

	if (dmin < 1e-5)
	  nrefine[cv] = short_type(nn*4);
	else if (dmin < .1) 
	  nrefine[cv] = short_type(nn*2);
	else nrefine[cv] = short_type(nn);
	if (dmin < .01)
	  cout << "cv: "<< cv << ", dmin = " << dmin << "Pmin=" << Pmin << " " << nrefine[cv] << "\n";
      }

      {
	getfem::mesh_slicer slicer(mcut); 
	getfem::slicer_build_mesh bmesh(mcut_refined);
	slicer.push_back_action(bmesh);
	slicer.exec(nrefine, getfem::mesh_region::all_convexes());
      }
      /*
      sl.build(mcut, 
      getfem::slicer_build_mesh(mcut_refined), nrefine);*/

      getfem::mesh_im mim_refined(mcut_refined); 
      mim_refined.set_integration_method(getfem::int_method_descriptor
					 ("IM_TRIANGLE(6)"));

      getfem::mesh_fem mf_refined(mcut_refined, dim_type(Q));
      mf_refined.set_classical_discontinuous_finite_element(2, 0.0001);
      plain_vector W(mf_refined.nb_dof());

      getfem::interpolation(p.mf_u(), mf_refined, U, W);

#ifdef VALIDATE_XFEM
      p.exact_sol.mf.set_qdim(dim_type(Q));
      assert(p.exact_sol.mf.nb_dof() == p.exact_sol.U.size());
      plain_vector EXACT(mf_refined.nb_dof());
      getfem::interpolation(p.exact_sol.mf, mf_refined, 
			    p.exact_sol.U, EXACT);

      plain_vector DIFF(EXACT); gmm::add(gmm::scaled(W,-1),DIFF);
#endif

      if (p.PARAM.int_value("VTK_EXPORT")) {
	getfem::mesh_fem mf_refined_vm(mcut_refined, 1);
	mf_refined_vm.set_classical_discontinuous_finite_element(1, 0.0001);
	cerr << "mf_refined_vm.nb_dof=" << mf_refined_vm.nb_dof() << "\n";
	plain_vector VM(mf_refined_vm.nb_dof());

	cout << "computing von mises\n";
	getfem::interpolation_von_mises(mf_refined, mf_refined_vm, W, VM);

	plain_vector D(mf_refined_vm.nb_dof() * Q), 
	  DN(mf_refined_vm.nb_dof());
	
#ifdef VALIDATE_XFEM
	getfem::interpolation(mf_refined, mf_refined_vm, DIFF, D);
	for (unsigned i=0; i < DN.size(); ++i) {
	  DN[i] = gmm::vect_norm2(gmm::sub_vector(D, gmm::sub_interval(i*Q, Q)));
	}
#endif

	cout << "export to " << p.datafilename + ".vtk" << "..\n";
	getfem::vtk_export exp(p.datafilename + ".vtk",
			       p.PARAM.int_value("VTK_EXPORT")==1);

	exp.exporting(mf_refined); 
	//exp.write_point_data(mf_refined_vm, DN, "error");
	exp.write_point_data(mf_refined_vm, VM, "von mises stress");

	exp.write_point_data(mf_refined, W, "elastostatic_displacement");
      
#ifdef VALIDATE_XFEM

	plain_vector VM_EXACT(mf_refined_vm.nb_dof());


	/* getfem::mesh_fem_global_function mf(mcut_refined,Q);
	   std::vector<getfem::pglobal_function> cfun(4);
	   for (unsigned j=0; j < 4; ++j)
	   cfun[j] = getfem::isotropic_crack_singular_2D(j, p.ls);
	   mf.set_functions(cfun);
	   getfem::interpolation_von_mises(mf, mf_refined_vm, p.exact_sol.U,
	   VM_EXACT);
	*/


	getfem::interpolation_von_mises(mf_refined, mf_refined_vm, EXACT, VM_EXACT);
	getfem::vtk_export exp2("crack_exact.vtk");
	exp2.exporting(mf_refined);
	exp2.write_point_data(mf_refined_vm, VM_EXACT, "exact von mises stress");
	exp2.write_point_data(mf_refined, EXACT, "reference solution");
	
#endif

	cout << "export done, you can view the data file with (for example)\n"
	  "mayavi -d " << p.datafilename << ".vtk -f "
	  "WarpVector -m BandedSurfaceMap -m Outline\n";
      }


#ifdef VALIDATE_XFEM
      cout << "L2 ERROR:"<< getfem::asm_L2_dist(p.mim, p.mf_u(), U,
						p.exact_sol.mf, p.exact_sol.U)
	   << endl << "H1 ERROR:"
	   << getfem::asm_H1_dist(p.mim, p.mf_u(), U,
				  p.exact_sol.mf, p.exact_sol.U) << "\n";
      
       
      cout << "L2 norm of the solution:"  << getfem::asm_L2_norm(p.mim,p.mf_u(),U)<<endl;
      cout << "H1 norm of the solution:"  << getfem::asm_H1_norm(p.mim,p.mf_u(),U)<<endl;


      /* cout << "OLD ERROR L2:" 
	 << getfem::asm_L2_norm(mim_refined,mf_refined,DIFF) 
	 << " H1:" << getfem::asm_H1_dist(mim_refined,mf_refined,
	 EXACT,mf_refined,W)  << "\n";

	 cout << "ex = " << p.exact_sol.U << "\n";
	 cout << "U  = " << gmm::sub_vector(U, gmm::sub_interval(0,8)) << "\n";
      */
#endif
    }

  }
  GMM_STANDARD_CATCH_ERROR;

  return 0; 
}
