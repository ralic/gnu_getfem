// -*- c++ -*- (enables emacs c++ mode)
//========================================================================
//
// Library : Generic Matrix Methods  (gmm)
// File    : gmm_solver_Schwarz_additive.h : generic solver.
//           
// Date    : October 13, 2002.
// Authors : Yves Renard <Yves.Renard@insa-toulouse.fr>
//           Michel Fournie, fournie@mip.ups-tlse.fr
//
//========================================================================
//
// Copyright (C) 2002-2005 Yves Renard
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

/**@file gmm_solver_Schwarz_additive.h*/

#ifndef GMM_SOLVERS_SCHWARZ_ADDITIVE_H__
#define GMM_SOLVERS_SCHWARZ_ADDITIVE_H__ 

#include <gmm_kernel.h>
#include <gmm_superlu_interface.h>
#include <gmm_solver_cg.h>
#include <gmm_solver_gmres.h>
#include <gmm_solver_bicgstab.h>
#include <gmm_solver_qmr.h>

namespace gmm {
      
  /* ******************************************************************** */
  /*		Additive Schwarz interfaced local solvers                 */
  /* ******************************************************************** */

  struct using_cg {};
  struct using_gmres {};
  struct using_bicgstab {};
  struct using_qmr {};

  template <typename P, typename local_solver, typename Matrix>
  struct actual_precond {
    typedef P APrecond;
    static const APrecond &transform(const P &PP) { return PP; }
  };

  template <typename Matrix1, typename Precond, typename Vector> 
  void AS_local_solve(using_cg, const Matrix1 &A, Vector &x, const Vector &b,
		 const Precond &P, iteration &iter)
  { cg(A, x, b, P, iter); }

  template <typename Matrix1, typename Precond, typename Vector> 
  void AS_local_solve(using_gmres, const Matrix1 &A, Vector &x,
		      const Vector &b, const Precond &P, iteration &iter)
  { gmres(A, x, b, P, 100, iter); }
  
  template <typename Matrix1, typename Precond, typename Vector> 
  void AS_local_solve(using_bicgstab, const Matrix1 &A, Vector &x,
		      const Vector &b, const Precond &P, iteration &iter)
  { bicgstab(A, x, b, P, iter); }

  template <typename Matrix1, typename Precond, typename Vector> 
  void AS_local_solve(using_qmr, const Matrix1 &A, Vector &x,
		      const Vector &b, const Precond &P, iteration &iter)
  { qmr(A, x, b, P, iter); }

#if defined(GMM_USES_SUPERLU)
  struct using_superlu {};

  template <typename P, typename Matrix>
  struct actual_precond<P, using_superlu, Matrix> {
    typedef typename linalg_traits<Matrix>::value_type value_type;
    typedef SuperLU_factor<value_type> APrecond;
    template <typename PR>
    static APrecond transform(const PR &) { return APrecond(); }
    static const APrecond &transform(const APrecond &PP) { return PP; }
  };

  template <typename Matrix1, typename Precond, typename Vector> 
  void AS_local_solve(using_superlu, const Matrix1 &, Vector &x,
		      const Vector &b, const Precond &P, iteration &iter)
  { P.solve(x, b); iter.set_iteration(1); }
#endif

  /* ******************************************************************** */
  /*		Additive Schwarz Linear system                            */
  /* ******************************************************************** */

  template <typename Matrix1, typename Matrix2, typename Precond,
	    typename local_solver>
  struct add_schwarz_mat{
    typedef typename linalg_traits<Matrix1>::value_type value_type;

    const Matrix1 *A;
    const std::vector<Matrix2> *vB;
    std::vector<Matrix2> vAloc;
    mutable iteration iter;
    double residual;
    mutable size_type itebilan;
    mutable std::vector<std::vector<value_type> > gi, fi;
    std::vector<typename actual_precond<Precond, local_solver,
					Matrix1>::APrecond> precond1;

