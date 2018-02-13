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
#include "elasticity/structural_element_2d.h"
#include "property_cards/element_property_card_2D.h"
#include "numerics/fem_operator_matrix.h"
#include "mesh/local_elem_base.h"
#include "elasticity/piston_theory_boundary_condition.h"
#include "elasticity/stress_output_base.h"
#include "elasticity/bending_operator.h"
#include "property_cards/material_property_card_base.h"
#include "base/system_initialization.h"
#include "base/boundary_condition_base.h"
#include "base/parameter.h"
#include "base/constant_field_function.h"
#include "mesh/local_elem_fe.h"
#include "base/assembly_base.h"


MAST::StructuralElement2D::
StructuralElement2D(MAST::SystemInitialization& sys,
                    MAST::AssemblyBase& assembly,
                    const libMesh::Elem& elem,
                    const MAST::ElementPropertyCardBase& p):
MAST::BendingStructuralElem(sys, assembly, elem, p),
_bending_operator(nullptr) {

    // now initialize the finite element data structures
    _fe            = assembly.build_fe(_elem).release();
    _fe->init(_elem);
    _Tmat          = RealMatrixX::Identity(3,3); //dynamic_cast<MAST::LocalElemFE*>(_fe)->local_elem().T_matrix();

    MAST::BendingOperatorType bending_model =
    _property.bending_model(_elem, _fe->get_fe_type());
    _bending_operator = MAST::build_bending_operator_2D(bending_model,
                                                        *this,
                                                        _fe->get_qpoints()).release();
}



MAST::StructuralElement2D::~StructuralElement2D() {
    
    if (_bending_operator)   delete _bending_operator;
}



void
MAST::StructuralElement2D::
initialize_direct_strain_operator(const unsigned int qp,
                                  const MAST::FEBase& fe,
                                  MAST::FEMOperatorMatrix& Bmat) {
    
    const std::vector<std::vector<libMesh::RealVectorValue> >& dphi = fe.get_dphi();
    
    unsigned int n_phi = (unsigned int)dphi.size();
    
    RealVectorX phi   = RealVectorX::Zero(n_phi);
    
    libmesh_assert_equal_to(Bmat.m(), 3);
    libmesh_assert_equal_to(Bmat.n(), 6*n_phi);
    libmesh_assert_less    (qp, dphi[0].size());

    // now set the shape function values
    // dN/dx
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
        phi(i_nd) = dphi[i_nd][qp](0);
    Bmat.set_shape_function(0, 0, phi); //  epsilon_xx = du/dx
    Bmat.set_shape_function(2, 1, phi); //  gamma_xy = dv/dx + ...
    
    // dN/dy
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
        phi(i_nd) = dphi[i_nd][qp](1);
    Bmat.set_shape_function(1, 1, phi); //  epsilon_yy = dv/dy
    Bmat.set_shape_function(2, 0, phi); //  gamma_xy = du/dy + ...
}



void
MAST::StructuralElement2D::
initialize_von_karman_strain_operator(const unsigned int qp,
                                      const MAST::FEBase& fe,
                                      RealVectorX& vk_strain,
                                      RealMatrixX& vk_dwdxi_mat,
                                      MAST::FEMOperatorMatrix& Bmat_vk) {
    
    const std::vector<std::vector<libMesh::RealVectorValue> >& dphi = fe.get_dphi();
    const unsigned int n_phi = (unsigned int)dphi.size();
    
    libmesh_assert_equal_to(vk_strain.size(), 3);
    libmesh_assert_equal_to(vk_dwdxi_mat.rows(), 3);
    libmesh_assert_equal_to(vk_dwdxi_mat.cols(), 2);
    libmesh_assert_equal_to(Bmat_vk.m(), 2);
    libmesh_assert_equal_to(Bmat_vk.n(), 6*n_phi);
    libmesh_assert_less    (qp, dphi[0].size());

    Real dw=0.;
    vk_strain.setZero();
    vk_dwdxi_mat.setZero();
    
    RealVectorX phi_vec  = RealVectorX::Zero(n_phi);
    
    dw = 0.;
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ ) {
        phi_vec(i_nd) = dphi[i_nd][qp](0);  // dphi/dx
        dw += phi_vec(i_nd)*_local_sol(2*n_phi+i_nd); // dw/dx
    }
    Bmat_vk.set_shape_function(0, 2, phi_vec); // dw/dx
    vk_dwdxi_mat(0, 0) = dw;  // epsilon-xx : dw/dx
    vk_dwdxi_mat(2, 1) = dw;  // gamma-xy : dw/dx
    vk_strain(0) = 0.5*dw*dw; // 1/2 * (dw/dx)^2
    vk_strain(2) = dw;        // (dw/dx)*(dw/dy)  only dw/dx is provided here
    
    dw = 0.;
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ ) {
        phi_vec(i_nd) = dphi[i_nd][qp](1);  // dphi/dy
        dw += phi_vec(i_nd)*_local_sol(2*n_phi+i_nd); // dw/dy
    }
    Bmat_vk.set_shape_function(1, 2, phi_vec); // dw/dy
    vk_dwdxi_mat(1, 1) = dw;  // epsilon-yy : dw/dy
    vk_dwdxi_mat(2, 0) = dw;  // gamma-xy : dw/dy
    vk_strain(1) = 0.5*dw*dw; // 1/2 * (dw/dy)^2
    vk_strain(2) *= dw;       // (dw/dx)*(dw/dy)
}




void
MAST::StructuralElement2D::
initialize_von_karman_strain_operator_sensitivity(const unsigned int qp,
                                                  const MAST::FEBase& fe,
                                                  RealMatrixX &vk_dwdxi_mat_sens) {
    
    const std::vector<std::vector<libMesh::RealVectorValue> >& dphi = fe.get_dphi();
    const unsigned int n_phi = (unsigned int)dphi.size();
    
    libmesh_assert_equal_to(vk_dwdxi_mat_sens.rows(), 3);
    libmesh_assert_equal_to(vk_dwdxi_mat_sens.cols(), 2);
    libmesh_assert_less    (qp, dphi[0].size());

    Real dw=0.;
    vk_dwdxi_mat_sens.setZero();
    
    RealVectorX phi_vec  = RealVectorX::Zero(n_phi);
    
    dw = 0.;
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ ) {
        phi_vec(i_nd) = dphi[i_nd][qp](0);  // dphi/dx
        dw += phi_vec(i_nd)*_local_sol_sens(2*n_phi+i_nd); // dw/dx
    }
    vk_dwdxi_mat_sens(0, 0) = dw;  // epsilon-xx : dw/dx
    vk_dwdxi_mat_sens(2, 1) = dw;  // gamma-xy : dw/dx
    
    dw = 0.;
    for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ ) {
        phi_vec(i_nd) = dphi[i_nd][qp](1);  // dphi/dy
        dw += phi_vec(i_nd)*_local_sol_sens(2*n_phi+i_nd); // dw/dy
    }
    vk_dwdxi_mat_sens(1, 1) = dw;  // epsilon-yy : dw/dy
    vk_dwdxi_mat_sens(2, 0) = dw;  // gamma-xy : dw/dy
}




