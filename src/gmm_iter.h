/* -*- c++ -*- (enables emacs c++ mode)                                    */
/* *********************************************************************** */
/*                                                                         */
/* Library :  Generic Matrix Methods  (gmm)                                */
/* File    :  gmm_iter.h : iterations.                                     */
/*     									   */
/* Date : February 10, 2003.                                               */
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
//=======================================================================
// Copyright (C) 1997-2001
// Authors: Andrew Lumsdaine <lums@osl.iu.edu> 
//          Lie-Quan Lee     <llee@osl.iu.edu>
//
// This file is part of the Iterative Template Library
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

//: Iteration
//
//  The Iteration object calculates whether the solution has reached the
//  desired accuracy, or whether the maximum number of iterations has
//  been reached. The method finished() checks the convergence
//  The first() method is used to determine the first iteration of the loop.

// A ajouter dans solver.h
// corriger les algos existants

#ifndef __GMM_ITER_H
#define __GMM_ITER_H

namespace gmm
{

  class iteration {
  protected :
    double rhsn;       /* Right hand side norm.                            */
    size_type maxiter; /* Max. number of iterations.                       */
    int noise;         /* if noise > 0 iterations are printed.             */
    double resmax;/* maximum residu.                                  */
    size_type nit;     /* iteration number.                                */
    double res;   /* last computed residu.                            */
    std::string name;  /* eventually, name of the method.                  */
    
  public :

    void init(void) { nit = 0; res = 0.0; }

    iteration(double r, int noi = 0, size_type mit = size_type(-1))
      : rhsn(1.0), maxiter(mit), noise(noi), resmax(r), nit(0), res(0.0) {}

    void  operator ++(int) { 
      nit++; if (noise > 0)
	cout << name << " iter " << nit << " residu " << res << endl;
    }
    void  operator ++() { (*this)++; }

    bool first(void) { return nit == 0; }

    int get_noisy(void) const { return noise; }
    void set_noisy(int n) { noise = n; }
    void reduce_noisy(void) { if (noise > 0) noise--; }

    double get_resmax(void) const { return resmax; }
    void set_resmax(double r) { resmax = r; }
    
    size_type get_iteration(void) const { return nit; }
    void set_iteration(size_type i) { nit = i; }
    
    size_type get_maxiter(void) const { return maxiter; }
    void set_maxiter(size_type i) { maxiter = i; }

    double get_rhsnorm(void) const { return rhsn; }
    void set_rhsnorm(double r) { rhsn = r; }
    
    bool converged(double nr)
    { res = dal::abs(nr); return res < rhsn * resmax; }
    template <class VECT> bool converged(const VECT &v)
    { return converged(gmm::vect_norm2(v)); }

    bool finished(double nr)
    { return (nit >= maxiter || converged(nr)); }
    template <class VECT> bool finished(const VECT &v)
    { return finished(gmm::vect_norm2(v)); }


    void set_name(const std::string &n) { name = n; }
    const std::string &get_name(void) const { return name; }

  };
  

}

#endif /* __GMM_ITER_H */