    void init(const Matrix1 &A_, const std::vector<Matrix2> &vB_,
	      iteration iter_, const Precond &P, double residual_);

    add_schwarz_mat(void) {}
    add_schwarz_mat(const Matrix1 &A_, const std::vector<Matrix2> &vB_,
		iteration iter_, const Precond &P, double residual_)
    { init(A_, vB_, iter_, P, residual_); }
  };

  template <typename Matrix1, typename Matrix2, typename Precond,
	    typename local_solver>
  void add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver>::init(
       const Matrix1 &A_, const std::vector<Matrix2> &vB_,
       iteration iter_, const Precond &P, double residual_) {

    vB = &vB_; A = &A_; iter = iter_;
    residual = residual_;
    
    size_type nb_sub = vB->size();
    vAloc.resize(nb_sub);
    gi.resize(nb_sub); fi.resize(nb_sub);
    precond1.resize(nb_sub);
    std::fill(precond1.begin(), precond1.end(),
	      actual_precond<Precond, local_solver, Matrix1>::transform(P));
    itebilan = 0;
    
    if (iter.get_noisy()) cout << "Init pour sub dom ";
#ifdef GMM_USES_MPI
    int size,tranche,borne_sup,borne_inf,rank,tag1=11,tag2=12,tag3=13,sizepr = 0,tab[4];
    double t_ref,t_final;
    MPI_Status status;
    t_ref=MPI_Wtime();
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    tranche=nb_sub/size;
    borne_inf=rank*tranche;
    borne_sup=(rank+1)*tranche;
    if (rank==size-1) borne_sup=nb_sub;


    cout << "Nombre de sous domaines " << borne_sup - borne_inf << endl;

    int sizeA = mat_nrows(*A);
    gmm::csr_matrix<value_type> Acsr(sizeA, sizeA), Acsrtemp(sizeA, sizeA);
    gmm::copy(gmm::eff_matrix(*A), Acsr);
    int next = (rank + 1) % size;
    int previous = (rank + size - 1) % size;
    //communication of local information on ring pattern
    //Each process receive  Nproc-1 contributions 

    for (int nproc = 0; nproc < size; ++nproc) {
      //      for (size_type i = size_type(borne_inf); i < size_type(borne_sup); ++i) {
     for (size_type j = 0; j < size_type(nb_sub/size); ++j) {
      size_type i=(rank+size*(j-1)+nb_sub)%nb_sub;

	cout << "Sous domaines " << i << " : " << mat_ncols((*vB)[i]) << endl;
#else
	for (size_type i = 0; i < nb_sub; ++i) {
#endif
	  
	  if (iter.get_noisy()) cout << i << " " << std::flush;
	  Matrix2 Maux(mat_ncols((*vB)[i]), mat_nrows((*vB)[i]));
	  
#ifdef GMM_USES_MPI
	  Matrix2 Maux2(mat_ncols((*vB)[i]), mat_ncols((*vB)[i]));
	  if (nproc == 0) {
	    gmm::resize(vAloc[i], mat_ncols((*vB)[i]), mat_ncols((*vB)[i]));
	    gmm::clear(vAloc[i]);
	  }
	  gmm::mult(gmm::transposed((*vB)[i]), Acsr, Maux);
	  gmm::mult(Maux, (*vB)[i], Maux2);
	  gmm::add(Maux2, vAloc[i]);
#else
	  gmm::resize(vAloc[i], mat_ncols((*vB)[i]), mat_ncols((*vB)[i]));
	  gmm::mult(gmm::transposed((*vB)[i]), *A, Maux);
	  gmm::mult(Maux, (*vB)[i], vAloc[i]);
#endif

#ifdef GMM_USES_MPI
	  if (nproc == size - 1 ) {
#endif
	    precond1[i].build_with(vAloc[i]);
	    gmm::resize(fi[i], mat_ncols((*vB)[i]));
	    gmm::resize(gi[i], mat_ncols((*vB)[i]));
#ifdef GMM_USES_MPI
	  }
#else
	}
#endif
#ifdef GMM_USES_MPI
     }
      if (nproc != size - 1) {
	MPI_Sendrecv(Acsr.jc, sizeA+1, MPI_INT, next, tag2,
		     Acsrtemp.jc, sizeA+1,MPI_INT,previous,tag2,
		     MPI_COMM_WORLD,&status);
	if (Acsrtemp.jc[sizeA] > size_type(sizepr)) {
	  sizepr = Acsrtemp.jc[sizeA];
	  delete[] Acsrtemp.pr; delete[] Acsrtemp.ir;
	  Acsrtemp.pr = new value_type[sizepr];
	  Acsrtemp.ir = new unsigned int[sizepr];
	}
	MPI_Sendrecv(Acsr.ir, Acsr.jc[sizeA], MPI_INT, next, tag1,
		     Acsrtemp.ir, Acsrtemp.jc[sizeA],MPI_INT,previous,tag1,
		     MPI_COMM_WORLD,&status);
	
	MPI_Sendrecv(Acsr.pr, Acsr.jc[sizeA], mpi_type(value_type()), next, tag3, 
		     Acsrtemp.pr, Acsrtemp.jc[sizeA],mpi_type(value_type()),previous,tag3,
		     MPI_COMM_WORLD,&status);
	gmm::copy(Acsrtemp, Acsr);
      }
    }
      t_final=MPI_Wtime();
    cout<<"temps boucle precond "<< t_final-t_ref<<endl;
#endif
    if (iter.get_noisy()) cout << "\n";
  }
  