bool
MAST::StructuralElement2D::calculate_stress(bool request_derivative,
                                            const MAST::FunctionBase* p,
                                            MAST::StressStrainOutputBase& output) {
    
    std::unique_ptr<MAST::FEBase>   fe(_assembly.build_fe(_elem));
    fe->init(_elem);
    
    const unsigned int
    qp_loc_fe_size = (unsigned int)fe->get_qpoints().size(),
    n_added_qp     = 2;

    std::vector<libMesh::Point>
    qp_loc_fe = fe->get_qpoints(),
    qp_loc(qp_loc_fe.size()*n_added_qp);

    // we will evaluate the stress at upper and lower layers of element,
    // so we will add two new points for each qp_loc.
    // TODO: this will need to be moved to element property section card
    // to handle composite elements at a later date
    for (unsigned int i=0; i<qp_loc_fe.size(); i++) {
        
        qp_loc[i*2]      = qp_loc_fe[i];
        qp_loc[i*2](2)   = +1.; // upper skin
        
        qp_loc[i*2+1]    = qp_loc_fe[i];
        qp_loc[i*2+1](2) = -1.; // lower skin
    }
    
    MAST::BendingOperatorType bending_model =
    _property.bending_model(_elem, _fe->get_fe_type());
    
    std::unique_ptr<MAST::BendingOperator2D>
    bend(MAST::build_bending_operator_2D(bending_model,
                                         *this,
                                         qp_loc_fe).release());
    
    // now that the FE object has been initialized, evaluate the stress values
    
    const std::vector<Real> &JxW              = fe->get_JxW();
    const std::vector<libMesh::Point>& xyz    = fe->get_xyz();
    const unsigned int
    n_phi    = (unsigned int)fe->n_shape_functions(),
    n1       = this->n_direct_strain_components(),
    n2       = 6*n_phi,
    n3       = this->n_von_karman_strain_components();
    
    Real
    z     =  0.,
    z_off =  0.,
    temp  =  0.,
    ref_t =  0.,
    alpha =  0.,
    dtemp =  0.,
    dref_t=  0.,
    dalpha=  0.;
    
    RealMatrixX
    material_mat,
    vk_dwdxi_mat = RealMatrixX::Zero(n1,n3),
    dstrain_dX   = RealMatrixX::Zero(n1,n2),
    dstress_dX   = RealMatrixX::Zero(n1,n2),
    mat_n1n2     = RealMatrixX::Zero(n1,n2),
    eye          = RealMatrixX::Identity(n1, n1),
    dstrain_dX_3D= RealMatrixX::Zero(6,n2),
    dstress_dX_3D= RealMatrixX::Zero(6,n2);
    
    RealVectorX
    strain      = RealVectorX::Zero(n1),
    stress      = RealVectorX::Zero(n1),
    strain_vk   = RealVectorX::Zero(n1),
    strain_bend = RealVectorX::Zero(n1),
    strain_3D   = RealVectorX::Zero(6),
    stress_3D   = RealVectorX::Zero(6),
    dstrain_dp  = RealVectorX::Zero(n1),
    dstress_dp  = RealVectorX::Zero(n1);
    
    
    MAST::FEMOperatorMatrix
    Bmat_mem,
    Bmat_bend,
    Bmat_vk;
    
    Bmat_mem.reinit (n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit  (n3, _system.n_vars(), n_phi);
    
    // TODO: remove this const-cast, which may need change in API of
    // material card
    const MAST::FieldFunction<RealMatrixX >&
    mat_stiff  =
    const_cast<MAST::MaterialPropertyCardBase&>(_property.get_material()).
    stiffness_matrix(2);
    
    // get the thickness values for the bending strain calculation
    const MAST::FieldFunction<Real>
    &h      =  _property.get<MAST::FieldFunction<Real> >("h"),
    &h_off  =  _property.get<MAST::FieldFunction<Real> >("off");
    
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, fe->get_fe_type()) != MAST::NO_BENDING);
    
    // a reference to the stress output data structure
    MAST::StressStrainOutputBase& stress_output =
    dynamic_cast<MAST::StressStrainOutputBase&>(output);
    
    // check to see if the element has any thermal loads specified
    // The object returns null
    MAST::BoundaryConditionBase *thermal_load =
    stress_output.get_thermal_load_for_elem(_elem);
    
    const MAST::FieldFunction<Real>
    *temp_func     = nullptr,
    *ref_temp_func = nullptr,
    *alpha_func    = nullptr;
    
    // get pointers to the temperature, if thermal load is specified
    if (thermal_load) {
        temp_func     =
        &(thermal_load->get<MAST::FieldFunction<Real> >("temperature"));
        ref_temp_func =
        &(thermal_load->get<MAST::FieldFunction<Real> >("ref_temperature"));
        alpha_func    =
        &(_property.get_material().get<MAST::FieldFunction<Real> >("alpha_expansion"));
    }
    
    ///////////////////////////////////////////////////////////////////////
    // second for loop to calculate the residual and stiffness contributions
    unsigned int
    qp = 0;
    for (unsigned int qp_loc_index=0; qp_loc_index<qp_loc_fe_size; qp_loc_index++)
        for (unsigned int section_qp_index=0; section_qp_index<n_added_qp; section_qp_index++)
        {
            qp = qp_loc_index*n_added_qp + section_qp_index;
            
            // get the material matrix
            mat_stiff(xyz[qp_loc_index], _time, material_mat);
            
            this->initialize_direct_strain_operator(qp_loc_index, *fe, Bmat_mem);
            
            // first handle constant throught the thickness stresses: membrane and vonKarman
            Bmat_mem.vector_mult(strain, _local_sol);
            
            
            // if thermal load was specified, then set the thermal strain
            // component of the total strain
            if (thermal_load) {
                (*temp_func)    (xyz[qp_loc_index], _time, temp);
                (*ref_temp_func)(xyz[qp_loc_index], _time, ref_t);
                (*alpha_func)   (xyz[qp_loc_index], _time, alpha);
                strain(0)  -=  alpha*(temp-ref_t);  // epsilon-xx
                strain(1)  -=  alpha*(temp-ref_t);  // epsilon-yy
            }
            
            if (if_bending) {
                
                // von Karman strain
                if (if_vk) {  // get the vonKarman strain operator if needed
                    
                    this->initialize_von_karman_strain_operator(qp_loc_index,
                                                                *fe,
                                                                strain_vk,
                                                                vk_dwdxi_mat,
                                                                Bmat_vk);
                    strain += strain_vk;
                }
                
                // add to this the bending strain
                // TODO: add coupling due to h_offset
                h    (xyz[qp_loc_index], _time,     z);
                h_off(xyz[qp_loc_index], _time, z_off);
                // TODO: this assumes isotropic section. Multilayered sections need
                // special considerations
                bend->initialize_bending_strain_operator_for_z(*fe,
                                                               qp_loc_index,
                                                               qp_loc[qp](2) * z/2.+z_off,
                                                               Bmat_bend);
                Bmat_bend.vector_mult(strain_bend, _local_sol);
                
                
                // add stress due to bending.
                strain += strain_bend;
            }
            
            // note that this assumes linear material laws
            stress = material_mat * strain;
            
            
            // now set the data for the 3D stress-strain vector
            // this is using only the direct strain/stress.
            // this can be improved by estimating the shear stresses from
            // torsion and shear flow from bending.
            stress_3D(0) = stress(0);  // sigma-xx
            stress_3D(1) = stress(1);  // sigma-yy
            stress_3D(3) = stress(2);  // tau-xy
            strain_3D(0) = strain(0);  // epsilon-xx
            strain_3D(1) = strain(1);  // epsilon-yy
            strain_3D(3) = strain(2);  // gamma-xy
            
            // set the stress and strain data
            MAST::StressStrainOutputBase::Data*
            data = nullptr;
            
            // if neither the derivative nor sensitivity is requested, then
            // we assume that a new data entry is to be provided. Otherwise,
            // we assume that the stress at this quantity already
            // exists, and we only need to append sensitivity/derivative
            // data to it
            if (!request_derivative && !p)
                data = &(stress_output.add_stress_strain_at_qp_location(&_elem,
                                                                        qp,
                                                                        qp_loc[qp],
                                                                        xyz[qp_loc_index],
                                                                        stress_3D,
                                                                        strain_3D,
                                                                        JxW[qp_loc_index]));
            else
                data = &(stress_output.get_stress_strain_data_for_elem_at_qp(&_elem,
                                                                             qp));
            
            
            // calculate the derivative if requested
            if (request_derivative || p) {
                
                Bmat_mem.left_multiply(dstrain_dX, eye);  // membrane strain is linear
                
                if (if_bending) {
                    
                    // von Karman strain
                    if (if_vk) {
                        
                        Bmat_vk.left_multiply(mat_n1n2, vk_dwdxi_mat);
                        dstrain_dX   +=  mat_n1n2;
                    }
                    
                    // bending strain
                    Bmat_bend.left_multiply(mat_n1n2, eye);
                    dstrain_dX  +=   mat_n1n2;
                }
                
                // note: this assumes linear material laws
                dstress_dX  = material_mat * dstrain_dX;
                
                // copy to the 3D structure
                dstress_dX_3D.row(0) = dstress_dX.row(0);  // sigma-xx
                dstress_dX_3D.row(1) = dstress_dX.row(1);  // sigma-yy
                dstress_dX_3D.row(3) = dstress_dX.row(2);  // tau-xy
                dstrain_dX_3D.row(0) = dstrain_dX.row(0);  // epsilon-xx
                dstrain_dX_3D.row(1) = dstrain_dX.row(1);  // epsilon-yy
                dstrain_dX_3D.row(3) = dstrain_dX.row(2);  // gamma-xy
                
                if (request_derivative)
                    data->set_derivatives(dstress_dX_3D, dstrain_dX_3D);
                
                
                if (p) {
                    // sensitivity of the response, s, is
                    //   ds/dp   = partial s/partial p  +
                    //             partial s/partial X   dX/dp
                    //   the first part of the sensitivity is obtained from
                    //
                    // the first term includes direct sensitivity of the stress
                    // with respect to the parameter, while holding the solution
                    // constant. This should include influence of shape changes,
                    // if the parameter is shape-dependent.
                    // TODO: include shape sensitivity.
                    // presently, only material parameter is included
                    
                    
                    dstrain_dp  =  RealVectorX::Zero(n1);
                    
                    // if thermal load was specified, then set the thermal strain
                    // component of the total strain
                    if (thermal_load) {
                        temp_func->derivative(*p, xyz[qp_loc_index], _time, dtemp);
                        ref_temp_func->derivative(*p, xyz[qp_loc_index], _time, dref_t);
                        alpha_func->derivative(*p, xyz[qp_loc_index], _time, dalpha);
                        dstrain_dp(0)  -=  alpha*(dtemp-dref_t) - dalpha*(temp-ref_t); // epsilon-xx
                        dstrain_dp(1)  -=  alpha*(dtemp-dref_t) - dalpha*(temp-ref_t); // epsilon-yy
                    }
                    
                    
                    
                    if (if_bending) {
                        
                        // add to this the bending strain
                        h.derivative    (*p,
                                         xyz[qp_loc_index], _time,     z);
                        h_off.derivative(*p,
                                         xyz[qp_loc_index], _time, z_off);
                        // TODO: this assumes isotropic section. Multilayered sections need
                        // special considerations
                        bend->initialize_bending_strain_operator_for_z(*fe,
                                                                       qp_loc_index,
                                                                       qp_loc[qp](2) * z/2.+z_off,
                                                                       Bmat_bend);
                        Bmat_bend.vector_mult(strain_bend, _local_sol);
                        
                        
                        // add stress due to bending.
                        dstrain_dp += strain_bend;
                    }
                    
                    
                    // now use this to calculate the stress sensitivity.
                    dstress_dp  =  material_mat * dstrain_dp;
                    
                    // get the material matrix sensitivity
                    mat_stiff.derivative(*p,
                                         xyz[qp_loc_index],
                                         _time,
                                         material_mat);
                    
                    // partial sensitivity of strain is zero unless it is a
                    // shape parameter.
                    // TODO: shape sensitivity of strain operator
                    
                    // now use this to calculate the stress sensitivity.
                    dstress_dp +=  material_mat * strain;
                    
                    //
                    // use the derivative data to evaluate the second term in the
                    // sensitivity
                    //
                    dstress_dp  += dstress_dX * _local_sol_sens;
                    dstrain_dp  += dstrain_dX * _local_sol_sens;
                    
                    // copy the 3D object
                    stress_3D(0) = dstress_dp(0);  // sigma-xx
                    stress_3D(1) = dstress_dp(1);  // sigma-yy
                    stress_3D(3) = dstress_dp(2);  // tau-xy
                    strain_3D(0) = dstrain_dp(0);  // epsilon-xx
                    strain_3D(1) = dstrain_dp(1);  // epsilon-yy
                    strain_3D(3) = dstrain_dp(2);  // gamma-xy
                    
                    // tell the data object about the sensitivity values
                    data->set_sensitivity(*p,
                                          stress_3D,
                                          strain_3D);
                }
            }
        }
    
    // make sure that the number of data points for this element is
    // the same as the number of requested points
    libmesh_assert(qp_loc.size() ==
                   stress_output.n_stress_strain_data_for_elem(&_elem));
    
    // if either derivative or sensitivity was requested, it was provided
    // by this routine
    return request_derivative || p;
}



