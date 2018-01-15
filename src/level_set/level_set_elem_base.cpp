/*
 * MAST: Multidisciplinary-design Adaptation and Sensitivity Toolkit
 * Copyright (C) 2013-2018  Manav Bhatia
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
#include "level_set/level_set_elem_base.h"
#include "numerics/fem_operator_matrix.h"
#include "base/system_initialization.h"
#include "base/field_function_base.h"
#include "base/parameter.h"
#include "base/boundary_condition_base.h"
#include "property_cards/element_property_card_base.h"
#include "property_cards/element_property_card_1D.h"
#include "base/mesh_field_function.h"
#include "base/nonlinear_system.h"
#include "mesh/local_elem_fe.h"
#include "base/assembly_base.h"


MAST::LevelSetElementBase::
LevelSetElementBase(MAST::SystemInitialization&             sys,
                    MAST::AssemblyBase&                     assembly,
                    const libMesh::Elem&                    elem,
                    const MAST::FieldFunction<RealVectorX>& velocity):
MAST::ElementBase(sys, assembly, elem),
_phi_vel(velocity) {
    
    // now initialize the finite element data structures
    _fe = assembly.build_fe(_elem).release();
    _fe->init(_elem);
}



MAST::LevelSetElementBase::~LevelSetElementBase() {
    
}




bool
MAST::LevelSetElementBase::internal_residual (bool request_jacobian,
                                              RealVectorX& f,
                                              RealMatrixX& jac) {
    
    const std::vector<Real>& JxW           = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz = _fe->get_xyz();
    const unsigned int
    n_phi  = _fe->n_shape_functions(),
    dim    = _elem.dim();
    
    RealMatrixX
    eye            = RealMatrixX::Identity(1, 1),
    tau            = RealMatrixX::Zero(    1, 1),
    mat1_n1n2      = RealMatrixX::Zero(    1, n_phi),
    mat2_n1n2      = RealMatrixX::Zero(    1, n_phi),
    mat_n2n2       = RealMatrixX::Zero(n_phi, n_phi);
    RealVectorX
    vec1_n1  = RealVectorX::Zero(1),
    vec2_n1  = RealVectorX::Zero(1),
    vec2_n2  = RealVectorX::Zero(n_phi),
    flux     = RealVectorX::Zero(1),
    vel      = RealVectorX::Zero(dim);
    
    std::vector<MAST::FEMOperatorMatrix> dBmat(dim);
    MAST::FEMOperatorMatrix Bmat;
    
    
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // initialize the Bmat operator for this term
        _initialize_fem_operators(qp, *_fe, Bmat, dBmat);
        
        _phi_vel(xyz[qp], _time, vel);
        _tau(qp, Bmat, dBmat, vel, tau);
        
        // calculate the flux for each dimension and add its weighted
        // component to the residual
        flux.setZero();
        mat2_n1n2.setZero();
        for (unsigned int j=0; j<dim; j++) {
            
            dBmat[j].right_multiply(vec1_n1, _sol);       // dphi_dx_j
            flux += vel(j) * vec1_n1;                     // v_j dphi_dx_j
            dBmat[j].left_multiply(mat1_n1n2, eye);       // dBmat
            mat2_n1n2 += mat1_n1n2 * vel(j);              // dBmat V_j
        }

        // add to the residual vector
        Bmat.vector_mult_transpose(vec2_n2, flux);
        f += JxW[qp] * vec2_n2;                              // int_omega          u       V.grad(phi)
        f += JxW[qp] * mat2_n1n2.transpose() * tau * flux;   // int_omega   V.grad(u) tau (V.grad(phi))
        
        if (request_jacobian) {
            
            for (unsigned int j=0; j<dim; j++) {
                    
                Bmat.right_multiply_transpose(mat_n2n2, dBmat[j]);
                jac += JxW[qp] * vel(j) * mat_n2n2;  // int_omega  u  V.grad(phi)
            }
            
            jac += JxW[qp] * mat2_n1n2.transpose() * tau * mat2_n1n2;  // int_omega  V.grad(u) tau (V.grad(phi))
        }
    }
    
    return request_jacobian;
}





bool
MAST::LevelSetElementBase::velocity_residual (bool request_jacobian,
                                              RealVectorX& f,
                                              RealMatrixX& jac_xdot,
                                              RealMatrixX& jac) {

    const std::vector<Real>& JxW           = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz = _fe->get_xyz();
    const unsigned int
    n_phi  = _fe->n_shape_functions(),
    dim    = _elem.dim();
    
    RealMatrixX
    eye            = RealMatrixX::Identity(1, 1),
    tau            = RealMatrixX::Zero(    1, 1),
    mat1_n1n2      = RealMatrixX::Zero(    1, n_phi),
    mat2_n1n2      = RealMatrixX::Zero(    1, n_phi),
    mat_n2n1       = RealMatrixX::Zero(n_phi,     1),
    mat_n2n2       = RealMatrixX::Zero(n_phi, n_phi);
    RealVectorX
    vec1_n1  = RealVectorX::Zero(1),
    vec2_n2  = RealVectorX::Zero(n_phi),
    flux     = RealVectorX::Zero(1),
    vel      = RealVectorX::Zero(dim);
    
    std::vector<MAST::FEMOperatorMatrix> dBmat(dim);
    MAST::FEMOperatorMatrix Bmat;
    
    
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // initialize the Bmat operator for this term
        _initialize_fem_operators(qp, *_fe, Bmat, dBmat);
        
        _phi_vel(xyz[qp], _time, vel);
        _tau(qp, Bmat, dBmat, vel, tau);
        
        // calculate the flux for each dimension and add its weighted
        // component to the residual
        mat2_n1n2.setZero();
        for (unsigned int j=0; j<dim; j++) {
            
            dBmat[j].left_multiply(mat1_n1n2, eye);       // dBmat
            mat2_n1n2 += mat1_n1n2 * vel(j);              // dBmat_j V_j
        }
        
        // add to the residual vector
        Bmat.right_multiply(vec1_n1, _vel);                  // dphi/dt
        Bmat.vector_mult_transpose(vec2_n2, vec1_n1);
        f += JxW[qp] * vec2_n2;                                 // int_omega          u  dphi/dt
        f += JxW[qp] * mat2_n1n2.transpose() * tau * vec1_n1;   // int_omega   V.grad(u) tau dphi/dt
        
        if (request_jacobian) {
            
            Bmat.right_multiply_transpose(mat_n2n2, Bmat);
            jac_xdot += JxW[qp] * mat_n2n2;  // int_omega  u  dphi/dt
            
            mat_n2n1 = mat2_n1n2.transpose()*tau;
            Bmat.left_multiply(mat_n2n2, mat_n2n1);
            jac_xdot += JxW[qp] * mat_n2n2;  // int_omega  V.grad(u) tau dphi/dt
        }
    }
    
    return request_jacobian;
}




bool
MAST::LevelSetElementBase::
side_external_residual (bool request_jacobian,
                        RealVectorX& f,
                        RealMatrixX& jac,
                        std::multimap<libMesh::boundary_id_type, MAST::BoundaryConditionBase*>& bc) {
    
    typedef std::multimap<libMesh::boundary_id_type, MAST::BoundaryConditionBase*> maptype;
    
    // iterate over the boundary ids given in the provided force map
    std::pair<maptype::const_iterator, maptype::const_iterator> it;
    
    const libMesh::BoundaryInfo& binfo = *_system.system().get_mesh().boundary_info;
    
    // for each boundary id, check if any of the sides on the element
    // has the associated boundary
    
    for (unsigned short int n=0; n<_elem.n_sides(); n++) {
        
        // if no boundary ids have been specified for the side, then
        // move to the next side.
        if (!binfo.n_boundary_ids(&_elem, n))
            continue;
        
        // check to see if any of the specified boundary ids has a boundary
        // condition associated with them
        std::vector<libMesh::boundary_id_type> bc_ids;
        binfo.boundary_ids(&_elem, n, bc_ids);
        std::vector<libMesh::boundary_id_type>::const_iterator bc_it = bc_ids.begin();
        
        for ( ; bc_it != bc_ids.end(); bc_it++) {
            
            // find the loads on this boundary and evaluate the f and jac
            it = bc.equal_range(*bc_it);
            
            for ( ; it.first != it.second; it.first++) {
                
                // apply all the types of loading
                switch (it.first->second->type()) {
                    case MAST::DIRICHLET:
                        // nothing to be done here
                        break;
                        
                    default:
                        // not implemented yet
                        libmesh_error();
                        break;
                }
            }
        }
    }
    return request_jacobian;
}





bool
MAST::LevelSetElementBase::
volume_external_residual (bool request_jacobian,
                          RealVectorX& f,
                          RealMatrixX& jac,
                          std::multimap<libMesh::subdomain_id_type, MAST::BoundaryConditionBase*>& bc) {
    
    typedef std::multimap<libMesh::subdomain_id_type, MAST::BoundaryConditionBase*> maptype;
    
    // iterate over the boundary ids given in the provided force map
    std::pair<maptype::const_iterator, maptype::const_iterator> it;
    
    // for each boundary id, check if any of the sides on the element
    // has the associated boundary
    
    libMesh::subdomain_id_type sid = _elem.subdomain_id();
    // find the loads on this boundary and evaluate the f and jac
    it =bc.equal_range(sid);
    
    for ( ; it.first != it.second; it.first++) {
        // apply all the types of loading
        switch (it.first->second->type()) {
                
            default:
                // not implemented yet
                libmesh_error();
                break;
        }
    }
    
    return request_jacobian;
}



bool
MAST::LevelSetElementBase::
side_external_residual_sensitivity (bool request_jacobian,
                                    RealVectorX& f,
                                    RealMatrixX& jac,
                                    std::multimap<libMesh::boundary_id_type, MAST::BoundaryConditionBase*>& bc) {
    
    
    typedef std::multimap<libMesh::boundary_id_type, MAST::BoundaryConditionBase*> maptype;
    
    // iterate over the boundary ids given in the provided force map
    std::pair<maptype::const_iterator, maptype::const_iterator> it;
    
    const libMesh::BoundaryInfo& binfo = *_system.system().get_mesh().boundary_info;
    
    // for each boundary id, check if any of the sides on the element
    // has the associated boundary
    
    for (unsigned short int n=0; n<_elem.n_sides(); n++) {
        
        // if no boundary ids have been specified for the side, then
        // move to the next side.
        if (!binfo.n_boundary_ids(&_elem, n))
            continue;
        
        // check to see if any of the specified boundary ids has a boundary
        // condition associated with them
        std::vector<libMesh::boundary_id_type> bc_ids;
        binfo.boundary_ids(&_elem, n, bc_ids);
        std::vector<libMesh::boundary_id_type>::const_iterator bc_it = bc_ids.begin();
        
        for ( ; bc_it != bc_ids.end(); bc_it++) {
            
            // find the loads on this boundary and evaluate the f and jac
            it = bc.equal_range(*bc_it);
            
            for ( ; it.first != it.second; it.first++) {
                
                // apply all the types of loading
                switch (it.first->second->type()) {
                        
                    default:
                        // not implemented yet
                        libmesh_error();
                        break;
                }
            }
        }
    }
    return request_jacobian;
}




bool
MAST::LevelSetElementBase::
volume_external_residual_sensitivity (bool request_jacobian,
                                      RealVectorX& f,
                                      RealMatrixX& jac,
                                      std::multimap<libMesh::subdomain_id_type, MAST::BoundaryConditionBase*>& bc) {
    
    typedef std::multimap<libMesh::subdomain_id_type, MAST::BoundaryConditionBase*> maptype;
    
    // iterate over the boundary ids given in the provided force map
    std::pair<maptype::const_iterator, maptype::const_iterator> it;
    
    // for each boundary id, check if any of the sides on the element
    // has the associated boundary
    
    libMesh::subdomain_id_type sid = _elem.subdomain_id();
    // find the loads on this boundary and evaluate the f and jac
    it =bc.equal_range(sid);
    
    for ( ; it.first != it.second; it.first++) {
        // apply all the types of loading
        switch (it.first->second->type()) {
                
            default:
                // not implemented yet
                libmesh_error();
                break;
        }
    }
    
    return request_jacobian;
}




bool
MAST::LevelSetElementBase::
internal_residual_sensitivity (bool request_jacobian,
                               RealVectorX& f,
                               RealMatrixX& jac) {
    
    return request_jacobian;
}



bool
MAST::LevelSetElementBase::
velocity_residual_sensitivity (bool request_jacobian,
                               RealVectorX& f,
                               RealMatrixX& jac) {
    
    return request_jacobian;
}




bool
MAST::LevelSetElementBase::volume_output_quantity
(bool request_derivative,
 bool request_sensitivity,
 std::multimap<libMesh::subdomain_id_type, MAST::OutputFunctionBase*>& output) {
    
    
    // there are currently no output quantities defined for heat conduction
    // elements
    
    return (request_derivative || request_sensitivity);
}




bool
MAST::LevelSetElementBase::
side_output_quantity (bool request_derivative,
                      bool request_sensitivity,
                      std::multimap<libMesh::boundary_id_type, MAST::OutputFunctionBase*>& output) {
    
    // there are currently no output quantities defined for heat conduction
    // elements
    
    return (request_derivative || request_sensitivity);
}




void
MAST::LevelSetElementBase::_tau(unsigned int qp,
                                const MAST::FEMOperatorMatrix& Bmat,
                                const std::vector<MAST::FEMOperatorMatrix>& dBmat,
                                const RealVectorX& vel,
                                RealMatrixX& tau) {
    
    const std::vector<std::vector<libMesh::RealVectorValue> >& dphi = _fe->get_dphi();
    const unsigned int n_phi = (unsigned int)_fe->n_shape_functions();
    RealVectorX
    phi = RealVectorX::Zero(n_phi);

    Real
    val = 0.;

    for (unsigned int i_dim=0; i_dim<_elem.dim(); i_dim++) {
        
        for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
            val += std::abs(dphi[i_nd][qp](i_dim) * vel(i_dim));
    }

    tau(0,0) = 1./val;
}



void
MAST::LevelSetElementBase::
_initialize_fem_operators(const unsigned int qp,
                          const MAST::FEBase& fe,
                          MAST::FEMOperatorMatrix& Bmat,
                          std::vector<MAST::FEMOperatorMatrix>& dBmat) {
    
    const std::vector<std::vector<Real> >&                   phi_fe = fe.get_phi();
    const std::vector<std::vector<libMesh::RealVectorValue> >& dphi = fe.get_dphi();

    const unsigned int n_phi = (unsigned int)phi_fe.size();
    
    RealVectorX
    phi = RealVectorX::Zero(n_phi);
    
    // shape function values
    // N
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
        phi(i_nd) = phi_fe[i_nd][qp];
    
    Bmat.reinit(1, phi);
    
    // now set the shape function derivatives
    for (unsigned int i_dim=0; i_dim<_elem.dim(); i_dim++) {

        phi.setZero();
        for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
            phi(i_nd) = dphi[i_nd][qp](i_dim);
        dBmat[i_dim].reinit(1, phi); //  dT/dx_i
    }

}





std::unique_ptr<MAST::FEBase>
MAST::build_level_set_fe(MAST::SystemInitialization& sys,
                         const libMesh::Elem& elem,
                         const MAST::ElementPropertyCardBase& p) {
    
    std::unique_ptr<MAST::FEBase> rval;
    
    switch (elem.dim()) {
            
        case 1: {
            
            MAST::LocalElemFE
            *fe = new MAST::LocalElemFE(sys);
            fe->set_1d_y_vector
            (dynamic_cast<const MAST::ElementPropertyCard1D&>(p).y_vector());
            rval.reset(fe);
        }
            break;
            
        case 2: {
            
            rval.reset(new MAST::LocalElemFE(sys));
        }
            break;
            
        case 3: {
            
            rval.reset(new MAST::FEBase(sys));
        }
            break;
            
        default:
            libmesh_error(); // should not get here.
    }
    
    return rval;
}