  template <typename Matrix1, typename Matrix2, typename Precond,
	    typename Vector2, typename Vector3, typename local_solver>
  void mult(const add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver> &M,
	    const Vector2 &p, Vector3 &q) {
    size_type itebilan = 0;
#ifdef GMM_USES_MPI
    static double tmult_tot = 0.0;
    double t_ref = MPI_Wtime();
#endif
    cout << "tmult AS begin " << endl;
    mult(*(M.A), p, q);
#ifdef GMM_USES_MPI
    tmult_tot += MPI_Wtime()-t_ref;
    cout << "tmult_tot = " << tmult_tot << endl;
#endif
    std::vector<double> qbis(gmm::vect_size(q));
    std::vector<double> qter(gmm::vect_size(q));
#ifdef GMM_USES_MPI
    MPI_Status status;
    MPI_Request request,request1;
    int tag=111;
    int size,tranche,borne_sup,borne_inf,rank,nb_sub;
    nb_sub=M.fi.size();
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    nb_sub=M.fi.size();
    tranche=nb_sub/size;
    borne_inf=rank*tranche;
    borne_sup=(rank+1)*tranche;
    if (rank==size-1) borne_sup=nb_sub;
    int next = (rank + 1) % size;
    int previous = (rank + size - 1) % size;
    t_ref = MPI_Wtime();
//     for (size_type i = size_type(borne_inf); i < size_type(borne_sup); ++i)
     for (size_type j = 0; j < size_type(nb_sub/size); ++j)
#else

    for (size_type i = 0; i < M.fi.size(); ++i)
#endif
      {
#ifdef GMM_USES_MPI
      size_type i=(rank+size*(j-1)+nb_sub)%nb_sub;
#endif
	gmm::mult(gmm::transposed((*(M.vB))[i]), q, M.fi[i]);
       M.iter.init();
       AS_local_solve(local_solver(), (M.vAloc)[i], (M.gi)[i],
		      (M.fi)[i],(M.precond1)[i],M.iter);
       itebilan = std::max(itebilan, M.iter.get_iteration());
       }

#ifdef GMM_USES_MPI
    cout << "First  AS loop time " <<  MPI_Wtime() - t_ref << endl;
#endif

    gmm::clear(q);
#ifdef GMM_USES_MPI
    t_ref = MPI_Wtime();
     for (size_type j = 0; j < size_type(nb_sub/size); ++j)

#else
      for (size_type i = 0; i < M.gi.size(); ++i)
#endif
	{

#ifdef GMM_USES_MPI
      size_type i=(rank+size*(j-1)+nb_sub)%nb_sub;
      gmm::mult((*(M.vB))[i], M.gi[i], qbis,qbis);

#else
	  gmm::mult((*(M.vB))[i], M.gi[i], q, q);
#endif
	}
#ifdef GMM_USES_MPI
     //WARNING this add only if you use the ring pattern below
		// need to do this below if using a n explicit ring pattern communication

//      add(qbis,q,q);
    cout << "Second AS loop time " <<  MPI_Wtime() - t_ref << endl;
#endif


#ifdef GMM_USES_MPI
    int tag1=11;
    static double t_tot = 0.0;
    double t_final;
    t_ref=MPI_Wtime();
//     int next = (rank + 1) % size;
//     int previous = (rank + size - 1) % size;
    //communication of local information on ring pattern
    //Each process receive  Nproc-1 contributions 

//     if (size > 1) {
//     for (int nproc = 0; nproc < size-1; ++nproc) 
//       {

// 	MPI_Sendrecv(&(qbis[0]), gmm::vect_size(q), MPI_DOUBLE, next, tag1,
// 		   &(qter[0]), gmm::vect_size(q),MPI_DOUBLE,previous,tag1,
// 		   MPI_COMM_WORLD,&status);
// 	gmm::copy(qter, qbis);
// 	add(qbis,q,q);
//       }
//     }
    MPI_Allreduce(&(qbis[0]), &(q[0]),gmm::vect_size(q), MPI_DOUBLE,
		  MPI_SUM,MPI_COMM_WORLD);
    t_final=MPI_Wtime();
    t_tot += t_final-t_ref;
     cout<<"["<< rank<<"] temps reduce Resol "<< t_final-t_ref << " t_tot = " << t_tot << endl;
#endif 

    if (M.iter.get_noisy() > 0) cout << "itebloc = " << itebilan << endl;
    M.itebilan += itebilan;
    M.iter.set_resmax((M.iter.get_resmax() + M.residual) * 0.5);
  }