bool
MAST::StructuralElement2D::internal_residual (bool request_jacobian,
                                              RealVectorX& f,
                                              RealMatrixX& jac)
{
    const std::vector<Real>& JxW           = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz = _fe->get_xyz();
    
    const unsigned int
    n_phi    = (unsigned int)_fe->get_phi().size(),
    n1       = this->n_direct_strain_components(),
    n2       =6*n_phi,
    n3       = this->n_von_karman_strain_components();
    
    RealMatrixX
    material_A_mat,
    material_B_mat,
    material_D_mat,
    mat1_n1n2     = RealMatrixX::Zero(n1,n2),
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    mat4_n3n2     = RealMatrixX::Zero(n3,n2),
    vk_dwdxi_mat  = RealMatrixX::Zero(n1,n3),
    stress        = RealMatrixX::Zero(2,2),
    stress_l      = RealMatrixX::Zero(2,2),
    local_jac     = RealMatrixX::Zero(n2,n2);

    RealVectorX
    vec1_n1    = RealVectorX::Zero(n1),
    vec2_n1    = RealVectorX::Zero(n1),
    vec3_n2    = RealVectorX::Zero(n2),
    vec4_n3    = RealVectorX::Zero(n3),
    vec5_n3    = RealVectorX::Zero(n3),
    local_f    = RealVectorX::Zero(n2);
    
    local_f.setZero();
    local_jac.setZero();
    
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX > >
    mat_stiff_A  = _property.stiffness_A_matrix(*this),
    mat_stiff_B  = _property.stiffness_B_matrix(*this),
    mat_stiff_D  = _property.stiffness_D_matrix(*this);
    
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // get the material matrix
        (*mat_stiff_A)(xyz[qp], _time, material_A_mat);
        
        if (if_bending) {
            (*mat_stiff_B)(xyz[qp], _time, material_B_mat);
            (*mat_stiff_D)(xyz[qp], _time, material_D_mat);
        }
        
        // now calculte the quantity for these matrices
        _internal_residual_operation(if_bending, if_vk, n2, qp, *_fe, JxW,
                                     request_jacobian,
                                     local_f, local_jac,
                                     Bmat_mem, Bmat_bend, Bmat_vk,
                                     stress, stress_l, vk_dwdxi_mat, material_A_mat,
                                     material_B_mat, material_D_mat, vec1_n1,
                                     vec2_n1, vec3_n2, vec4_n3,
                                     vec5_n3, mat1_n1n2, mat2_n2n2,
                                     mat3, mat4_n3n2);
        
    }
    
    
    // now calculate the transverse shear contribution if appropriate for the
    // element
    if (if_bending &&
        _bending_operator->include_transverse_shear_energy())
        _bending_operator->calculate_transverse_shear_residual(request_jacobian,
                                                               local_f,
                                                               local_jac);
    
    
    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f += vec3_n2;
    
    if (request_jacobian) {
        // for 2D elements
        if (_elem.dim() == 2) {
            // add small values to the diagonal of the theta_z dofs
            for (unsigned int i=0; i<n_phi; i++)
                local_jac(5*n_phi+i, 5*n_phi+i) = 1.0e-8;
        }
        transform_matrix_to_global_system(local_jac, mat2_n2n2);
        jac += mat2_n2n2;
    }
    
    return request_jacobian;
}





bool
MAST::StructuralElement2D::internal_residual_sensitivity (const MAST::FunctionBase& p,
                                                          bool request_jacobian,
                                                          RealVectorX& f,
                                                          RealMatrixX& jac)
{
    // this should be true if the function is called
    libmesh_assert(!p.is_shape_parameter()); // this is not implemented for now
    
    
    // check if the material property or the provided exterior
    // values, like temperature, are functions of the sensitivity parameter
    bool calculate = false;
    calculate = calculate || _property.depends_on(p);
    
    // nothing to be calculated if the element does not depend on the
    // sensitivity parameter.
    if (!calculate)
        return false;
    
    const std::vector<Real>& JxW = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz = _fe->get_xyz();
    
    const unsigned int
    n_phi    = (unsigned int)_fe->get_phi().size(),
    n1       = this->n_direct_strain_components(),
    n2       =6*n_phi,
    n3       = this->n_von_karman_strain_components();

    RealMatrixX
    material_A_mat,
    material_B_mat,
    material_D_mat,
    material_trans_shear_mat,
    mat1_n1n2     = RealMatrixX::Zero(n1,n2),
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    mat4_n3n2     = RealMatrixX::Zero(n3,n2),
    vk_dwdxi_mat  = RealMatrixX::Zero(n1,n3),
    stress        = RealMatrixX::Zero(2,2),
    stress_l      = RealMatrixX::Zero(2,2),
    local_jac     = RealMatrixX::Zero(n2,n2);
    RealVectorX
    vec1_n1    = RealVectorX::Zero(n1),
    vec2_n1    = RealVectorX::Zero(n1),
    vec3_n2    = RealVectorX::Zero(n2),
    vec4_n3    = RealVectorX::Zero(n3),
    vec5_n3    = RealVectorX::Zero(n3),
    local_f    = RealVectorX::Zero(n2);
    
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX > >
    mat_stiff_A = _property.stiffness_A_matrix(*this),
    mat_stiff_B = _property.stiffness_B_matrix(*this),
    mat_stiff_D = _property.stiffness_D_matrix(*this);
    
    // first calculate the sensitivity due to the parameter
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // get the material matrix
        mat_stiff_A->derivative(p, xyz[qp], _time, material_A_mat);
        
        if (if_bending) {
            
            mat_stiff_B->derivative(p, xyz[qp], _time, material_B_mat);
            mat_stiff_D->derivative(p, xyz[qp], _time, material_D_mat);
        }
        
        // now calculte the quantity for these matrices
        // this accounts for the sensitivity of the material property matrices
        _internal_residual_operation(if_bending, if_vk, n2, qp, *_fe, JxW,
                                     request_jacobian,
                                     local_f, local_jac,
                                     Bmat_mem, Bmat_bend, Bmat_vk,
                                     stress, stress_l, vk_dwdxi_mat, material_A_mat,
                                     material_B_mat, material_D_mat, vec1_n1,
                                     vec2_n1, vec3_n2, vec4_n3,
                                     vec5_n3, mat1_n1n2, mat2_n2n2,
                                     mat3, mat4_n3n2);
        
        // this accounts for the sensitivity of the linear stress as a result of
        // static solution. This is needed only for cases that require linearized
        // geometric stiffness matrix, for example in buckling or natural frequency
        // analysis
        /*if (if_bending && if_vk && if_ignore_ho_jac && request_jacobian) {
            (*mat_stiff_A)(p, _time, material_A_mat);
            (*mat_stiff_B)(p, _time, material_B_mat);
            
            _linearized_geometric_stiffness_sensitivity_with_static_solution
            (n2, qp, *_fe, JxW, local_jac, Bmat_mem, Bmat_bend, Bmat_vk, stress_l,
             vk_dwdxi_mat, material_A_mat, material_B_mat, vec1_n1,
             vec2_n1, mat1_n1n2, mat2_n2n2, mat3);
        }*/
    }
    
    // now calculate the transverse shear contribution if appropriate for the
    // element
    if (if_bending && _bending_operator->include_transverse_shear_energy())
        _bending_operator->calculate_transverse_shear_residual_sensitivity(p,
                                                                           request_jacobian,
                                                                           local_f,
                                                                           local_jac);

    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f += vec3_n2;
    
    if (request_jacobian) {
        transform_matrix_to_global_system(local_jac, mat2_n2n2);
        jac += mat2_n2n2;
    }
    
    
    return request_jacobian;
}



void
MAST::StructuralElement2D::
internal_residual_boundary_velocity(const MAST::FunctionBase& p,
                                    RealVectorX& f,
                                    const unsigned int s,
                                    const MAST::FieldFunction<RealVectorX>& vel_f) {

    // prepare the side finite element
    std::unique_ptr<MAST::FEBase> fe(_assembly.build_fe(_elem).release());
    fe->init_for_side(_elem, s, true);
    
    std::vector<Real> JxW_Vn                        = fe->get_JxW();
    const std::vector<libMesh::Point>& xyz          = fe->get_xyz();
    const std::vector<libMesh::Point>& face_normals = fe->get_normals();

    const unsigned int
    n_phi    = (unsigned int)_fe->get_phi().size(),
    n1       = this->n_direct_strain_components(),
    n2       =6*n_phi,
    n3       = this->n_von_karman_strain_components(),
    dim      = 2;
    
    RealMatrixX
    material_A_mat,
    material_B_mat,
    material_D_mat,
    material_trans_shear_mat,
    mat1_n1n2     = RealMatrixX::Zero(n1,n2),
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    mat4_n3n2     = RealMatrixX::Zero(n3,n2),
    vk_dwdxi_mat  = RealMatrixX::Zero(n1,n3),
    stress        = RealMatrixX::Zero(2,2),
    stress_l      = RealMatrixX::Zero(2,2),
    local_jac     = RealMatrixX::Zero(n2,n2);
    RealVectorX
    vec1_n1    = RealVectorX::Zero(n1),
    vec2_n1    = RealVectorX::Zero(n1),
    vec3_n2    = RealVectorX::Zero(n2),
    vec4_n3    = RealVectorX::Zero(n3),
    vec5_n3    = RealVectorX::Zero(n3),
    local_f    = RealVectorX::Zero(n2),
    vel        = RealVectorX::Zero(dim);
    
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX > >
    mat_stiff_A = _property.stiffness_A_matrix(*this),
    mat_stiff_B = _property.stiffness_B_matrix(*this),
    mat_stiff_D = _property.stiffness_D_matrix(*this);
    
    Real
    vn  = 0.;
    
    // modify the JxW_Vn by multiplying the normal velocity to it
    for (unsigned int qp=0; qp<JxW_Vn.size(); qp++) {
        
        vel_f(xyz[qp], _time, vel);
        vn = 0.;
        for (unsigned int i=0; i<dim; i++)
            vn += vel(i)*face_normals[qp](i);
        JxW_Vn[qp] *= vn;
    }
    
    
    // first calculate the sensitivity due to the parameter
    for (unsigned int qp=0; qp<JxW_Vn.size(); qp++) {

        (*mat_stiff_A)(xyz[qp], _time, material_A_mat);
        
        if (if_bending) {
            
            (*mat_stiff_B)(xyz[qp], _time, material_B_mat);
            (*mat_stiff_D)(xyz[qp], _time, material_D_mat);
        }
        
        // now calculte the quantity for these matrices
        // this accounts for the sensitivity of the material property matrices
        _internal_residual_operation(if_bending, if_vk, n2, qp, *fe, JxW_Vn,
                                     false,
                                     local_f, local_jac,
                                     Bmat_mem, Bmat_bend, Bmat_vk,
                                     stress, stress_l, vk_dwdxi_mat, material_A_mat,
                                     material_B_mat, material_D_mat, vec1_n1,
                                     vec2_n1, vec3_n2, vec4_n3,
                                     vec5_n3, mat1_n1n2, mat2_n2n2,
                                     mat3, mat4_n3n2);
    }
    
    // now calculate the transverse shear contribution if appropriate for the
    // element
    if (if_bending && _bending_operator->include_transverse_shear_energy())
        _bending_operator->calculate_transverse_shear_residual_boundary_velocity
        (p, s, vel_f, local_f);
    
    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f += vec3_n2;
}




