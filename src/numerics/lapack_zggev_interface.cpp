/*
 * MAST: Multidisciplinary-design Adaptation and Sensitivity Toolkit
 * Copyright (C) 2013-2016  Manav Bhatia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


// MAST includes
#include "numerics/lapack_zggev_interface.h"


void
MAST::LAPACK_ZGGEV::compute(ComplexMatrixX &A,
                            ComplexMatrixX &B,
                            bool computeEigenvectors) {
    
    libmesh_assert(A.cols() == A.rows() &&
                   B.cols() == A.rows() &&
                   B.cols() == B.rows());
    
    _A = A;
    _B = B;
    
    int n = (int)A.cols();
    
    char L='N',R='N';
    
    if (computeEigenvectors)
    {
        L = 'V'; R = 'V';
        VL.setZero(n, n);
        VR.setZero(n, n);
    }
    
    int lwork=16*n, l_rwork=8*n; info_val=-1;
    
    alpha.setZero(n); beta.setZero(n);
    ComplexVectorX work; work.setZero(lwork);
    RealVectorX rwork; rwork.setZero(l_rwork);
    
    Complex *a_vals = A.data(),  *b_vals = B.data(),
    *alpha_v = alpha.data(), *beta_v = beta.data(),
    *VL_v = VL.data(), *VR_v = VR.data(),
    *work_v = work.data();
    
    Real *rwork_v = rwork.data();
    
    
    zggev_(&L, &R, &n,
           &(a_vals[0]), &n,
           &(b_vals[0]), &n,
           &(alpha_v[0]), &(beta_v[0]),
           &(VL_v[0]), &n, &(VR_v[0]), &n,
           &(work_v[0]), &lwork,
           &(rwork_v[0]),
           &info_val);
    
}