  template <typename Matrix1, typename Matrix2, typename Precond,
	    typename Vector2, typename Vector3, typename local_solver>
  void mult(const add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver> &M,
	    const Vector2 &p, const Vector3 &q) {
    mult(M, p, const_cast<Vector3 &>(q));
  }

  template <typename Matrix1, typename Matrix2, typename Precond,
	    typename Vector2, typename Vector3, typename Vector4,
	    typename local_solver>
  void mult(const add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver> &M,
	    const Vector2 &p, const Vector3 &p2, Vector4 &q)
  { mult(M, p, q); add(p2, q); }

  template <typename Matrix1, typename Matrix2, typename Precond,
	    typename Vector2, typename Vector3, typename Vector4,
	    typename local_solver>
  void mult(const add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver> &M,
	    const Vector2 &p, const Vector3 &p2, const Vector4 &q)
  { mult(M, p, const_cast<Vector4 &>(q)); add(p2, q); }

  /* ******************************************************************** */
  /*		Additive Schwarz interfaced global solvers                */
  /* ******************************************************************** */

  template <typename ASM_type, typename Vect>
  void AS_global_solve(using_cg, const ASM_type &ASM, Vect &x,
		       const Vect &b, iteration &iter)
  { cg(ASM, x, b, *(ASM.A), identity_matrix(), iter); }

  template <typename ASM_type, typename Vect>
  void AS_global_solve(using_gmres, const ASM_type &ASM, Vect &x,
		       const Vect &b, iteration &iter)
  { gmres(ASM, x, b, identity_matrix(), 100, iter); }