bool
MAST::StructuralElement2D::
internal_residual_jac_dot_state_sensitivity (RealMatrixX& jac) {
    
    const std::vector<Real>& JxW            = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz  = _fe->get_xyz();
    const unsigned int
    n_phi = (unsigned int)_fe->get_phi().size(),
    n1    = this->n_direct_strain_components(),
    n2    = 6*n_phi,
    n3    = this->n_von_karman_strain_components();
    
    RealMatrixX
    material_A_mat,
    material_B_mat,
    material_D_mat,
    mat1_n1n2     = RealMatrixX::Zero(n1,n2),
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    vk_dwdxi_mat_sens = RealMatrixX::Zero(n1,n3),
    mat4_n3n2         = RealMatrixX::Zero(n3,n2),
    vk_dwdxi_mat      = RealMatrixX::Zero(n1,n3),
    stress            = RealMatrixX::Zero(2,2),
    local_jac         = RealMatrixX::Zero(n2,n2);
    RealVectorX
    vec1_n1    = RealVectorX::Zero(n1),
    vec2_n1    = RealVectorX::Zero(n1);
    
    
    MAST::FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi);
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    // without the nonlinear strain, this matrix is zero.
    if (!if_vk)
        return false;
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX > >
    mat_stiff_A  = _property.stiffness_A_matrix(*this),
    mat_stiff_B  = _property.stiffness_B_matrix(*this),
    mat_stiff_D  = _property.stiffness_D_matrix(*this);
    
    
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // get the material matrix
        (*mat_stiff_A)(xyz[qp], _time, material_A_mat);
        (*mat_stiff_B)(xyz[qp], _time, material_B_mat);
        (*mat_stiff_D)(xyz[qp], _time, material_D_mat);
        
        // now calculte the quantity for these matrices
        this->initialize_direct_strain_operator(qp, *_fe, Bmat_mem);
        
        // first handle constant throught the thickness stresses: membrane and vonKarman
        Bmat_mem.vector_mult(vec1_n1, _local_sol_sens);
        vec2_n1 = material_A_mat * vec1_n1; // linear direct stress
        
        // get the bending strain operator
        if (if_bending) {
            _bending_operator->initialize_bending_strain_operator(*_fe, qp, Bmat_bend);
            
            //  evaluate the bending stress and add that to the stress vector
            // for evaluation in the nonlinear stress term
            Bmat_bend.vector_mult(vec1_n1, _local_sol_sens);
            vec2_n1 += material_B_mat * vec1_n1;

            if (if_vk) {  // get the vonKarman strain operator if needed
                
                this->initialize_von_karman_strain_operator(qp,
                                                            *_fe,
                                                            vec1_n1, // epsilon_vk
                                                            vk_dwdxi_mat,
                                                            Bmat_vk);
                this->initialize_von_karman_strain_operator_sensitivity(qp,
                                                                        *_fe,
                                                                        vk_dwdxi_mat_sens);
                
                // sensitivity of von Karman strain
                vec1_n1(0)  = (vk_dwdxi_mat(0,0)*vk_dwdxi_mat_sens(0,0));  // dw/dx dwp/dx
                vec1_n1(1)  = (vk_dwdxi_mat(1,1)*vk_dwdxi_mat_sens(1,1));  // dw/dy dwp/dy
                vec1_n1(2)  = (vk_dwdxi_mat(0,0)*vk_dwdxi_mat_sens(1,1) +  // dw/dx dwp/dy
                               vk_dwdxi_mat(1,1)*vk_dwdxi_mat_sens(0,0));  // dw/dy dwp/dx
                
                vec2_n1 += material_A_mat * vec1_n1;
            }
        }
        
        // copy the stress values to a matrix
        stress(0,0)   = vec2_n1(0); // sigma_xx
        stress(1,1)   = vec2_n1(1); // sigma_yy
        stress(0,1)   = vec2_n1(2); // gamma_xy
        stress(1,0)   = vec2_n1(2); // gamma_xy
        
        
        // copy the stress to use here.
        vec2_n1.setZero();
        
        // now calculate the matrix
        // vk - membrane: w-displacement with sens
        Bmat_mem.left_multiply(mat1_n1n2, material_A_mat);
        mat3 = vk_dwdxi_mat_sens.transpose() * mat1_n1n2;
        Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;
        
        // vk - bending: w-displacement with stress sens
        Bmat_bend.left_multiply(mat1_n1n2, material_B_mat);
        mat3 = vk_dwdxi_mat_sens.transpose() * mat1_n1n2;
        Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;

        // vk - vk: with stress sens and stress
        mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
        Bmat_vk.left_multiply(mat3, vk_dwdxi_mat);
        mat3 = vk_dwdxi_mat_sens.transpose() * material_A_mat * mat3;
        Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;

        // vk - vk: with stress and stress sens
        mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
        Bmat_vk.left_multiply(mat3, vk_dwdxi_mat_sens);
        mat3 = vk_dwdxi_mat.transpose() * material_A_mat * mat3;
        Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;
        
        // membrane - vk: w-displacement with sens
        mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
        Bmat_vk.left_multiply(mat3, vk_dwdxi_mat_sens);
        mat3 = material_A_mat * mat3;
        Bmat_mem.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;
        
        // bending - vk: w-displacement with stress sens
        mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
        Bmat_vk.left_multiply(mat3, vk_dwdxi_mat_sens);
        mat3 = material_B_mat.transpose() * mat3;
        Bmat_bend.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;

        // vk - vk: w-displacement with stress sens
        mat3 = RealMatrixX::Zero(2, n2);
        Bmat_vk.left_multiply(mat3, stress);
        Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
        local_jac += JxW[qp] * mat2_n2n2;
    }
    
    transform_matrix_to_global_system(local_jac, mat2_n2n2);
    jac += mat2_n2n2;
    
    return true;
}




void
MAST::StructuralElement2D::_internal_residual_operation
(bool if_bending,
 bool if_vk,
 const unsigned int n2,
 const unsigned int qp,
 const MAST::FEBase& fe,
 const std::vector<Real>& JxW,
 bool request_jacobian,
 RealVectorX& local_f,
 RealMatrixX& local_jac,
 FEMOperatorMatrix& Bmat_mem,
 FEMOperatorMatrix& Bmat_bend,
 FEMOperatorMatrix& Bmat_vk,
 RealMatrixX& stress,
 RealMatrixX& stress_l,
 RealMatrixX& vk_dwdxi_mat,
 RealMatrixX& material_A_mat,
 RealMatrixX& material_B_mat,
 RealMatrixX& material_D_mat,
 RealVectorX& vec1_n1,
 RealVectorX& vec2_n1,
 RealVectorX& vec3_n2,
 RealVectorX& vec4_2,
 RealVectorX& vec5_2,
 RealMatrixX& mat1_n1n2,
 RealMatrixX& mat2_n2n2,
 RealMatrixX& mat3,
 RealMatrixX& mat4_2n2)
{
    this->initialize_direct_strain_operator(qp, fe, Bmat_mem);
    
    // first handle constant throught the thickness stresses: membrane and vonKarman
    Bmat_mem.vector_mult(vec1_n1, _local_sol);
    vec2_n1 = material_A_mat * vec1_n1; // linear direct stress
    
    // copy the stress values to a matrix
    stress_l(0,0) = vec2_n1(0); // sigma_xx
    stress_l(0,1) = vec2_n1(2); // sigma_xy
    stress_l(1,0) = vec2_n1(2); // sigma_yx
    stress_l(1,1) = vec2_n1(1); // sigma_yy
    
    stress = stress_l;
    
    // get the bending strain operator
    vec2_n1.setZero(); // used to store vk strain, if applicable
    if (if_bending) {
        _bending_operator->initialize_bending_strain_operator(fe, qp, Bmat_bend);
        
        Bmat_bend.vector_mult(vec2_n1, _local_sol);
        vec1_n1 = material_B_mat * vec2_n1;
        stress_l(0,0) += vec1_n1(0); // sigma_xx
        stress_l(0,1) += vec1_n1(2); // sigma_xy
        stress_l(1,0) += vec1_n1(2); // sigma_yx
        stress_l(1,1) += vec1_n1(1); // sigma_yy
        
        stress(0,0) += vec1_n1(0); // sigma_xx
        stress(0,1) += vec1_n1(2); // sigma_xy
        stress(1,0) += vec1_n1(2); // sigma_yx
        stress(1,1) += vec1_n1(1); // sigma_yy
        
        // set vec2_n1 to zero, because we need to store only the vk-strain
        // in it for the next operation
        vec2_n1.setZero();
        if (if_vk)  { // get the vonKarman strain operator if needed
            this->initialize_von_karman_strain_operator(qp,
                                                        fe,
                                                        vec2_n1, // epsilon_vk
                                                        vk_dwdxi_mat,
                                                        Bmat_vk);
            
            vec1_n1 = material_A_mat * vec2_n1; // stress
            stress(0,0) += vec1_n1(0); // sigma_xx
            stress(0,1) += vec1_n1(2); // sigma_xy
            stress(1,0) += vec1_n1(2); // sigma_yx
            stress(1,1) += vec1_n1(1); // sigma_yy

        }
    }
    
    // add the linear and nonlinear direct strains
    Bmat_mem.vector_mult(vec1_n1, _local_sol);
    vec2_n1 += vec1_n1;  // epsilon_mem + epsilon_vk

    // copy the total integrated stress to the vector
    vec1_n1(0) = stress(0,0);
    vec1_n1(1) = stress(1,1);
    vec1_n1(2) = stress(0,1);
    
    // now the internal force vector
    // this includes the membrane strain operator with all A and B material operators
    Bmat_mem.vector_mult_transpose(vec3_n2, vec1_n1);
    local_f += JxW[qp] * vec3_n2;
    
    
    if (if_bending) {
        if (if_vk) {
            // von Karman strain
            vec4_2 = vk_dwdxi_mat.transpose() * vec1_n1;
            Bmat_vk.vector_mult_transpose(vec3_n2, vec4_2);
            local_f += JxW[qp] * vec3_n2;
        }
        
        // now coupling with the bending strain
        // B_bend^T [B] B_mem
        vec1_n1 = material_B_mat.transpose() * vec2_n1;
        Bmat_bend.vector_mult_transpose(vec3_n2, vec1_n1);
        local_f += JxW[qp] * vec3_n2;
        
        // now bending stress
        Bmat_bend.vector_mult(vec2_n1, _local_sol);
        vec1_n1 = material_D_mat * vec2_n1;
        Bmat_bend.vector_mult_transpose(vec3_n2, vec1_n1);
        local_f += JxW[qp] * vec3_n2;
    }
    
    if (request_jacobian) {
        // membrane - membrane
        Bmat_mem.left_multiply(mat1_n1n2, material_A_mat);
        Bmat_mem.right_multiply_transpose(mat2_n2n2, mat1_n1n2);
        local_jac += JxW[qp] * mat2_n2n2;
        
        if (if_bending) {
            if (if_vk) {
                // membrane - vk
                mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
                Bmat_vk.left_multiply(mat3, vk_dwdxi_mat);
                mat3 = material_A_mat * mat3;
                Bmat_mem.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
                
                // vk - membrane
                Bmat_mem.left_multiply(mat1_n1n2, material_A_mat);
                mat3 = vk_dwdxi_mat.transpose() * mat1_n1n2;
                Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
                
                // if only the first order term of the Jacobian is needed, for
                // example for linearized buckling analysis, then the linear
                // stress combined with the variation of the von Karman strain
                // is included. Otherwise, all terms are included
                /*if (if_ignore_ho_jac) {
                    // vk - vk: first order term
                    mat3 = RealMatrixX::Zero(2, n2);
                    Bmat_vk.left_multiply(mat3, stress_l);
                    Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                    local_jac += JxW[qp] * mat2_n2n2;
                }
                else*/ {
                    // vk - vk
                    mat3 = RealMatrixX::Zero(2, n2);
                    Bmat_vk.left_multiply(mat3, stress);
                    Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                    local_jac += JxW[qp] * mat2_n2n2;
                    
                    mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
                    Bmat_vk.left_multiply(mat3, vk_dwdxi_mat);
                    mat3 = vk_dwdxi_mat.transpose() * material_A_mat * mat3;
                    Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                    local_jac += JxW[qp] * mat2_n2n2;
                }
                
                // bending - vk
                mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
                Bmat_vk.left_multiply(mat3, vk_dwdxi_mat);
                mat3 = material_B_mat.transpose() * mat3;
                Bmat_bend.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
                
                // vk - bending
                Bmat_bend.left_multiply(mat1_n1n2, material_B_mat);
                mat3 = vk_dwdxi_mat.transpose() * mat1_n1n2;
                Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
            }
            
            // bending - membrane
            mat3 = material_B_mat.transpose();
            Bmat_mem.left_multiply(mat1_n1n2, mat3);
            Bmat_bend.right_multiply_transpose(mat2_n2n2, mat1_n1n2);
            local_jac += JxW[qp] * mat2_n2n2;
            
            // membrane - bending
            Bmat_bend.left_multiply(mat1_n1n2, material_B_mat);
            Bmat_mem.right_multiply_transpose(mat2_n2n2, mat1_n1n2);
            local_jac += JxW[qp] * mat2_n2n2;
            
            // bending - bending
            Bmat_bend.left_multiply(mat1_n1n2, material_D_mat);
            Bmat_bend.right_multiply_transpose(mat2_n2n2, mat1_n1n2);
            local_jac += JxW[qp] * mat2_n2n2;
        }
    }
}




void
MAST::StructuralElement2D::
_linearized_geometric_stiffness_sensitivity_with_static_solution
(const unsigned int n2,
 const unsigned int qp,
 const MAST::FEBase& fe,
 const std::vector<Real>& JxW,
 RealMatrixX& local_jac,
 FEMOperatorMatrix& Bmat_mem,
 FEMOperatorMatrix& Bmat_bend,
 FEMOperatorMatrix& Bmat_vk,
 RealMatrixX& stress_l,
 RealMatrixX& vk_dwdxi_mat,
 RealMatrixX& material_A_mat,
 RealMatrixX& material_B_mat,
 RealVectorX& vec1_n1,
 RealVectorX& vec2_n1,
 RealMatrixX& mat1_n1n2,
 RealMatrixX& mat2_n2n2,
 RealMatrixX& mat3)
{
    this->initialize_direct_strain_operator(qp, fe, Bmat_mem);
    _bending_operator->initialize_bending_strain_operator(fe, qp, Bmat_bend);
    
    // first handle constant throught the thickness stresses: membrane and vonKarman
    Bmat_mem.vector_mult(vec1_n1, _local_sol_sens);
    vec2_n1 = material_A_mat * vec1_n1; // linear direct stress
    
    // copy the stress values to a matrix
    stress_l(0,0) = vec2_n1(0); // sigma_xx
    stress_l(0,1) = vec2_n1(2); // sigma_xy
    stress_l(1,0) = vec2_n1(2); // sigma_yx
    stress_l(1,1) = vec2_n1(1); // sigma_yy
    
    // get the von Karman operator matrix
    this->initialize_von_karman_strain_operator(qp,
                                                fe,
                                                vec2_n1, // epsilon_vk
                                                vk_dwdxi_mat,
                                                Bmat_vk);
    
    // sensitivity of the vk_dwdxi matrix due to solution sensitivity
    this->initialize_von_karman_strain_operator_sensitivity(qp,
                                                            fe,
                                                            vk_dwdxi_mat);
    
    // membrane - vk
    mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
    Bmat_vk.left_multiply(mat3, vk_dwdxi_mat);
    mat3 = material_A_mat * mat3;
    Bmat_mem.right_multiply_transpose(mat2_n2n2, mat3);
    local_jac += JxW[qp] * mat2_n2n2;
    
    // vk - membrane
    Bmat_mem.left_multiply(mat1_n1n2, material_A_mat);
    mat3 = vk_dwdxi_mat.transpose() * mat1_n1n2;
    Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
    local_jac += JxW[qp] * mat2_n2n2;
    
    // vk - vk: first order term
    mat3 = RealMatrixX::Zero(2, n2);
    Bmat_vk.left_multiply(mat3, stress_l);
    Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
    local_jac += JxW[qp] * mat2_n2n2;
    
    // bending - vk
    mat3 = RealMatrixX::Zero(vk_dwdxi_mat.rows(), n2);
    Bmat_vk.left_multiply(mat3, vk_dwdxi_mat);
    mat3 = material_B_mat.transpose() * mat3;
    Bmat_bend.right_multiply_transpose(mat2_n2n2, mat3);
    local_jac += JxW[qp] * mat2_n2n2;
    
    // vk - bending
    Bmat_bend.left_multiply(mat1_n1n2, material_B_mat);
    mat3 =  vk_dwdxi_mat.transpose() * mat1_n1n2;
    Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
    local_jac += JxW[qp] * mat2_n2n2;
}



void
MAST::StructuralElement2D::_convert_prestress_A_mat_to_vector(const RealMatrixX& mat,
                                                              RealVectorX& vec) const {
    
    libmesh_assert_equal_to(mat.rows(), 2);
    libmesh_assert_equal_to(mat.cols(), 2);
    vec = RealVectorX::Zero(3);
    vec(0) = mat(0,0);  // sigma x
    vec(1) = mat(1,1);  // sigma y
    vec(2) = mat(0,1);  // tau xy
}



void
MAST::StructuralElement2D::_convert_prestress_B_mat_to_vector(const RealMatrixX& mat,
                                                              RealVectorX& vec) const {
    
    libmesh_assert_equal_to(mat.rows(), 2);
    libmesh_assert_equal_to(mat.cols(), 2);
    vec = RealVectorX::Zero(3);
    vec(0) = mat(0,0);  // sigma x
    vec(1) = mat(1,1);  // sigma y
    vec(2) = mat(0,1);  // tau xy
}




bool
MAST::StructuralElement2D::prestress_residual (bool request_jacobian,
                                               RealVectorX& f,
                                               RealMatrixX& jac)
{
    if (!_property.if_prestressed())
        return false;
    
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    const std::vector<Real>& JxW               = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz     = _fe->get_xyz();
    const unsigned int
    n_phi  = (unsigned int)_fe->get_phi().size(),
    n1     = this->n_direct_strain_components(),
    n2     = 6*n_phi,
    n3     = this->n_von_karman_strain_components();
    
    RealMatrixX
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    vk_dwdxi_mat  = RealMatrixX::Zero(n1,n3),
    local_jac     = RealMatrixX::Zero(n2,n2),
    prestress_mat_A,
    prestress_mat_B;
    RealVectorX
    vec2_n1         = RealVectorX::Zero(n1),
    vec3_n2         = RealVectorX::Zero(n2),
    vec4_n3         = RealVectorX::Zero(n3),
    vec5_n3         = RealVectorX::Zero(n3),
    local_f         = RealVectorX::Zero(n2),
    prestress_vec_A,
    prestress_vec_B;
    
    local_f.setZero();
    local_jac.setZero();

    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX> >
    prestress_A = _property.prestress_A_matrix(*this),
    prestress_B = _property.prestress_B_matrix(*this);
    
    // now calculate the quantity
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        (*prestress_A)(xyz[qp], _time, prestress_mat_A);
        (*prestress_B)(xyz[qp], _time, prestress_mat_B);
        _convert_prestress_A_mat_to_vector(prestress_mat_A, prestress_vec_A);
        _convert_prestress_B_mat_to_vector(prestress_mat_B, prestress_vec_B);
        
        this->initialize_direct_strain_operator(qp, *_fe, Bmat_mem);
        
        // get the bending strain operator if needed
        vec2_n1.setZero(); // used to store vk strain, if applicable
        if (if_bending) {
            _bending_operator->initialize_bending_strain_operator(*_fe, qp, Bmat_bend);
            
            if (if_vk)  // get the vonKarman strain operator if needed
                this->initialize_von_karman_strain_operator(qp,
                                                            *_fe,
                                                            vec2_n1,
                                                            vk_dwdxi_mat,
                                                            Bmat_vk);
        }
        
        // first handle constant throught the thickness stresses: membrane and vonKarman
        // multiply this with the constant through the thickness strain
        // membrane strain
        Bmat_mem.vector_mult_transpose(vec3_n2, prestress_vec_A);
        local_f += JxW[qp] * vec3_n2; // epsilon_mem * sigma_0
        
        if (if_bending) {
            if (if_vk) {
                // von Karman strain
                vec4_n3 = vk_dwdxi_mat.transpose() * prestress_vec_A;
                Bmat_vk.vector_mult_transpose(vec3_n2, vec4_n3);
                local_f += JxW[qp] * vec3_n2; // epsilon_vk * sigma_0
            }
            
            // now coupling with the bending strain
            Bmat_bend.vector_mult_transpose(vec3_n2, prestress_vec_B);
            local_f += JxW[qp] * vec3_n2; // epsilon_bend * sigma_0
        }
        
        if (request_jacobian) {
            if (if_bending && if_vk) {
                mat3 = RealMatrixX::Zero(2, n2);
                Bmat_vk.left_multiply(mat3, prestress_mat_A);
                Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
            }
        }
    }
    
    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f += vec3_n2;
    if (request_jacobian && if_vk) {
        transform_matrix_to_global_system(local_jac, mat2_n2n2);
        jac += mat2_n2n2;
    }
    
    // only the nonlinear strain returns a Jacobian for prestressing
    return (request_jacobian);
}