  template <typename ASM_type, typename Vect>
  void AS_global_solve(using_bicgstab, const ASM_type &ASM, Vect &x,
		       const Vect &b, iteration &iter)
  { bicgstab(ASM, x, b, identity_matrix(), iter); }

  template <typename ASM_type, typename Vect>
  void AS_global_solve(using_qmr,const ASM_type &ASM, Vect &x,
		       const Vect &b, iteration &iter)
  { qmr(ASM, x, b, identity_matrix(), iter); }

#if defined(GMM_USES_SUPERLU)
  template <typename ASM_type, typename Vect>
  void AS_global_solve(using_superlu, const ASM_type &, Vect &,
		       const Vect &, iteration &) {
    DAL_THROW(failure_error,
     "You cannot use SuperLU as global solver in additive Schwarz meethod");
  }
#endif
  
  /* ******************************************************************** */
  /*		Sequential Linear Additive Schwarz method                 */
  /* ******************************************************************** */
  /* ref : Domain decomposition algorithms for the p-version finite       */
  /*       element method for elliptic problems, Luca F. Pavarino,        */
  /*       PhD thesis, Courant Institute of Mathematical Sciences, 1992.  */
  /* ******************************************************************** */

  /** Function to call if the ASM matrix is precomputed for successive solve
   * with the same system.
   */
  template <typename Matrix1, typename Matrix2,
	    typename Vector2, typename Vector3, typename Precond,
	    typename local_solver, typename global_solver>
  void additive_schwarz(
    add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver> &ASM, Vector3 &u,
    const Vector2 &f, iteration &iter, const global_solver&) {

    typedef typename linalg_traits<Matrix1>::value_type value_type;

    size_type nb_sub = ASM.vB->size(), nb_dof = gmm::vect_size(f);
    ASM.itebilan = 0;
    std::vector<value_type> g(nb_dof);
    std::vector<value_type> gbis(nb_dof);
#ifdef GMM_USES_MPI
    double t_init=MPI_Wtime();
    int size,tranche,borne_sup,borne_inf,rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    tranche=nb_sub/size;
    borne_inf=rank*tranche;
    borne_sup=(rank+1)*tranche;
    if (rank==size-1) borne_sup=nb_sub;
//     for (size_type i = size_type(borne_inf); i < size_type(borne_sup); ++i)
     for (size_type j = 0; j < size_type(nb_sub/size); ++j)

    //    for (size_type i = rank; i < nb_sub; i+=size)
#else
    for (size_type i = 0; i < nb_sub; ++i)
#endif
    {

#ifdef GMM_USES_MPI
      size_type i=(rank+size*(j-1)+nb_sub)%nb_sub;
#endif
      gmm::mult(gmm::transposed((*(ASM.vB))[i]), f, ASM.fi[i]);
      ASM.iter.init();
      AS_local_solve(local_solver(), ASM.vAloc[i], ASM.gi[i], ASM.fi[i],
		     ASM.precond1[i], ASM.iter);
      ASM.itebilan = std::max(ASM.itebilan, ASM.iter.get_iteration());
#ifdef GMM_USES_MPI

    gmm::mult((*(ASM.vB))[i], ASM.gi[i], gbis,gbis);

#else   
      gmm::mult((*(ASM.vB))[i], ASM.gi[i], g, g);
#endif
    }
#ifdef GMM_USES_MPI
    cout<<"temps boucle init "<< MPI_Wtime()-t_init<<endl;
    double t_ref,t_final;
    t_ref=MPI_Wtime();
    MPI_Allreduce(&(gbis[0]), &(g[0]),gmm::vect_size(g), MPI_DOUBLE,
		  MPI_SUM,MPI_COMM_WORLD);
    t_final=MPI_Wtime();
    cout<<"temps reduce init "<< t_final-t_ref<<endl;
#endif
#ifdef GMM_USES_MPI
    t_ref=MPI_Wtime();
    cout<<"begin global AS"<<endl;
#endif
    AS_global_solve(global_solver(), ASM, u, g, iter);
#ifdef GMM_USES_MPI
    t_final=MPI_Wtime();
    cout<<"temps AS Global Solve "<< t_final-t_ref<<endl;
#endif
    if (iter.get_noisy())
      cout << "Total number of internal iterations : " << ASM.itebilan << endl;
  }