bool
MAST::StructuralElement2D::prestress_residual_sensitivity (const MAST::FunctionBase& p,
                                                           bool request_jacobian,
                                                           RealVectorX& f,
                                                           RealMatrixX& jac)
{
    if (!_property.if_prestressed())
        return false;
    
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    const std::vector<Real>& JxW            = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz  = _fe->get_xyz();
    const unsigned int
    n_phi = (unsigned int)_fe->get_phi().size(),
    n1    = this->n_direct_strain_components(),
    n2    = 6*n_phi,
    n3    = this->n_von_karman_strain_components();
    
    RealMatrixX
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    vk_dwdxi_mat  = RealMatrixX::Zero(n1,n3),
    local_jac     = RealMatrixX::Zero(n2,n2),
    prestress_mat_A,
    prestress_mat_B;
    RealVectorX
    vec2_n1     = RealVectorX::Zero(n1),
    vec3_n2     = RealVectorX::Zero(n2),
    vec4_n3     = RealVectorX::Zero(n3),
    vec5_n3     = RealVectorX::Zero(n3),
    local_f     = RealVectorX::Zero(n2),
    prestress_vec_A,
    prestress_vec_B;
    
    local_f.setZero();
    local_jac.setZero();

    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX> >
    prestress_A = _property.prestress_A_matrix(*this),
    prestress_B = _property.prestress_B_matrix(*this);
    
    // transform to the local coordinate system
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        prestress_A->derivative(p, xyz[qp], _time, prestress_mat_A);
        prestress_B->derivative(p, xyz[qp], _time, prestress_mat_B);
        _convert_prestress_A_mat_to_vector(prestress_mat_A, prestress_vec_A);
        _convert_prestress_B_mat_to_vector(prestress_mat_B, prestress_vec_B);
        
        this->initialize_direct_strain_operator(qp, *_fe, Bmat_mem);
        
        // get the bending strain operator if needed
        vec2_n1.setZero(); // used to store vk strain, if applicable
        if (if_bending) {
            _bending_operator->initialize_bending_strain_operator(*_fe, qp, Bmat_bend);
            
            if (if_vk)  // get the vonKarman strain operator if needed
                this->initialize_von_karman_strain_operator(qp,
                                                            *_fe,
                                                            vec2_n1,
                                                            vk_dwdxi_mat,
                                                            Bmat_vk);
        }
        
        // first handle constant throught the thickness stresses: membrane and vonKarman
        // multiply this with the constant through the thickness strain
        // membrane strain
        Bmat_mem.vector_mult_transpose(vec3_n2, prestress_vec_A);
        local_f += JxW[qp] * vec3_n2; // epsilon_mem * sigma_0
        
        if (if_bending) {
            if (if_vk) {
                // von Karman strain
                vec4_n3 = vk_dwdxi_mat.transpose() * prestress_vec_A;
                Bmat_vk.vector_mult_transpose(vec3_n2, vec4_n3);
                local_f += JxW[qp] * vec3_n2; // epsilon_vk * sigma_0
            }
            
            // now coupling with the bending strain
            Bmat_bend.vector_mult_transpose(vec3_n2, prestress_vec_B);
            local_f += JxW[qp] * vec3_n2; // epsilon_bend * sigma_0
        }
        
        if (request_jacobian) {
            if (if_bending && if_vk) {
                mat3 = RealMatrixX::Zero(2, n2);
                Bmat_vk.left_multiply(mat3, prestress_mat_A);
                Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
            }
        }
    }
    
    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f += vec3_n2;
    if (request_jacobian && if_vk) {
        transform_matrix_to_global_system(local_jac, mat2_n2n2);
        jac += mat2_n2n2;
    }
    
    // only the nonlinear strain returns a Jacobian for prestressing
    return (request_jacobian);
}





bool
MAST::StructuralElement2D::
surface_pressure_residual(bool request_jacobian,
                          RealVectorX &f,
                          RealMatrixX &jac,
                          const unsigned int side,
                          MAST::BoundaryConditionBase& bc) {
    
    libmesh_assert(!follower_forces); // not implemented yet for follower forces
    
    // prepare the side finite element
    std::unique_ptr<MAST::FEBase> fe(_assembly.build_fe(_elem).release());
    fe->init_for_side(_elem, side, false);
    
    const std::vector<Real> &JxW                    = fe->get_JxW();
    const std::vector<libMesh::Point>& qpoint       = fe->get_xyz();
    const std::vector<std::vector<Real> >& phi      = fe->get_phi();
    const std::vector<libMesh::Point>& face_normals = fe->get_normals();
    const unsigned int
    n_phi  = (unsigned int)phi.size(),
    n1     = 3,
    n2     = 6*n_phi;
    
    
    // get the function from this boundary condition
    const MAST::FieldFunction<Real>& p_func =
    bc.get<MAST::FieldFunction<Real> >("pressure");
    
    // get the thickness function to calculate the force
    const MAST::FieldFunction<Real>& t_func =
    _property.get<MAST::FieldFunction<Real> >("h");
    
    FEMOperatorMatrix Bmat;
    Real
    press   = 0.,
    t_val   = 0.;
    
    RealVectorX
    phi_vec     = RealVectorX::Zero(n_phi),
    force       = RealVectorX::Zero(2*n1),
    local_f     = RealVectorX::Zero(n2),
    vec_n2      = RealVectorX::Zero(n2);
    
    for (unsigned int qp=0; qp<qpoint.size(); qp++) {
        
        // now set the shape function values
        for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
            phi_vec(i_nd) = phi[i_nd][qp];
        
        Bmat.reinit(2*n1, phi_vec);
        
        // get pressure and thickness values
        p_func(qpoint[qp], _time, press);
        t_func(qpoint[qp], _time, t_val);
        
        // calculate force
        for (unsigned int i_dim=0; i_dim<n1; i_dim++)
            force(i_dim) = (press*t_val) * face_normals[qp](i_dim);
        
        Bmat.vector_mult_transpose(vec_n2, force);
        
        local_f += JxW[qp] * vec_n2;
    }
    
    // now transform to the global system and add
    transform_vector_to_global_system(local_f, vec_n2);
    f -= vec_n2;
    
    
    return (request_jacobian);
}





bool
MAST::StructuralElement2D::
surface_pressure_residual_sensitivity(const MAST::FunctionBase& p,
                                      bool request_jacobian,
                                      RealVectorX &f,
                                      RealMatrixX &jac,
                                      const unsigned int side,
                                      MAST::BoundaryConditionBase& bc) {
    
    libmesh_assert(!follower_forces); // not implemented yet for follower forces
    
    // prepare the side finite element
    std::unique_ptr<MAST::FEBase> fe(_assembly.build_fe(_elem).release());
    fe->init_for_side(_elem, side, false);

    const std::vector<Real> &JxW                    = fe->get_JxW();
    const std::vector<libMesh::Point>& qpoint       = fe->get_xyz();
    const std::vector<std::vector<Real> >& phi      = fe->get_phi();
    const std::vector<libMesh::Point>& face_normals = fe->get_normals();
    const unsigned int
    n_phi  = (unsigned int)phi.size(),
    n1     = 3,
    n2     = 6*n_phi;
    
    
    // get the function from this boundary condition
    const MAST::FieldFunction<Real>& p_func =
    bc.get<MAST::FieldFunction<Real> >("pressure");

    // get the thickness function to calculate the force
    const MAST::FieldFunction<Real>& t_func =
    _property.get<MAST::FieldFunction<Real> >("h");

    
    FEMOperatorMatrix Bmat;
    Real
    press   =  0.,
    dpress  =  0.,
    t_val   =  0.,
    dt_val  =  0.;
    
    RealVectorX
    phi_vec     = RealVectorX::Zero(n_phi),
    force       = RealVectorX::Zero(2*n1),
    local_f     = RealVectorX::Zero(n2),
    vec_n2      = RealVectorX::Zero(n2);
    
    for (unsigned int qp=0; qp<qpoint.size(); qp++) {
        
        // now set the shape function values
        for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
            phi_vec(i_nd) = phi[i_nd][qp];
        
        Bmat.reinit(2*n1, phi_vec);
        
        // get pressure and thickness values and their sensitivities
        p_func(qpoint[qp], _time, press);
        p_func.derivative(p, qpoint[qp], _time, dpress);
        t_func(qpoint[qp], _time, t_val);
        t_func.derivative(p, qpoint[qp], _time, dt_val);
        
        // calculate force
        for (unsigned int i_dim=0; i_dim<n1; i_dim++)
            force(i_dim) = (press*dt_val + dpress*t_val) * face_normals[qp](i_dim);
        
        Bmat.vector_mult_transpose(vec_n2, force);
        
        local_f += JxW[qp] * vec_n2;
    }
    
    // now transform to the global system and add
    transform_vector_to_global_system(local_f, vec_n2);
    f -= vec_n2;
    
    
    return (request_jacobian);
}





bool
MAST::StructuralElement2D::thermal_residual (bool request_jacobian,
                                             RealVectorX& f,
                                             RealMatrixX& jac,
                                             MAST::BoundaryConditionBase& bc)
{
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    const std::vector<Real>& JxW = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz = _fe->get_xyz();
    const unsigned int n_phi = (unsigned int)_fe->get_phi().size();
    const unsigned int n1= this->n_direct_strain_components(), n2=6*n_phi,
    n3 = this->n_von_karman_strain_components();
    RealMatrixX
    material_exp_A_mat,
    material_exp_B_mat,
    mat1_n1n2     = RealMatrixX::Zero(n1,n2),
    mat2_n2n2     = RealMatrixX::Zero(n2,n2),
    mat3,
    mat4_n3n2     = RealMatrixX::Zero(n3,n2),
    vk_dwdxi_mat  = RealMatrixX::Zero(n1,n3),
    stress        = RealMatrixX::Zero(2,2),
    local_jac     = RealMatrixX::Zero(n2,n2);
    RealVectorX
    vec1_n1     = RealVectorX::Zero(n1),
    vec2_n1     = RealVectorX::Zero(n1),
    vec3_n2     = RealVectorX::Zero(n2),
    vec4_2      = RealVectorX::Zero(2),
    vec5_n3     = RealVectorX::Zero(n3),
    local_f     = RealVectorX::Zero(n2),
    delta_t     = RealVectorX::Zero(1);
    
    local_f.setZero();
    local_jac.setZero();

    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX > >
    expansion_A = _property.thermal_expansion_A_matrix(*this),
    expansion_B = _property.thermal_expansion_B_matrix(*this);
    
    const MAST::FieldFunction<Real>
    &temp_func     = bc.get<MAST::FieldFunction<Real> >("temperature"),
    &ref_temp_func = bc.get<MAST::FieldFunction<Real> >("ref_temperature");
    
    Real t, t0;
    
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // this is moved inside the domain since
        (*expansion_A)(xyz[qp], _time, material_exp_A_mat);
        (*expansion_B)(xyz[qp], _time, material_exp_B_mat);
        
        // get the temperature function
        temp_func(xyz[qp], _time, t);
        ref_temp_func(xyz[qp], _time, t0);
        delta_t(0) = t-t0;
        
        vec1_n1 = material_exp_A_mat * delta_t; // [C]{alpha (T - T0)} (with membrane strain)
        vec2_n1 = material_exp_B_mat * delta_t; // [C]{alpha (T - T0)} (with bending strain)
        stress(0,0) = vec1_n1(0); // sigma_xx
        stress(0,1) = vec1_n1(2); // sigma_xy
        stress(1,0) = vec1_n1(2); // sigma_yx
        stress(1,1) = vec1_n1(1); // sigma_yy
        
        this->initialize_direct_strain_operator(qp, *_fe, Bmat_mem);
        
        // membrane strain
        Bmat_mem.vector_mult_transpose(vec3_n2, vec1_n1);
        local_f += JxW[qp] * vec3_n2;
        
        if (if_bending) {
            // bending strain
            _bending_operator->initialize_bending_strain_operator(*_fe, qp, Bmat_bend);
            Bmat_bend.vector_mult_transpose(vec3_n2, vec2_n1);
            local_f += JxW[qp] * vec3_n2;
            
            // von Karman strain
            if (if_vk) {
                // get the vonKarman strain operator if needed
                this->initialize_von_karman_strain_operator(qp,
                                                            *_fe,
                                                            vec2_n1, // epsilon_vk
                                                            vk_dwdxi_mat,
                                                            Bmat_vk);
                // von Karman strain
                vec4_2 = vk_dwdxi_mat.transpose() * vec1_n1;
                Bmat_vk.vector_mult_transpose(vec3_n2, vec4_2);
                local_f += JxW[qp] * vec3_n2;
            }
            
            if (request_jacobian && if_vk) { // Jacobian only for vk strain
                                             // vk - vk
                mat3 = RealMatrixX::Zero(2, n2);
                Bmat_vk.left_multiply(mat3, stress);
                Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
            }
        }
    }
    
    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f -= vec3_n2;
    if (request_jacobian && if_vk) {
        transform_matrix_to_global_system(local_jac, mat2_n2n2);
        jac -= mat2_n2n2;
    }
    
    // Jacobian contribution from von Karman strain
    return request_jacobian;
}