  /** Global function. Compute the ASM matrix and call the previous function.
   *  The ASM matrix represent the preconditionned linear system.
   */
  template <typename Matrix1, typename Matrix2,
	    typename Vector2, typename Vector3, typename Precond,
	    typename local_solver, typename global_solver>
  void additive_schwarz(const Matrix1 &A, Vector3 &u,
				  const Vector2 &f, const Precond &P,
				  const std::vector<Matrix2> &vB,
				  iteration &iter, local_solver,
				  global_solver) {
    iter.set_rhsnorm(vect_norm2(f));
    if (iter.get_rhsnorm() == 0.0) { gmm::clear(u); return; }
    iteration iter2 = iter; iter2.reduce_noisy();
    iter2.set_maxiter(size_type(-1));
    add_schwarz_mat<Matrix1, Matrix2, Precond, local_solver>
      ASM(A, vB, iter2, P, iter.get_resmax());
    additive_schwarz(ASM, u, f, iter, global_solver());
  }

  /* ******************************************************************** */
  /*		Sequential Non-Linear Additive Schwarz method             */
  /* ******************************************************************** */
  /* ref : Nonlinearly Preconditionned Inexact Newton Algorithms,         */
  /*       Xiao-Chuan Cai, David E. Keyes,                                */
  /*       SIAM J. Sci. Comp. 24: p183-200.  l                             */
  /* ******************************************************************** */

  template <typename Matrixt, typename MatrixBi> 
  class NewtonAS_struct {
    
  public :
    typedef Matrixt tangent_matrix_type;
    typedef MatrixBi B_matrix_type;
    typedef typename linalg_traits<Matrixt>::value_type value_type;
    typedef std::vector<value_type> Vector;
    
    virtual size_type size(void) = 0;
    virtual const std::vector<MatrixBi> &get_vB() = 0;
    
    virtual void compute_F(Vector &f, Vector &x) = 0;
    virtual void compute_tangent_matrix(Matrixt &M, Vector &x) = 0;
    // compute Bi^T grad(F(X)) Bi
    virtual void compute_sub_tangent_matrix(Matrixt &Mloc, Vector &x,
					    size_type i) = 0;
    // compute Bi^T F(X)
    virtual void compute_sub_F(Vector &fi, Vector &x, size_type i) = 0;

    virtual ~NewtonAS_struct() {}
  };
  
  template <typename Matrixt, typename MatrixBi, typename Vector,
	    typename Precond, typename local_solver, typename global_solver>
  void Newton_additive_Schwarz(NewtonAS_struct<Matrixt, MatrixBi> &NS,
			       const Vector &u_,
			       iteration &iter, const Precond &P,
			       local_solver, global_solver) {
    Vector &u = const_cast<Vector &>(u_);
    typedef typename linalg_traits<Vector>::value_type value_type;
    typedef typename number_traits<value_type>::magnitude_type mtype;
    typedef actual_precond<Precond, local_solver, Matrixt> chgt_precond;
    
    double residual = iter.get_resmax();
    typename chgt_precond::APrecond PP = chgt_precond::transform(P);
    iter.set_rhsnorm(mtype(1));
    iteration iternc(iter);
    iternc.reduce_noisy(); iternc.set_maxiter(size_type(-1));
    iteration iter2(iternc);
    iteration iter3(iter2); iter3.reduce_noisy();
    iteration iter4(iter3);
    iternc.set_name("Local Newton");
    iter2.set_name("Linear System for Global Newton");
    iternc.set_resmax(residual/100.0);
    iter3.set_resmax(residual/1000.0);
    iter2.set_resmax(residual/100.0);
    iter4.set_resmax(residual/1000.0);
    std::vector<value_type> rhs(NS.size()), x(NS.size()), d(NS.size());
    std::vector<value_type> xi, xii, fi, di;
    Matrixt Mloc, M(NS.size(), NS.size());
    NS.compute_F(rhs, u);
    mtype act_res=gmm::vect_norm2(rhs), act_res_new(0), precond_res = act_res;
    mtype alpha, alpha_min=mtype(1)/mtype(16), alpha_mult=mtype(3)/mtype(4);
    mtype alpha_max_ratio(2);
    
    while(!iter.finished(std::min(act_res, precond_res))) {
      for (int SOR_step = 0;  SOR_step >= 0; --SOR_step) {
	gmm::clear(rhs);
	for (size_type isd = 0; isd < NS.get_vB().size(); ++isd) {
	  const MatrixBi &Bi = (NS.get_vB())[isd];
	  size_type si = mat_ncols(Bi);
	  gmm::resize(Mloc, si, si);
	  xi.resize(si); xii.resize(si); fi.resize(si); di.resize(si);
	  
	  iternc.init();
	  iternc.set_maxiter(30); // ?
	  if (iternc.get_noisy())
	    cout << "Non-linear local problem " << isd << endl;
	  gmm::clear(xi);
	  gmm::copy(u, x);
	  NS.compute_sub_F(fi, x, isd); gmm::scale(fi, value_type(-1));
	  mtype r = gmm::vect_norm2(fi), r_t(r);
	  if (r > value_type(0)) {
	    iternc.set_rhsnorm(std::max(r, mtype(1)));
	    while(!iternc.finished(r)) {
	      NS.compute_sub_tangent_matrix(Mloc, x, isd);
	      PP.build_with(Mloc);
	      iter3.init();
	      AS_local_solve(local_solver(), Mloc, di, fi, PP, iter3);
	      
	      for (alpha = mtype(1); alpha >= alpha_min; alpha *= alpha_mult) {
		gmm::add(xi, gmm::scaled(di, alpha), xii);
		gmm::mult(Bi, xii, u, x);
		NS.compute_sub_F(fi, x, isd); gmm::scale(fi, value_type(-1));
		if ((r_t = gmm::vect_norm2(fi)) <= r * alpha_max_ratio) break;
	      }
	      if (iternc.get_noisy()) cout << "(step=" << alpha << ")\t";
	      ++iternc; r = r_t; gmm::copy(xii, xi); 
	    }
	    if (SOR_step) gmm::mult(Bi, gmm::scaled(xii, 1.7), u, u);
	    gmm::mult(Bi, xii, rhs, rhs);
	  }
	}
	precond_res = gmm::vect_norm2(rhs);
	if (SOR_step) cout << "SOR step residual = " << precond_res << endl;
	if (precond_res < residual) break;
      }

      iter2.init();
      // solving linear system for the global Newton method
      NS.compute_tangent_matrix(M, u);
      add_schwarz_mat<Matrixt, MatrixBi, Precond, local_solver>
	ASM(M, NS.get_vB(), iter4, P, iter.get_resmax());
      AS_global_solve(global_solver(), ASM, d, rhs, iter2);

      for (alpha = mtype(1); alpha >= alpha_min; alpha *= alpha_mult) {
	gmm::add(gmm::scaled(d, alpha), u, x);
	NS.compute_F(rhs, x);
	act_res_new = gmm::vect_norm2(rhs);
	if (act_res_new <= act_res * alpha_max_ratio) break;
      }
      if (iter.get_noisy() > 1) cout << endl;
      act_res = act_res_new; 
      if (iter.get_noisy()) cout << "(step=" << alpha << ")\t unprecond res = " << act_res << " ";
      
      
      ++iter; gmm::copy(x, u);
    }
  }

}


#endif //  GMM_SOLVERS_SCHWARZ_ADDITIVE_H__