bool
MAST::StructuralElement2D::
thermal_residual_sensitivity (const MAST::FunctionBase& p,
                              bool request_jacobian,
                              RealVectorX& f,
                              RealMatrixX& jac,
                              MAST::BoundaryConditionBase& bc)
{
    FEMOperatorMatrix Bmat_mem, Bmat_bend, Bmat_vk;
    
    const std::vector<Real>& JxW = _fe->get_JxW();
    const std::vector<libMesh::Point>& xyz = _fe->get_xyz();
    const unsigned int n_phi = (unsigned int)_fe->get_phi().size();
    const unsigned int n1= this->n_direct_strain_components(), n2=6*n_phi,
    n3 = this->n_von_karman_strain_components();
    RealMatrixX
    material_exp_A_mat,
    material_exp_B_mat,
    material_exp_A_mat_sens,
    material_exp_B_mat_sens,
    mat1_n1n2       = RealMatrixX::Zero(n1,n2),
    mat2_n2n2       = RealMatrixX::Zero(n2,n2),
    mat3,
    mat4_n3n2       = RealMatrixX::Zero(n3,n2),
    vk_dwdxi_mat    = RealMatrixX::Zero(n1,n3),
    stress          = RealMatrixX::Zero(2,2),
    local_jac       = RealMatrixX::Zero(n2,n2);
    RealVectorX
    vec1_n1      = RealVectorX::Zero(n1),
    vec2_n1      = RealVectorX::Zero(n1),
    vec3_n2      = RealVectorX::Zero(n2),
    vec4_2       = RealVectorX::Zero(2),
    vec5_n1      = RealVectorX::Zero(n1),
    local_f      = RealVectorX::Zero(n2),
    delta_t      = RealVectorX::Zero(1),
    delta_t_sens = RealVectorX::Zero(1);
    
    Bmat_mem.reinit(n1, _system.n_vars(), n_phi); // three stress-strain components
    Bmat_bend.reinit(n1, _system.n_vars(), n_phi);
    Bmat_vk.reinit(n3, _system.n_vars(), n_phi); // only dw/dx and dw/dy
    
    bool if_vk = (_property.strain_type() == MAST::VON_KARMAN_STRAIN),
    if_bending = (_property.bending_model(_elem, _fe->get_fe_type()) != MAST::NO_BENDING);
    
    std::unique_ptr<MAST::FieldFunction<RealMatrixX > >
    expansion_A = _property.thermal_expansion_A_matrix(*this),
    expansion_B = _property.thermal_expansion_B_matrix(*this);
    
    // temperature function
    const MAST::FieldFunction<Real>
    &temp_func     = bc.get<MAST::FieldFunction<Real> >("temperature"),
    &ref_temp_func = bc.get<MAST::FieldFunction<Real> >("ref_temperature");
    
    Real t, t0, t_sens;
    
    for (unsigned int qp=0; qp<JxW.size(); qp++) {
        
        // this is moved inside the domain since
        (*expansion_A)(xyz[qp], _time, material_exp_A_mat);
        (*expansion_B)(xyz[qp], _time, material_exp_B_mat);
        expansion_A->derivative(p, xyz[qp], _time, material_exp_A_mat_sens);
        expansion_B->derivative(p, xyz[qp], _time, material_exp_B_mat_sens);
        
        // get the temperature function
        temp_func(xyz[qp], _time, t);
        ref_temp_func(xyz[qp], _time, t0);
        delta_t(0) = t-t0;
        
        // get the temperature function
        temp_func(xyz[qp], _time, t);
        temp_func.derivative(p, xyz[qp], _time, t_sens);
        ref_temp_func(xyz[qp], _time, t0);
        delta_t(0) = t-t0;
        delta_t_sens(0) = t_sens;
        
        // now prepare the membrane force sensitivity
        vec1_n1 = material_exp_A_mat * delta_t_sens; // [C]{alpha dT/dp} (with membrane strain)
        vec2_n1 = material_exp_A_mat_sens * delta_t; // d([C]alpha)/dp (T - T0)} (with membrane strain)
        vec1_n1 += vec2_n1;
        stress(0,0) = vec1_n1(0); // sigma_xx
        stress(0,1) = vec1_n1(2); // sigma_xy
        stress(1,0) = vec1_n1(2); // sigma_yx
        stress(1,1) = vec1_n1(1); // sigma_yy
        
        vec2_n1 = material_exp_B_mat * delta_t_sens; // [C]{alpha dT/dp} (with bending strain)
        vec5_n1 = material_exp_B_mat_sens * delta_t; // d([C] alpha)/dp (T - T0) (with bending strain)
        vec2_n1 += vec5_n1;
        
        
        this->initialize_direct_strain_operator(qp, *_fe, Bmat_mem);
        
        // membrane strain
        Bmat_mem.vector_mult_transpose(vec3_n2, vec1_n1);
        local_f += JxW[qp] * vec3_n2;
        
        if (if_bending) {
            // bending strain
            _bending_operator->initialize_bending_strain_operator(*_fe, qp, Bmat_bend);
            Bmat_bend.vector_mult_transpose(vec3_n2, vec2_n1);
            local_f += JxW[qp] * vec3_n2;
            
            // von Karman strain
            if (if_vk) {
                // get the vonKarman strain operator if needed
                this->initialize_von_karman_strain_operator(qp,
                                                            *_fe,
                                                            vec2_n1, // epsilon_vk
                                                            vk_dwdxi_mat,
                                                            Bmat_vk);
                // von Karman strain
                vec4_2 = vk_dwdxi_mat.transpose() * vec1_n1;
                Bmat_vk.vector_mult_transpose(vec3_n2, vec4_2);
                local_f += JxW[qp] * vec3_n2;
            }
            
            if (request_jacobian && if_vk) { // Jacobian only for vk strain
                
                // vk - vk
                mat3 = RealMatrixX::Zero(2, n2);
                Bmat_vk.left_multiply(mat3, stress);
                Bmat_vk.right_multiply_transpose(mat2_n2n2, mat3);
                local_jac += JxW[qp] * mat2_n2n2;
            }
        }
    }
    
    
    // now transform to the global coorodinate system
    transform_vector_to_global_system(local_f, vec3_n2);
    f -= vec3_n2;
    if (request_jacobian && if_vk) {
        transform_matrix_to_global_system(local_jac, mat2_n2n2);
        jac -= mat2_n2n2;
    }
    
    // Jacobian contribution from von Karman strain
    return request_jacobian;
}





bool
MAST::StructuralElement2D::
piston_theory_residual(bool request_jacobian,
                       RealVectorX &f,
                       RealMatrixX& jac_xdot,
                       RealMatrixX& jac,
                       const unsigned int side,
                       MAST::BoundaryConditionBase& bc) {
    
    libmesh_assert(false); // to be implemented
    
    return (request_jacobian);
}




bool
MAST::StructuralElement2D::
piston_theory_residual_sensitivity(const MAST::FunctionBase& p,
                                   bool request_jacobian,
                                   RealVectorX &f,
                                   RealMatrixX& jac_xdot,
                                   RealMatrixX& jac,
                                   const unsigned int side,
                                   MAST::BoundaryConditionBase& bc) {
    
    libmesh_assert(false); // to be implemented
    
    return (request_jacobian);
}





bool
MAST::StructuralElement2D::
piston_theory_residual(bool request_jacobian,
                       RealVectorX &f,
                       RealMatrixX& jac_xdot,
                       RealMatrixX& jac,
                       MAST::BoundaryConditionBase& bc) {
    
    libmesh_assert(_elem.dim() < 3); // only applicable for lower dimensional elements
    libmesh_assert(!follower_forces); // not implemented yet for follower forces
    
    
    const std::vector<Real> &JxW                = _fe->get_JxW();
    const std::vector<libMesh::Point>& qpoint   = _fe->get_xyz();
    const std::vector<std::vector<Real> >& phi  = _fe->get_phi();
    const unsigned int
    n_phi = (unsigned int)phi.size(),
    n1    = 2,
    n2    = _system.n_vars()*n_phi;
    
    
    // normal for face integration
    libMesh::Point normal;
    // direction of pressure assumed to be normal (along local z-axis)
    // to the element face for 2D and along local y-axis for 1D element.
    normal(_elem.dim()) = -1.;
    
    
    // convert to piston theory boundary condition so that the necessary
    // flow properties can be obtained
    const MAST::PistonTheoryBoundaryCondition& piston_bc =
    dynamic_cast<MAST::PistonTheoryBoundaryCondition&>(bc);

    
    // create the constant field functions to pass the dwdx and dwdt values
    // to the piston theory pressure functions
    MAST::Parameter
    dwdx_p  ("dwdx", 0.),
    dwdt_p  ("dwdt", 0.);
    
    MAST::ConstantFieldFunction
    dwdx_f  ("dwdx", dwdx_p),
    dwdt_f  ("dwdx", dwdt_p);
    
    
    std::unique_ptr<MAST::FieldFunction<Real> >
    pressure        (piston_bc.get_pressure_function(dwdx_f, dwdt_f).release()),
    dpressure_dx    (piston_bc.get_dpdx_function    (dwdx_f, dwdt_f).release()),
    dpressure_dxdot (piston_bc.get_dpdxdot_function (dwdx_f, dwdt_f).release());
    
    FEMOperatorMatrix
    Bmat_w,         // operator matrix for the w-displacement
    dBmat;          // operator matrix to calculate the derivativ of w wrt x and y
    
    dBmat.reinit(n1, _system.n_vars(), n_phi);
    
    RealVectorX
    phi_vec  = RealVectorX::Zero(n_phi),
    force    = RealVectorX::Zero(n1),
    local_f  = RealVectorX::Zero(n2),
    vec_n1   = RealVectorX::Zero(n1),
    vec_n2   = RealVectorX::Zero(n2),
    vel_vec  = RealVectorX::Zero(3),
    dummy    = RealVectorX::Zero(3);

    RealMatrixX
    dwdx            = RealMatrixX::Zero(3,2),
    local_jac_xdot  = RealMatrixX::Zero(n2,n2),
    local_jac       = RealMatrixX::Zero(n2,n2),
    mat_n2n2        = RealMatrixX::Zero(n2,n2),
    mat_n1n2        = RealMatrixX::Zero(n1,n2),
    mat_22          = RealMatrixX::Zero(2,2);
    
    // we need the velocity vector in the local coordinate system so that
    // the appropriate component of the w-derivative can be used
    vel_vec = _Tmatrix().transpose() * piston_bc.vel_vec();
    
    Real
    dwdt_val = 0.,
    dwdx_val = 0.,
    p_val    = 0.;
    
    
    for (unsigned int qp=0; qp<qpoint.size(); qp++)
    {

        // now set the shape function values
        for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
            phi_vec(i_nd) = phi[i_nd][qp];
        
        // initialize the B matrix for only the w-displacement
        Bmat_w.reinit(n1, _system.n_vars(), n_phi);
        Bmat_w.set_shape_function(0, 2, phi_vec);  // interpolates w-displacement
        
        // use the Bmat to calculate the velocity vector. Only the w-displacement
        // is of interest in the local coordinate, since that is the only
        // component normal to the surface.
        Bmat_w.right_multiply(vec_n1, _local_vel);
        dwdt_val = vec_n1(0);
        
        // get the operators for dw/dx and dw/dy to calculate the
        // normal velocity. We will use the von Karman strain operators
        // for this
        this->initialize_von_karman_strain_operator(qp,
                                                    *_fe,
                                                    dummy,
                                                    dwdx,
                                                    dBmat);

        // the diagonal of dwdx matrix stores the
        dwdx_val = 0.;
        for (unsigned int i=0; i<2; i++)
            dwdx_val  +=  dwdx(i,i) * vel_vec(i);   // (dw/dx_i)*U_inf . n_i
        
        // calculate the pressure value
        dwdx_p = dwdx_val;
        dwdt_p = dwdt_val;
        (*pressure)(qpoint[qp], _time, p_val);
        
        // calculate force
        force(0) = p_val * normal(2);
        
        
        Bmat_w.vector_mult_transpose(vec_n2, force);
        local_f += JxW[qp] * vec_n2;
        

        // calculate the Jacobian if requested
        if (request_jacobian) {
            
            // we need the derivative of cp wrt normal velocity
            (*dpressure_dxdot)(qpoint[qp], _time, p_val);
            
            // calculate the component of Jacobian due to w-velocity
            Bmat_w.right_multiply_transpose(mat_n2n2, Bmat_w);
            local_jac_xdot += (JxW[qp] * p_val * normal(2)) * mat_n2n2;

            // now calculate the component of Jacobian
            (*dpressure_dx)(qpoint[qp], _time, p_val);
            
            // derivative wrt x
            mat_22.setZero(2,2);
            mat_22(0,0)  =  vel_vec(0);
            dBmat.left_multiply(mat_n1n2, mat_22);
            Bmat_w.right_multiply_transpose(mat_n2n2, mat_n1n2);  // v: B^T dB/dx
            local_jac += (JxW[qp] * p_val * normal(2)) * mat_n2n2;
            
            // derivative wrt y
            mat_22.setZero(2,2);
            mat_22(1,1)  =  vel_vec(1);
            dBmat.left_multiply(mat_n1n2, mat_22);
            Bmat_w.right_multiply_transpose(mat_n2n2, mat_n1n2);  // v: B^T dB/dy
            local_jac += (JxW[qp] * p_val * normal(2)) * mat_n2n2;
        }
    }
    
    
    // now transform to the global system and add
    transform_vector_to_global_system(local_f, vec_n2);
    f -= vec_n2;
    
    // if the Jacobian was requested, then transform it and add to the
    // global Jacobian
    if (request_jacobian) {
        transform_matrix_to_global_system(local_jac_xdot, mat_n2n2);
        jac_xdot -= mat_n2n2;
        
        transform_matrix_to_global_system(local_jac, mat_n2n2);
        jac      -= mat_n2n2;
    }
    
    return request_jacobian;
}




bool
MAST::StructuralElement2D::
piston_theory_residual_sensitivity(const MAST::FunctionBase& p,
                                   bool request_jacobian,
                                   RealVectorX &f,
                                   RealMatrixX& jac_xdot,
                                   RealMatrixX& jac,
                                   MAST::BoundaryConditionBase& bc) {
    

    libmesh_assert(_elem.dim() < 3); // only applicable for lower dimensional elements
    libmesh_assert(!follower_forces); // not implemented yet for follower forces
    
    
    const std::vector<Real> &JxW                = _fe->get_JxW();
    const std::vector<libMesh::Point>& qpoint   = _fe->get_xyz();
    const std::vector<std::vector<Real> >& phi  = _fe->get_phi();
    const unsigned int
    n_phi = (unsigned int)phi.size(),
    n1    = 2,
    n2    = _system.n_vars()*n_phi;
    
    
    // normal for face integration
    libMesh::Point normal;
    // direction of pressure assumed to be normal (along local z-axis)
    // to the element face for 2D and along local y-axis for 1D element.
    normal(_elem.dim()) = -1.;
    
    
    // convert to piston theory boundary condition so that the necessary
    // flow properties can be obtained
    const MAST::PistonTheoryBoundaryCondition& piston_bc =
    dynamic_cast<MAST::PistonTheoryBoundaryCondition&>(bc);
    
    
    // create the constant field functions to pass the dwdx and dwdt values
    // to the piston theory pressure functions
    MAST::Parameter
    dwdx_p  ("dwdx", 0.),
    dwdt_p  ("dwdt", 0.);
    
    MAST::ConstantFieldFunction
    dwdx_f  ("dwdx", dwdx_p),
    dwdt_f  ("dwdx", dwdt_p);
    
    
    std::unique_ptr<MAST::FieldFunction<Real> >
    pressure        (piston_bc.get_pressure_function(dwdx_f, dwdt_f).release()),
    dpressure_dx    (piston_bc.get_dpdx_function    (dwdx_f, dwdt_f).release()),
    dpressure_dxdot (piston_bc.get_dpdxdot_function (dwdx_f, dwdt_f).release());
    
    FEMOperatorMatrix
    Bmat_w,         // operator matrix for the w-displacement
    dBmat;          // operator matrix to calculate the derivativ of w wrt x and y
    
    dBmat.reinit(n1, _system.n_vars(), n_phi);
    
    RealVectorX
    phi_vec  = RealVectorX::Zero(n_phi),
    force    = RealVectorX::Zero(n1),
    local_f  = RealVectorX::Zero(n2),
    vec_n1   = RealVectorX::Zero(n1),
    vec_n2   = RealVectorX::Zero(n2),
    vel_vec  = RealVectorX::Zero(3),
    dummy    = RealVectorX::Zero(3);
    
    RealMatrixX
    dwdx            = RealMatrixX::Zero(3,2),
    local_jac_xdot  = RealMatrixX::Zero(n2,n2),
    local_jac       = RealMatrixX::Zero(n2,n2),
    mat_n2n2        = RealMatrixX::Zero(n2,n2),
    mat_n1n2        = RealMatrixX::Zero(n1,n2),
    mat_22          = RealMatrixX::Zero(2,2);
    
    // we need the velocity vector in the local coordinate system so that
    // the appropriate component of the w-derivative can be used
    vel_vec = _Tmatrix().transpose() * piston_bc.vel_vec();
    
    Real
    dwdt_val = 0.,
    dwdx_val = 0.,
    p_val    = 0.;
    
    
    for (unsigned int qp=0; qp<qpoint.size(); qp++)
    {
        
        // now set the shape function values
        for ( unsigned int i_nd=0; i_nd<n_phi; i_nd++ )
            phi_vec(i_nd) = phi[i_nd][qp];
        
        // initialize the B matrix for only the w-displacement
        Bmat_w.reinit(n1, _system.n_vars(), n_phi);
        Bmat_w.set_shape_function(0, 2, phi_vec);  // interpolates w-displacement
        
        // use the Bmat to calculate the velocity vector. Only the w-displacement
        // is of interest in the local coordinate, since that is the only
        // component normal to the surface.
        Bmat_w.right_multiply(vec_n1, _local_vel);
        dwdt_val = vec_n1(0);
        
        // get the operators for dw/dx and dw/dy to calculate the
        // normal velocity. We will use the von Karman strain operators
        // for this
        this->initialize_von_karman_strain_operator(qp,
                                                    *_fe,
                                                    dummy,
                                                    dwdx,
                                                    dBmat);
        
        // the diagonal of dwdx matrix stores the
        dwdx_val = 0.;
        for (unsigned int i=0; i<2; i++)
            dwdx_val  +=  dwdx(i,i) * vel_vec(i);   // (dw/dx_i)*U_inf . n_i
        
        // calculate the pressure value
        dwdx_p = dwdx_val;
        dwdt_p = dwdt_val;
        pressure->derivative(p, qpoint[qp], _time, p_val);
        
        // calculate force
        force(0) = p_val * normal(2);
        
        
        Bmat_w.vector_mult_transpose(vec_n2, force);
        local_f += JxW[qp] * vec_n2;
        
        
        // calculate the Jacobian if requested
        if (request_jacobian) {
            
            // we need the derivative of cp wrt normal velocity
            dpressure_dxdot->derivative(p, qpoint[qp], _time, p_val);
            
            // calculate the component of Jacobian due to w-velocity
            Bmat_w.right_multiply_transpose(mat_n2n2, Bmat_w);
            local_jac_xdot += (JxW[qp] * p_val * normal(2)) * mat_n2n2;
            
            // now calculate the component of Jacobian
            dpressure_dx->derivative(p, qpoint[qp], _time, p_val);
            
            // derivative wrt x
            mat_22.setZero(2,2);
            mat_22(0,0)  =  vel_vec(0);
            dBmat.left_multiply(mat_n1n2, mat_22);
            Bmat_w.right_multiply_transpose(mat_n2n2, mat_n1n2);  // v: B^T dB/dx
            local_jac += (JxW[qp] * p_val * normal(2)) * mat_n2n2;
            
            // derivative wrt y
            mat_22.setZero(2,2);
            mat_22(1,1)  =  vel_vec(1);
            dBmat.left_multiply(mat_n1n2, mat_22);
            Bmat_w.right_multiply_transpose(mat_n2n2, mat_n1n2);  // v: B^T dB/dy
            local_jac += (JxW[qp] * p_val * normal(2)) * mat_n2n2;
        }
    }
    
    
    // now transform to the global system and add
    transform_vector_to_global_system(local_f, vec_n2);
    f -= vec_n2;
    
    // if the Jacobian was requested, then transform it and add to the
    // global Jacobian
    if (request_jacobian) {
        transform_matrix_to_global_system(local_jac_xdot, mat_n2n2);
        jac_xdot -= mat_n2n2;
        
        transform_matrix_to_global_system(local_jac, mat_n2n2);
        jac      -= mat_n2n2;
    }
    
    
    return request_jacobian;
}
