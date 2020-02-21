// Catch2 includes
#include "catch.hpp"

// libMesh includes
#include "libmesh/point.h"

// MAST includes
#include "base/parameter.h"
#include "base/constant_field_function.h"
#include "property_cards/isotropic_material_property_card.h"
#include "property_cards/solid_1d_rod_section_element_property_card.h"

// Custom includes
#include "test_helpers.h"

extern libMesh::LibMeshInit* p_global_init;

#define PI 3.1415926535897932

/**
 * A BAR is defined as a solid rectangular cross section defined by two 
 * parameters.
 */
TEST_CASE("rod_element_property_card_constant_base_1d",
          "[1D],[isotropic],[constant],[property]")
{
    const uint dim = 1;
        
    // Define Material Properties as MAST Parameters
    MAST::Parameter rho("rho_param", 1420.5);         // Density
    MAST::Parameter E("E_param", 72.0e9);             // Modulus of Elasticity
    MAST::Parameter nu("nu_param", 0.33);             // Poisson's ratio
    MAST::Parameter cp("cp_param",   908.0);          // Specific Heat Capacity
    MAST::Parameter k("k_param",     237.0);          // Thermal Conductivity
    
    const Real G = E() / (2. * (1. + nu()));
    
    // Define Section Properties as MAST Parameters
    MAST::Parameter DIM1("DIM1", 3.234);   // Section thickness in z-direction
    MAST::Parameter offset_y("offy_param", 0.287);     // Section offset in y-direction
    MAST::Parameter offset_z("offz_param", -1.654);     // Section offset in z-direction
    
    // Define Sensitivity Parameters
    std::vector<MAST::Parameter*> sens_params = {&DIM1};
    uint n_s = sens_params.size();
    
    // Create field functions to dsitribute these constant parameters throughout the model
    // Section Property Field Functions
    MAST::ConstantFieldFunction DIM1_f("DIM1", DIM1);
    MAST::ConstantFieldFunction offsety_f("hy_off", offset_y);
    MAST::ConstantFieldFunction offsetz_f("hz_off", offset_z);
    // Material Property Field Functions
    MAST::ConstantFieldFunction rho_f("rho", rho);
    MAST::ConstantFieldFunction E_f("E", E);
    MAST::ConstantFieldFunction nu_f("nu", nu);
    MAST::ConstantFieldFunction cp_f("cp", cp);
    MAST::ConstantFieldFunction k_f("k_th", k);
    
    // Initialize the material
    MAST::IsotropicMaterialPropertyCard material;                   
    
    // Add the material property constant field functions to the material card
    material.add(rho_f);
    material.add(k_f);
    material.add(cp_f);
    material.add(E_f);
    material.add(nu_f);
    
    // Initialize the section
    MAST::Solid1DRodSectionElementPropertyCard section;
    
    // Add the section property constant field functions to the section card
    section.add(DIM1_f);
    
    section.add(offsety_f);
    section.add(offsetz_f);
    
    // Add the material card to the section card
    section.set_material(material);
    
    // Specify a section orientation point and add it to the section.
    RealVectorX orientation = RealVectorX::Zero(3);
    orientation(1) = 1.0;
    section.y_vector() = orientation;
    
    // Now initialize the section
    section.init(*p_global_init);
    
    REQUIRE( section.dim() == dim); // Ensure section is 1 dimensional
    REQUIRE( section.depends_on(DIM1) );
    REQUIRE( section.depends_on(offset_y) );
    REQUIRE( section.depends_on(offset_z) );
    REQUIRE( section.depends_on(k) );
    REQUIRE( section.depends_on(cp) );
    REQUIRE( section.depends_on(rho) );
    
    REQUIRE( section.if_isotropic() );
    
    // True values
    const Real r = DIM1();
    const Real area_true = PI*r*r;
    const Real torsion_constant_true = PI*pow(r, 4.0)/2.0;
    const Real first_area_moment_z_true = area_true * offset_y();
    const Real first_area_moment_y_true = area_true * offset_z();
    const Real Izzc_true = PI*pow(r, 4.0)/4.0;
    const Real Iyyc_true = PI*pow(r, 4.0)/4.0;
    const Real Izyc_true = 0.0;
    const Real Ipc_true = PI*pow(r, 4.0)/2.0;
    const Real second_area_moment_zz_true = Izzc_true + area_true * pow(offset_y(), 2.);
    const Real second_area_moment_yy_true = Iyyc_true + area_true * pow(offset_z(), 2.);
    const Real second_area_moment_zy_true = Izyc_true + area_true * offset_y() * offset_z();
    const Real second_area_moment_polar_true = second_area_moment_zz_true + second_area_moment_yy_true;
    const Real warping_constant_true = 0.0;
    const Real kappa_z_true = 8.4967018479575718e-01;
    const Real kappa_y_true = 8.4967018474199052e-01;
    const Real xs_true = offset_z();
    const Real ys_true = offset_y();
    const Real xc_true = offset_z();
    const Real yc_true = offset_y();
    const libMesh::Point shear_center_true(xs_true, ys_true, 0.);
    const libMesh::Point centroid_true(xc_true, yc_true, 0.);
    
    const libMesh::Point point(4.3, -3.5, -6.7);
    const Real time = 8.22;
    
    Real area;
    const MAST::FieldFunction<Real>& Area = section.A();
    Area(point, time, area);
    REQUIRE( area == Approx(area_true) );
    
    Real first_area_moment_y;
    const MAST::FieldFunction<Real>& Ay = section.Ay();
    Ay(point, time, first_area_moment_y);
    CHECK( first_area_moment_y == Approx(first_area_moment_y_true) );
    
    Real first_area_moment_z;
    const MAST::FieldFunction<Real>& Az = section.Az();
    Az(point, time, first_area_moment_z);
    CHECK( first_area_moment_z == Approx(first_area_moment_z_true) );
    
    RealMatrixX I;
    const MAST::FieldFunction<RealMatrixX>& Inertias = section.I();
    Inertias(point, time, I);
    REQUIRE( I(0,1) == I(1,0) );
    Real Iyy = I(1,1);
    Real Izz = I(0,0);
    Real Izy = I(0,1);
    REQUIRE( Izz == Approx(second_area_moment_zz_true) );
    REQUIRE( Iyy == Approx(second_area_moment_yy_true) );
    REQUIRE( Izy == Approx(second_area_moment_zy_true) );
    
    Real Ip;
    const MAST::FieldFunction<Real>& PolarInertia = section.Ip();
    PolarInertia(point, time, Ip);
    REQUIRE( Ip == Approx(second_area_moment_polar_true) );
    
    Real torsion_constant;
    const MAST::FieldFunction<Real>& TorsionConstant = section.J();
    TorsionConstant(point, time, torsion_constant);
    REQUIRE( torsion_constant == Approx(torsion_constant_true));
    
    Real warping_constant;
    const MAST::FieldFunction<Real>& WarpingConstant = section.Gam();
    WarpingConstant(point, time, warping_constant);
    REQUIRE( warping_constant == Approx(warping_constant_true) );
    
    RealMatrixX shear_coefficients;
    const MAST::FieldFunction<RealMatrixX>& ShearCoefficientMatrix = section.Kap();
    ShearCoefficientMatrix(point, time, shear_coefficients);
    REQUIRE( shear_coefficients(0,0) == Approx(kappa_z_true) );
    REQUIRE( shear_coefficients(1,1) == Approx(kappa_y_true) );
    
    libMesh::Point centroid = section.get_centroid(point, time);
    REQUIRE( centroid(0) == Approx(centroid_true(0)) );
    REQUIRE( centroid(1) == Approx(centroid_true(1)) );
    
    libMesh::Point shear_center = section.get_shear_center(point, time);
    REQUIRE(shear_center(0) == Approx(xs_true));
    REQUIRE(shear_center(1) == Approx(ys_true));
    
    REQUIRE_FALSE( section.if_diagonal_mass_matrix() );
    
    section.set_diagonal_mass_matrix(true);
    REQUIRE( section.if_diagonal_mass_matrix() );
}


/*!
 * These sensitivity checks are performed against a 4th order accurate
 * central difference approximation with a perturbation of 1.22e-04.
 */
TEST_CASE("rod_element_property_card_constant_base_sensitivity_1d",
          "[1D],[isotropic],[constant],[property],[sensitivity]")
{
    const uint dim = 1;
        
    // Define Material Properties as MAST Parameters
    MAST::Parameter rho("rho_param", 1420.5);         // Density
    MAST::Parameter E("E_param", 72.0e9);             // Modulus of Elasticity
    MAST::Parameter nu("nu_param", 0.33);             // Poisson's ratio
    MAST::Parameter cp("cp_param",   908.0);          // Specific Heat Capacity
    MAST::Parameter k("k_param",     237.0);          // Thermal Conductivity
    
    const Real G = E() / (2. * (1. + nu()));
    
    // Define Section Properties as MAST Parameters
    MAST::Parameter DIM1("DIM1", 3.234);   // Section thickness in z-direction
    MAST::Parameter offset_y("offy_param", 0.287);     // Section offset in y-direction
    MAST::Parameter offset_z("offz_param", -1.654);     // Section offset in z-direction
    
    // Define Sensitivity Parameters
    std::vector<MAST::Parameter*> sens_params = {&DIM1};
    uint n_s = sens_params.size();
    
    // Create field functions to dsitribute these constant parameters throughout the model
    // Section Property Field Functions
    MAST::ConstantFieldFunction DIM1_f("DIM1", DIM1);
    MAST::ConstantFieldFunction offsety_f("hy_off", offset_y);
    MAST::ConstantFieldFunction offsetz_f("hz_off", offset_z);
    // Material Property Field Functions
    MAST::ConstantFieldFunction rho_f("rho", rho);
    MAST::ConstantFieldFunction E_f("E", E);
    MAST::ConstantFieldFunction nu_f("nu", nu);
    MAST::ConstantFieldFunction cp_f("cp", cp);
    MAST::ConstantFieldFunction k_f("k_th", k);
    
    // Initialize the material
    MAST::IsotropicMaterialPropertyCard material;                   
    
    // Add the material property constant field functions to the material card
    material.add(rho_f);
    material.add(k_f);
    material.add(cp_f);
    material.add(E_f);
    material.add(nu_f);
    
    // Initialize the section
    MAST::Solid1DRodSectionElementPropertyCard section;
    
    // Add the section property constant field functions to the section card
    section.add(DIM1_f);
    section.add(offsety_f);
    section.add(offsetz_f);
    
    // Add the material card to the section card
    section.set_material(material);
    
    // Specify a section orientation point and add it to the section.
    RealVectorX orientation = RealVectorX::Zero(3);
    orientation(1) = 1.0;
    section.y_vector() = orientation;
    
    // Now initialize the section
    section.init(*p_global_init);
    
    REQUIRE( section.dim() == dim); // Ensure section is 1 dimensional
    REQUIRE( section.depends_on(DIM1) );
    REQUIRE( section.depends_on(offset_y) );
    REQUIRE( section.depends_on(offset_z) );
    REQUIRE( section.depends_on(k) );
    REQUIRE( section.depends_on(cp) );
    REQUIRE( section.depends_on(rho) );
    
    REQUIRE( section.if_isotropic() );
    
    // True values
    const Real r = DIM1();
    const Real area_true = PI*r*r;
    const Real torsion_constant_true = PI*pow(r, 4.0)/2.0;
    const Real first_area_moment_z_true = area_true * offset_y();
    const Real first_area_moment_y_true = area_true * offset_z();
    const Real Izzc_true = PI*pow(r, 4.0)/4.0;
    const Real Iyyc_true = PI*pow(r, 4.0)/4.0;
    const Real Izyc_true = 0.0;
    const Real Ipc_true = PI*pow(r, 4.0)/2.0;
    const Real second_area_moment_zz_true = Izzc_true + area_true * pow(offset_y(), 2.);
    const Real second_area_moment_yy_true = Iyyc_true + area_true * pow(offset_z(), 2.);
    const Real second_area_moment_zy_true = Izyc_true + area_true * offset_y() * offset_z();
    const Real second_area_moment_polar_true = second_area_moment_zz_true + second_area_moment_yy_true;
    const Real warping_constant_true = 0.0;
    const Real kappa_z_true = 8.4967018479575718e-01;
    const Real kappa_y_true = 8.4967018474199052e-01;
    const Real xs_true = offset_z();
    const Real ys_true = offset_y();
    const Real xc_true = offset_z();
    const Real yc_true = offset_y();
    const libMesh::Point shear_center_true(xs_true, ys_true, 0.);
    const libMesh::Point centroid_true(xc_true, yc_true, 0.);
    
    const libMesh::Point point(4.3, -3.5, -6.7);
    const Real time = 8.22;
    
    Real dA_dDIM1, dA_dDIM2;
    Real dCz_dDIM1, dCz_dDIM2;
    Real dCy_dDIM1, dCy_dDIM2;
    Real dQz_dDIM1, dQz_dDIM2;
    Real dQy_dDIM1, dQy_dDIM2;
    Real dIzz_dDIM1, dIzz_dDIM2;
    Real dIyy_dDIM1, dIyy_dDIM2;
    Real dIzy_dDIM1, dIzy_dDIM2;
    Real dIp_dDIM1, dIp_dDIM2;
    Real dIzzc_dDIM1, dIzzc_dDIM2;
    Real dIyyc_dDIM1, dIyyc_dDIM2;
    Real dIzyc_dDIM1, dIzyc_dDIM2;
    Real dIpc_dDIM1, dIpc_dDIM2;
    Real dJ_dDIM1, dJ_dDIM2;
    Real dxs_dDIM1, dxs_dDIM2;
    Real dys_dDIM1, dys_dDIM2;
    Real dW_dDIM1, dW_dDIM2;
    
    Real f_h, f_2h, f_n, f_2n;
    RealVectorX fv_h, fv_2h, fv_n, fv_2n;
    RealMatrixX fm_h, fm_2h, fm_n, fm_2n;
    
    const Real delta = 1.220703125e-04; // (np.spacing(1))**(0.25)
    
    // Area Sensitivity Check
    libMesh::out << "\tArea Sensitivity Check..." << std::endl;
    const MAST::FieldFunction<Real>& Area = section.A();
    std::vector<Real> dA(n_s);
    for (uint i=0; i<n_s; i++)
    {
        Area.derivative(*sens_params[i], point, time, dA[i]);
    }
    
    std::vector<Real> dA_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        Area(point, time, f_h);
        
        (*sens_params[i])() += delta;
        Area(point, time, f_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        Area(point, time, f_n);
        
        (*sens_params[i])() -= delta;
        Area(point, time, f_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dA_cd[i] = (f_2n - 8.*f_n + 8*f_h - f_2h)/(12.*delta);
        
        REQUIRE(dA[i] == Approx(dA_cd[i]));
    }
    
    
    // Centroid Sensitivity Check
    libMesh::out << "\tCentroid Sensitivity Check..." << std::endl;
    std::vector<libMesh::Point> dC(n_s);
    for (uint i=0; i<n_s; i++)
    {
        dC[i] = section.get_centroid_derivative(*sens_params[i], point, time);
    }
    
    libMesh::Point fp_h, fp_2h, fp_n, fp_2n;
    std::vector<libMesh::Point> dC_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        fp_h = section.get_centroid(point, time);
        
        (*sens_params[i])() += delta;
        fp_2h = section.get_centroid(point, time);
        
        (*sens_params[i])() -= 3.0*delta;
        fp_n = section.get_centroid(point, time);
        
        (*sens_params[i])() -= delta;
        fp_2n = section.get_centroid(point, time);
        
        (*sens_params[i])() += 2.0*delta;
        
        dC_cd[i] = (fp_2n - 8.*fp_n + 8*fp_h - fp_2h)/(12.*delta);
        
        REQUIRE(dC[i](0) == Approx(dC_cd[i](0)).margin(1.49e-08) );
        REQUIRE(dC[i](1) == Approx(dC_cd[i](1)).margin(1.49e-08) );
    }
    
    // First Area Moments Sensitivity Check
    libMesh::out << "\tArea Moment Y Sensitivity Check..." << std::endl;
    const MAST::FieldFunction<Real>& Area_y = section.Ay();
    std::vector<Real> dAy(n_s);
    for (uint i=0; i<n_s; i++)
    {
        Area_y.derivative(*sens_params[i], point, time, dAy[i]);
    }
    
    std::vector<Real> dAy_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        Area_y(point, time, f_h);
        
        (*sens_params[i])() += delta;
        Area_y(point, time, f_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        Area_y(point, time, f_n);
        
        (*sens_params[i])() -= delta;
        Area_y(point, time, f_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dAy_cd[i] = (f_2n - 8.*f_n + 8*f_h - f_2h)/(12.*delta);
        
        REQUIRE(dAy[i] == Approx(dAy_cd[i]));
    }
    
    libMesh::out << "\tArea Momemnt Z Sensitivity Check..." << std::endl;
    const MAST::FieldFunction<Real>& Area_z = section.Az();
    std::vector<Real> dAz(n_s);
    for (uint i=0; i<n_s; i++)
    {
        Area_z.derivative(*sens_params[i], point, time, dAz[i]);
    }
    
    std::vector<Real> dAz_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        Area_z(point, time, f_h);
        
        (*sens_params[i])() += delta;
        Area_z(point, time, f_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        Area_z(point, time, f_n);
        
        (*sens_params[i])() -= delta;
        Area_z(point, time, f_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dAz_cd[i] = (f_2n - 8.*f_n + 8*f_h - f_2h)/(12.*delta);
        
        REQUIRE(dAz[i] == Approx(dAz_cd[i]));
    }
    
    
    // Second Area Moments Sensitivity Check
    libMesh::out << "\tSecond Area Moments Sensitivity Check..." << std::endl;
    const MAST::FieldFunction<RealMatrixX>& Inertia = section.I();
    std::vector<RealMatrixX> dI(n_s);
    for (uint i=0; i<n_s; i++)
    {
        Inertia.derivative(*sens_params[i], point, time, dI[i]);
    }
    
    std::vector<RealMatrixX> dI_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        Inertia(point, time, fm_h);
        
        (*sens_params[i])() += delta;
        Inertia(point, time, fm_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        Inertia(point, time, fm_n);
        
        (*sens_params[i])() -= delta;
        Inertia(point, time, fm_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dI_cd[i] = (fm_2n - 8.*fm_n + 8*fm_h - fm_2h)/(12.*delta);
                
        Real dIzz = dI[i](0,0);
        Real dIyy = dI[i](1,1);
        Real dIyz = dI[i](1,0);
        Real dIzy = dI[i](0,1);
        
        Real dIzz_cd = dI_cd[i](0,0);
        Real dIyy_cd = dI_cd[i](1,1);
        Real dIyz_cd = dI_cd[i](1,0);
        Real dIzy_cd = dI_cd[i](0,1);
        
        REQUIRE(dIzz == Approx(dIzz_cd));
        REQUIRE(dIyy == Approx(dIyy_cd));
        REQUIRE(dIyz == Approx(dIyz_cd));
        REQUIRE(dIzy == Approx(dIzy_cd));
        REQUIRE(dIyz == dIzy); // symmetry check
    }
    
    
    // Second Area Polar Moment Sensitivity Check
    libMesh::out << "\tArea Polar Moment Sensitivity Check..." << std::endl;
    const MAST::FieldFunction<Real>& PolarInertia = section.Ip();
    std::vector<Real> dIp(n_s);
    for (uint i=0; i<n_s; i++)
    {
        PolarInertia.derivative(*sens_params[i], point, time, dIp[i]);
    }
    
    std::vector<Real> dIp_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        PolarInertia(point, time, f_h);
        
        (*sens_params[i])() += delta;
        PolarInertia(point, time, f_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        PolarInertia(point, time, f_n);
        
        (*sens_params[i])() -= delta;
        PolarInertia(point, time, f_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dIp_cd[i] = (f_2n - 8.*f_n + 8.*f_h - f_2h)/(12.*delta);
        
        REQUIRE(dIp[i] == Approx(dIp_cd[i]));
    }
    
    
//     // Shear Center Sensitivity Check
//     std::vector<libMesh::Point> dCs(n_s);
//     for (uint i=0; i<n_s; i++)
//     {
//         dCs[i] = section.get_shear_center_derivative(*sens_params[i], point, time);
//     }
//     
//     std::vector<libMesh::Point> dCs_cd(n_s);
//     for (uint i=0; i<n_s; i++)
//     {
//         (*sens_params[i])() += delta;
//         fp_h = section.get_shear_center(point, time);
//         
//         (*sens_params[i])() += delta;
//         fp_2h = section.get_shear_center(point, time);
//         
//         (*sens_params[i])() -= 3.0*delta;
//         fp_n = section.get_shear_center(point, time);
//         
//         (*sens_params[i])() -= delta;
//         fp_2n = section.get_shear_center(point, time);
//         
//         (*sens_params[i])() += 2.0*delta;
//         
//         dCs_cd[i] = (fp_2n - 8.*fp_n + 8.*fp_h - fp_2h)/(12.*delta);
//         
//         libMesh::out << "dCs_d" << sens_params[i]->name() << " = " << dCs[i] << "\tdCs_cd = " << dCs_cd[i] << std::endl;
//         REQUIRE(dCs[i](0) == Approx(dCs_cd[i](0)) );
//         REQUIRE(dCs[i](1) == Approx(dCs_cd[i](1)) );
//     }
    
    
    // Torsion Constant Sensitivity Check
    // NOTE: The field function below is not made constant because currently a finite difference is used to calculate sensitivity.
    libMesh::out << "\tTorsion Constant Sensitivity Check..." << std::endl;
    MAST::FieldFunction<Real>& TorsionConstant = section.J();
    std::vector<Real> dJ(n_s);
    for (uint i=0; i<n_s; i++)
    {
        TorsionConstant.derivative(*sens_params[i], point, time, dJ[i]);
    }
    
    std::vector<Real> dJ_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        TorsionConstant(point, time, f_h);
        
        (*sens_params[i])() += delta;
        TorsionConstant(point, time, f_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        TorsionConstant(point, time, f_n);
        
        (*sens_params[i])() -= delta;
        TorsionConstant(point, time, f_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dJ_cd[i] = (f_2n - 8.*f_n + 8*f_h - f_2h)/(12.*delta);
        
        REQUIRE(dJ[i] == Approx(dJ_cd[i]).epsilon(0.1) );
        // NOTE: 10% error margin due to 'exact' sensitivity being calculated using finite difference
    }
    
    
//     // Shear Coefficient Sensitivity Check
//     // NOTE: The field function below is not made constant because currently a finite difference is used to calculate sensitivity.
//     libMesh::out << "\tShear Coefficient Sensitivity Check..." << std::endl;
//     MAST::FieldFunction<RealMatrixX>& Kappa = section.Kap();
//     std::vector<RealMatrixX> dK(n_s);
//     for (uint i=0; i<n_s; i++)
//     {
//         Kappa.derivative(*sens_params[i], point, time, dK[i]);
//     }
//     
//     std::vector<RealMatrixX> dK_cd(n_s);
//     for (uint i=0; i<n_s; i++)
//     {
//         (*sens_params[i])() += delta;
//         Kappa(point, time, fm_h);
//         
//         (*sens_params[i])() += delta;
//         Kappa(point, time, fm_2h);
//         
//         (*sens_params[i])() -= 3.0*delta;
//         Kappa(point, time, fm_n);
//         
//         (*sens_params[i])() -= delta;
//         Kappa(point, time, fm_2n);
//         
//         (*sens_params[i])() += 2.0*delta;
//         
//         dK_cd[i] = (fm_2n + 8.*(fm_h - fm_n) - fm_2h)/(12.*delta);
//                 
//         Real dKzz = dK[i](0,0);
//         Real dKyy = dK[i](1,1);
//         Real dKyz = dK[i](1,0);
//         Real dKzy = dK[i](0,1);
//         
//         Real dKzz_cd = dK_cd[i](0,0);
//         Real dKyy_cd = dK_cd[i](1,1);
//         Real dKyz_cd = dK_cd[i](1,0);
//         Real dKzy_cd = dK_cd[i](0,1);
//         
//         REQUIRE(dKzz == Approx(dKzz_cd).epsilon(0.1));
//         REQUIRE(dKyy == Approx(dKyy_cd).epsilon(0.1));
//         //REQUIRE(dKyz == Approx(dKyz_cd).epsilon(0.1)); // Comparison can be hard when this is nearly infinite (~1e+13)
//         //REQUIRE(dKzy == Approx(dKzy_cd).epsilon(0.1)); // Comparison can be hard when this is nearly infinite (~1e+13)
//         // NOTE: 10% error margin due to 'exact' sensitivity being calculated using finite difference
//         REQUIRE(dKyz == dKzy); // symmetry check
//     }
    
    
    // Warping Constant Sensitivity Check
    // NOTE: The field function below is not made constant because currently a finite difference is used to calculate sensitivity.
    libMesh::out << "\tWarping Constant Sensitivity Check..." << std::endl;
    MAST::FieldFunction<Real>& WarpingConstant = section.Gam();
    std::vector<Real> dW(n_s);
    for (uint i=0; i<n_s; i++)
    {
        WarpingConstant.derivative(*sens_params[i], point, time, dW[i]);
    }
    
    std::vector<Real> dW_cd(n_s);
    for (uint i=0; i<n_s; i++)
    {
        (*sens_params[i])() += delta;
        WarpingConstant(point, time, f_h);
        
        (*sens_params[i])() += delta;
        WarpingConstant(point, time, f_2h);
        
        (*sens_params[i])() -= 3.0*delta;
        WarpingConstant(point, time, f_n);
        
        (*sens_params[i])() -= delta;
        WarpingConstant(point, time, f_2n);
        
        (*sens_params[i])() += 2.0*delta;
        
        dW_cd[i] = (f_2n - 8.*f_n + 8*f_h - f_2h)/(12.*delta);
        
        libMesh::out << "dW_d" << sens_params[i]->name() << " = " << dW[i] << "\tdW_cd = " << dW_cd[i] << std::endl;
        REQUIRE(dW[i] == Approx(dW_cd[i]).epsilon(0.1) );
        // NOTE: 10% error margin due to 'exact' sensitivity being calculated using finite difference
    }
}


TEST_CASE("rod_element_property_card_constant_heat_transfer_1d",
          "[heat_transfer],[1D],[isotropic],[constant],[property]")
{
    const uint dim = 1;
        
    // Define Material Properties as MAST Parameters
    MAST::Parameter rho("rho_param", 1420.5);         // Density
    MAST::Parameter E("E_param", 72.0e9);             // Modulus of Elasticity
    MAST::Parameter nu("nu_param", 0.33);             // Poisson's ratio
    MAST::Parameter cp("cp_param",   908.0);          // Specific Heat Capacity
    MAST::Parameter k("k_param",     237.0);          // Thermal Conductivity
    
    const Real G = E() / (2. * (1. + nu()));
    
    // Define Section Properties as MAST Parameters
    MAST::Parameter DIM1("DIM1", 3.234);   // Section thickness in z-direction
    MAST::Parameter offset_y("offy_param", 0.287);     // Section offset in y-direction
    MAST::Parameter offset_z("offz_param", -1.654);     // Section offset in z-direction
    
    // Create field functions to dsitribute these constant parameters throughout the model
    // Section Property Field Functions
    MAST::ConstantFieldFunction DIM1_f("DIM1", DIM1);
    MAST::ConstantFieldFunction offsety_f("hy_off", offset_y);
    MAST::ConstantFieldFunction offsetz_f("hz_off", offset_z);
    // Material Property Field Functions
    MAST::ConstantFieldFunction rho_f("rho", rho);
    MAST::ConstantFieldFunction E_f("E", E);
    MAST::ConstantFieldFunction nu_f("nu", nu);
    MAST::ConstantFieldFunction cp_f("cp", cp);
    MAST::ConstantFieldFunction k_f("k_th", k);
    
    // Initialize the material
    MAST::IsotropicMaterialPropertyCard material;                   
    
    // Add the material property constant field functions to the material card
    material.add(rho_f);
    material.add(k_f);
    material.add(cp_f);
    material.add(E_f);
    material.add(nu_f);
    
    // Initialize the section
    MAST::Solid1DRodSectionElementPropertyCard section;
    
    // Add the section property constant field functions to the section card
    section.add(DIM1_f);
    section.add(offsety_f);
    section.add(offsetz_f);
    
    // Add the material card to the section card
    section.set_material(material);
    
    // Specify a section orientation point and add it to the section.
    RealVectorX orientation = RealVectorX::Zero(3);
    orientation(1) = 1.0;
    section.y_vector() = orientation;
    
    // Now initialize the section
    section.init(*p_global_init);
    
    REQUIRE( section.dim() == dim); // Ensure section is 1 dimensional
    REQUIRE( section.depends_on(DIM1) );
    REQUIRE( section.depends_on(offset_y) );
    REQUIRE( section.depends_on(offset_z) );
    REQUIRE( section.depends_on(k) );
    REQUIRE( section.depends_on(cp) );
    REQUIRE( section.depends_on(rho) );
    
    REQUIRE( section.if_isotropic() );
    
    // True values
    const Real r = DIM1();
    const Real area_true = PI*r*r;
    const Real torsion_constant_true = PI*pow(r, 4.0)/2.0;
    const Real first_area_moment_z_true = area_true * offset_y();
    const Real first_area_moment_y_true = area_true * offset_z();
    const Real Izzc_true = PI*pow(r, 4.0)/4.0;
    const Real Iyyc_true = PI*pow(r, 4.0)/4.0;
    const Real Izyc_true = 0.0;
    const Real Ipc_true = PI*pow(r, 4.0)/2.0;
    const Real second_area_moment_zz_true = Izzc_true + area_true * pow(offset_y(), 2.);
    const Real second_area_moment_yy_true = Iyyc_true + area_true * pow(offset_z(), 2.);
    const Real second_area_moment_zy_true = Izyc_true + area_true * offset_y() * offset_z();
    const Real second_area_moment_polar_true = second_area_moment_zz_true + second_area_moment_yy_true;
    const Real warping_constant_true = 0.0;
    const Real kappa_z_true = 8.4967018479575718e-01;
    const Real kappa_y_true = 8.4967018474199052e-01;
    const Real xs_true = offset_z();
    const Real ys_true = offset_y();
    const Real xc_true = offset_z();
    const Real yc_true = offset_y();
    const libMesh::Point shear_center_true(xs_true, ys_true, 0.);
    const libMesh::Point centroid_true(xc_true, yc_true, 0.);
    
    const libMesh::Point point(4.3, -3.5, -6.7);
    const Real time = 8.22;
    
    Real area;
    const MAST::FieldFunction<Real>& Area = section.A();
    Area(point, time, area);
    REQUIRE( area == Approx(area_true) );
    

    SECTION("1D section thermal conductance matrix")
    {
        /** 
         * As of Dec. 17, 2019, inertia_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * inertia_matrix to be obtained without any input arguments.
         */
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> conduct_mat = section.thermal_conductance_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_conduc;
        conduct_mat->operator()(point, time, D_sec_conduc);
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_conduc_true = RealMatrixX::Zero(1,1);
        D_sec_conduc_true(0,0) = k() * area_true;
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_conduc);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_conduc_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
    
    SECTION("1D section thermal capacitance matrix")
    {
        /** 
         * As of Dec. 17, 2019, inertia_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * inertia_matrix to be obtained without any input arguments.
         */
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> capaci_mat = section.thermal_capacitance_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_capac;
        capaci_mat->operator()(point, time, D_sec_capac);
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_capac_true = RealMatrixX::Zero(1,1);
        D_sec_capac_true(0,0) = rho() * cp() * area_true;
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_capac);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_capac_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
}


TEST_CASE("rod_element_property_card_constant_thermoelastic_1d",
          "[thermoelastic],[1D],[isotropic],[constant],[property]")
{
    const uint dim = 1;
        
    // Define Material Properties as MAST Parameters
    MAST::Parameter rho("rho_param", 1420.5);         // Density
    MAST::Parameter E("E_param", 72.0e9);             // Modulus of Elasticity
    MAST::Parameter nu("nu_param", 0.33);             // Poisson's ratio
    MAST::Parameter cp("cp_param",   908.0);          // Specific Heat Capacity
    MAST::Parameter k("k_param",     237.0);          // Thermal Conductivity
    MAST::Parameter alpha("alpha_param", 5.73e-05);  // Coeff. of Thermal Exp.
    
    const Real G = E() / (2. * (1. + nu()));
    
    // Define Section Properties as MAST Parameters
    MAST::Parameter DIM1("DIM1", 3.234);   // Section thickness in z-direction
    MAST::Parameter offset_y("offy_param", 0.287);     // Section offset in y-direction
    MAST::Parameter offset_z("offz_param", -1.654);     // Section offset in z-direction
    
    // Create field functions to dsitribute these constant parameters throughout the model
    // Section Property Field Functions
    MAST::ConstantFieldFunction DIM1_f("DIM1", DIM1);
    MAST::ConstantFieldFunction offsety_f("hy_off", offset_y);
    MAST::ConstantFieldFunction offsetz_f("hz_off", offset_z);
    // Material Property Field Functions
    MAST::ConstantFieldFunction rho_f("rho", rho);
    MAST::ConstantFieldFunction E_f("E", E);
    MAST::ConstantFieldFunction nu_f("nu", nu);
    MAST::ConstantFieldFunction cp_f("cp", cp);
    MAST::ConstantFieldFunction k_f("k_th", k);
    MAST::ConstantFieldFunction alpha_f("alpha_expansion", alpha);
    
    // Initialize the material
    MAST::IsotropicMaterialPropertyCard material;                   
    
    // Add the material property constant field functions to the material card
    material.add(rho_f);
    material.add(k_f);
    material.add(cp_f);
    material.add(E_f);
    material.add(nu_f);
    material.add(alpha_f);
    
    // Initialize the section
    MAST::Solid1DRodSectionElementPropertyCard section;
    
    // Add the section property constant field functions to the section card
    section.add(DIM1_f);
    section.add(offsety_f);
    section.add(offsetz_f);
    
    // Add the material card to the section card
    section.set_material(material);
    
    // Specify a section orientation point and add it to the section.
    RealVectorX orientation = RealVectorX::Zero(3);
    orientation(1) = 1.0;
    section.y_vector() = orientation;
    
    // Now initialize the section
    section.init(*p_global_init);
    
    REQUIRE( section.dim() == dim); // Ensure section is 1 dimensional
    REQUIRE( section.depends_on(DIM1) );
    REQUIRE( section.depends_on(offset_y) );
    REQUIRE( section.depends_on(offset_z) );
    REQUIRE( section.depends_on(k) );
    REQUIRE( section.depends_on(cp) );
    REQUIRE( section.depends_on(rho) );
    
    REQUIRE( section.if_isotropic() );
    
    // True values
    const Real r = DIM1();
    const Real area_true = PI*r*r;
    const Real torsion_constant_true = PI*pow(r, 4.0)/2.0;
    const Real first_area_moment_z_true = area_true * offset_y();
    const Real first_area_moment_y_true = area_true * offset_z();
    const Real Izzc_true = PI*pow(r, 4.0)/4.0;
    const Real Iyyc_true = PI*pow(r, 4.0)/4.0;
    const Real Izyc_true = 0.0;
    const Real Ipc_true = PI*pow(r, 4.0)/2.0;
    const Real second_area_moment_zz_true = Izzc_true + area_true * pow(offset_y(), 2.);
    const Real second_area_moment_yy_true = Iyyc_true + area_true * pow(offset_z(), 2.);
    const Real second_area_moment_zy_true = Izyc_true + area_true * offset_y() * offset_z();
    const Real second_area_moment_polar_true = second_area_moment_zz_true + second_area_moment_yy_true;
    const Real warping_constant_true = 0.0;
    const Real kappa_z_true = 8.4967018479575718e-01;
    const Real kappa_y_true = 8.4967018474199052e-01;
    const Real xs_true = offset_z();
    const Real ys_true = offset_y();
    const Real xc_true = offset_z();
    const Real yc_true = offset_y();
    const libMesh::Point shear_center_true(xs_true, ys_true, 0.);
    const libMesh::Point centroid_true(xc_true, yc_true, 0.);
    
    const libMesh::Point point(4.3, -3.5, -6.7);
    const Real time = 8.22;
    
    SECTION("1D thermal expansion A matrix")
    {
        /** 
         * As of Dec. 17, 2019, thermal_expansion_A_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * thermal_expansion_A_matrix to be obtained without any input arguments.
         */
        
        Real area;
        const MAST::FieldFunction<Real>& Area = section.A();
        Area(point, time, area);
        CHECK( area == Approx(area_true) );
        
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> texp_A_mat = section.thermal_expansion_A_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_texpA;
        texp_A_mat->operator()(point, time, D_sec_texpA);
                
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_texpA_true = RealMatrixX::Zero(2,1);
        D_sec_texpA_true(0,0) = E() * alpha() * area_true;
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_texpA);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_texpA_true);
        
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
    
    SECTION("1D thermal expansion B matrix")
    {
        /** 
         * As of Dec. 17, 2019, thermal_expansion_B_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * thermal_expansion_B_matrix to be obtained without any input arguments.
         */
        
        Real first_area_moment_y;
        const MAST::FieldFunction<Real>& Ay = section.Ay();
        Ay(point, time, first_area_moment_y);
        CHECK( first_area_moment_y == Approx(first_area_moment_y_true) );
        
        Real first_area_moment_z;
        const MAST::FieldFunction<Real>& Az = section.Az();
        Az(point, time, first_area_moment_z);
        CHECK( first_area_moment_z == Approx(first_area_moment_z_true) );
        
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> texp_B_mat = section.thermal_expansion_B_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_texpB;
        texp_B_mat->operator()(point, time, D_sec_texpB);
        
        libMesh::out << "texp_B_mat =\n" << D_sec_texpB << std::endl;
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_texpB_true = RealMatrixX::Zero(2,1);
        D_sec_texpB_true(0,0) = E() * alpha() * first_area_moment_z_true;
        D_sec_texpB_true(1,0) = E() * alpha() * first_area_moment_y_true;
        
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_texpB);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_texpB_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
}


TEST_CASE("rod_element_property_card_constant_dynamic_1d",
          "[dynamic],[1D],[isotropic],[constant],[property]")
{
    const uint dim = 1;
        
    // Define Material Properties as MAST Parameters
    MAST::Parameter rho("rho_param", 1420.5);         // Density
    MAST::Parameter E("E_param", 72.0e9);             // Modulus of Elasticity
    MAST::Parameter nu("nu_param", 0.33);             // Poisson's ratio
    MAST::Parameter cp("cp_param",   908.0);          // Specific Heat Capacity
    MAST::Parameter k("k_param",     237.0);          // Thermal Conductivity
    
    const Real G = E() / (2. * (1. + nu()));
    
    // Define Section Properties as MAST Parameters
    MAST::Parameter DIM1("DIM1", 3.234);   // Section thickness in z-direction
    MAST::Parameter offset_y("offy_param", 0.287);     // Section offset in y-direction
    MAST::Parameter offset_z("offz_param", -1.654);     // Section offset in z-direction
    
    // Create field functions to dsitribute these constant parameters throughout the model
    // Section Property Field Functions
    MAST::ConstantFieldFunction DIM1_f("DIM1", DIM1);
    MAST::ConstantFieldFunction offsety_f("hy_off", offset_y);
    MAST::ConstantFieldFunction offsetz_f("hz_off", offset_z);
    // Material Property Field Functions
    MAST::ConstantFieldFunction rho_f("rho", rho);
    MAST::ConstantFieldFunction E_f("E", E);
    MAST::ConstantFieldFunction nu_f("nu", nu);
    MAST::ConstantFieldFunction cp_f("cp", cp);
    MAST::ConstantFieldFunction k_f("k_th", k);
    
    // Initialize the material
    MAST::IsotropicMaterialPropertyCard material;                   
    
    // Add the material property constant field functions to the material card
    material.add(rho_f);
    material.add(k_f);
    material.add(cp_f);
    material.add(E_f);
    material.add(nu_f);
    
    // Initialize the section
    MAST::Solid1DRodSectionElementPropertyCard section;
    
    // Add the section property constant field functions to the section card
    section.add(DIM1_f);
    section.add(offsety_f);
    section.add(offsetz_f);
    
    // Add the material card to the section card
    section.set_material(material);
    
    // Specify a section orientation point and add it to the section.
    RealVectorX orientation = RealVectorX::Zero(3);
    orientation(1) = 1.0;
    section.y_vector() = orientation;
    
    // Now initialize the section
    section.init(*p_global_init);
    
    REQUIRE( section.dim() == dim); // Ensure section is 1 dimensional
    REQUIRE( section.depends_on(DIM1) );
    REQUIRE( section.depends_on(offset_y) );
    REQUIRE( section.depends_on(offset_z) );
    REQUIRE( section.depends_on(k) );
    REQUIRE( section.depends_on(cp) );
    REQUIRE( section.depends_on(rho) );
    
    REQUIRE( section.if_isotropic() );
    
    // True values
    const Real r = DIM1();
    const Real area_true = PI*r*r;
    const Real torsion_constant_true = PI*pow(r, 4.0)/2.0;
    const Real first_area_moment_z_true = area_true * offset_y();
    const Real first_area_moment_y_true = area_true * offset_z();
    const Real Izzc_true = PI*pow(r, 4.0)/4.0;
    const Real Iyyc_true = PI*pow(r, 4.0)/4.0;
    const Real Izyc_true = 0.0;
    const Real Ipc_true = PI*pow(r, 4.0)/2.0;
    const Real second_area_moment_zz_true = Izzc_true + area_true * pow(offset_y(), 2.);
    const Real second_area_moment_yy_true = Iyyc_true + area_true * pow(offset_z(), 2.);
    const Real second_area_moment_zy_true = Izyc_true + area_true * offset_y() * offset_z();
    const Real second_area_moment_polar_true = second_area_moment_zz_true + second_area_moment_yy_true;
    const Real warping_constant_true = 0.0;
    const Real kappa_z_true = 8.4967018479575718e-01;
    const Real kappa_y_true = 8.4967018474199052e-01;
    const Real xs_true = offset_z();
    const Real ys_true = offset_y();
    const Real xc_true = offset_z();
    const Real yc_true = offset_y();
    const libMesh::Point shear_center_true(xs_true, ys_true, 0.);
    const libMesh::Point centroid_true(xc_true, yc_true, 0.);
    
    const libMesh::Point point(4.3, -3.5, -6.7);
    const Real time = 8.22;
    
    SECTION("1D section inertia matrix")
    {
        /** 
         * As of Dec. 17, 2019, inertia_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * inertia_matrix to be obtained without any input arguments.
         */
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> inertia_mat = section.inertia_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_iner;
        inertia_mat->operator()(point, time, D_sec_iner);
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_iner_true = RealMatrixX::Zero(6,6);
        
        
        Real area;
        const MAST::FieldFunction<Real>& Area = section.A();
        Area(point, time, area);
        REQUIRE( area == Approx(area_true) );
        D_sec_iner_true(0,0) = D_sec_iner_true(1,1) = D_sec_iner_true(2,2) = area_true;
        
        Real Ip;
        const MAST::FieldFunction<Real>& PolarInertia = section.Ip();
        PolarInertia(point, time, Ip);
        CHECK( Ip == Approx(second_area_moment_polar_true) );
        D_sec_iner_true(3,3) = second_area_moment_polar_true;
        
        Real first_area_moment_y;
        const MAST::FieldFunction<Real>& Ay = section.Ay();
        Ay(point, time, first_area_moment_y);
        CHECK( first_area_moment_y == Approx(first_area_moment_y_true) );
        D_sec_iner_true(0,4) = D_sec_iner_true(4,0) = first_area_moment_y_true;
        
        Real first_area_moment_z;
        const MAST::FieldFunction<Real>& Az = section.Az();
        Az(point, time, first_area_moment_z);
        CHECK( first_area_moment_z == Approx(first_area_moment_z_true) );
        D_sec_iner_true(0,5) = D_sec_iner_true(5,0) = first_area_moment_z_true;
        
        RealMatrixX I;
        const MAST::FieldFunction<RealMatrixX>& Inertias = section.I();
        Inertias(point, time, I);
        REQUIRE( I(0,1) == I(1,0) );
        Real Iyy = I(1,1);
        Real Izz = I(0,0);
        Real Izy = I(0,1);
        REQUIRE( Izz == Approx(second_area_moment_zz_true) );
        REQUIRE( Iyy == Approx(second_area_moment_yy_true) );
        REQUIRE( Izy == Approx(second_area_moment_zy_true) );

        D_sec_iner_true(4,4) = Iyy; // TODO: Should this be Izz?
        D_sec_iner_true(4,5) = Izy;
        D_sec_iner_true(5,4) = Izy;
        D_sec_iner_true(5,5) = Izz; // TODO: Should this be Iyy??
        
        D_sec_iner_true *= rho();

        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_iner);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_iner_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
}


TEST_CASE("rod_element_property_card_constant_structural_1d",
          "[structural],[1D],[isotropic],[constant],[property]")
{
    const uint dim = 1;
        
    // Define Material Properties as MAST Parameters
    MAST::Parameter rho("rho_param", 1420.5);         // Density
    MAST::Parameter E("E_param", 72.0e9);             // Modulus of Elasticity
    MAST::Parameter nu("nu_param", 0.33);             // Poisson's ratio
    MAST::Parameter cp("cp_param",   908.0);          // Specific Heat Capacity
    MAST::Parameter k("k_param",     237.0);          // Thermal Conductivity
    
    const Real G = E() / (2. * (1. + nu()));
    
    // Define Section Properties as MAST Parameters
    MAST::Parameter DIM1("DIM1", 3.234);   // Section thickness in z-direction
    MAST::Parameter offset_y("offy_param", 0.287);     // Section offset in y-direction
    MAST::Parameter offset_z("offz_param", -1.654);     // Section offset in z-direction
    
    // Create field functions to dsitribute these constant parameters throughout the model
    // Section Property Field Functions
    MAST::ConstantFieldFunction DIM1_f("DIM1", DIM1);
    MAST::ConstantFieldFunction offsety_f("hy_off", offset_y);
    MAST::ConstantFieldFunction offsetz_f("hz_off", offset_z);
    // Material Property Field Functions
    MAST::ConstantFieldFunction rho_f("rho", rho);
    MAST::ConstantFieldFunction E_f("E", E);
    MAST::ConstantFieldFunction nu_f("nu", nu);
    MAST::ConstantFieldFunction cp_f("cp", cp);
    MAST::ConstantFieldFunction k_f("k_th", k);
    
    // Initialize the material
    MAST::IsotropicMaterialPropertyCard material;                   
    
    // Add the material property constant field functions to the material card
    material.add(rho_f);
    material.add(k_f);
    material.add(cp_f);
    material.add(E_f);
    material.add(nu_f);
    
    // Initialize the section
    MAST::Solid1DRodSectionElementPropertyCard section;
    
    // Add the section property constant field functions to the section card
    section.add(DIM1_f);
    section.add(offsety_f);
    section.add(offsetz_f);
    
    // Add the material card to the section card
    section.set_material(material);
    
    // Specify a section orientation point and add it to the section.
    RealVectorX orientation = RealVectorX::Zero(3);
    orientation(1) = 1.0;
    section.y_vector() = orientation;
    
    // Now initialize the section
    section.init(*p_global_init);
    
    REQUIRE( section.dim() == dim); // Ensure section is 1 dimensional
    REQUIRE( section.depends_on(DIM1) );
    REQUIRE( section.depends_on(offset_y) );
    REQUIRE( section.depends_on(offset_z) );
    REQUIRE( section.depends_on(k) );
    REQUIRE( section.depends_on(cp) );
    REQUIRE( section.depends_on(rho) );
    
    REQUIRE( section.if_isotropic() );
    
    // True values
    const Real r = DIM1();
    const Real area_true = PI*r*r;
    const Real torsion_constant_true = PI*pow(r, 4.0)/2.0;
    const Real first_area_moment_z_true = area_true * offset_y();
    const Real first_area_moment_y_true = area_true * offset_z();
    const Real Izzc_true = PI*pow(r, 4.0)/4.0;
    const Real Iyyc_true = PI*pow(r, 4.0)/4.0;
    const Real Izyc_true = 0.0;
    const Real Ipc_true = PI*pow(r, 4.0)/2.0;
    const Real second_area_moment_zz_true = Izzc_true + area_true * pow(offset_y(), 2.);
    const Real second_area_moment_yy_true = Iyyc_true + area_true * pow(offset_z(), 2.);
    const Real second_area_moment_zy_true = Izyc_true + area_true * offset_y() * offset_z();
    const Real second_area_moment_polar_true = second_area_moment_zz_true + second_area_moment_yy_true;
    const Real warping_constant_true = 0.0;
    const Real kappa_z_true = 8.4967018479575718e-01;
    const Real kappa_y_true = 8.4967018474199052e-01;
    const Real xs_true = offset_z();
    const Real ys_true = offset_y();
    const Real xc_true = offset_z();
    const Real yc_true = offset_y();
    const libMesh::Point shear_center_true(xs_true, ys_true, 0.);
    const libMesh::Point centroid_true(xc_true, yc_true, 0.);
    
    const libMesh::Point point(4.3, -3.5, -6.7);
    const Real time = 8.22;
    
    SECTION("set_get_bending_model")
    {
        // NOTE: MAST::DKT and MAST::MINDLIN are not valid options for 1D sections, even though their input is accepted.
        section.set_bending_model(MAST::BERNOULLI);
        section.set_bending_model(MAST::DEFAULT_BENDING);
        section.set_bending_model(MAST::NO_BENDING);
        section.set_bending_model(MAST::TIMOSHENKO);
        
        // TODO: Implement element creation for testing of getting bending_model and checking default
        //REQUIRE( section.bending_model()
    }
    
//     SECTION("quadrature_order")
//     {
//         section.set_bending_model(MAST::MINDLIN);
//         REQUIRE( section.extra_quadrature_order(elem) == 0 );
//         
//         section.set_bending_model(MAST::DKT);
//         REQUIRE( section.extra_quadrature_order(elem) == 2 );
//     }
    
    SECTION("1D extension stiffness matrix")
    {                
        /** 
         * As of Dec. 17, 2019, stiffness_A_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * stiffness_A_matrix to be obtained without any input arguments.
         */
        Real area;
        const MAST::FieldFunction<Real>& Area = section.A();
        Area(point, time, area);
        REQUIRE( area == Approx(area_true) );
        
        Real torsion_constant;
        const MAST::FieldFunction<Real>& TorsionConstant = section.J();
        TorsionConstant(point, time, torsion_constant);
        REQUIRE( torsion_constant == Approx(torsion_constant_true).epsilon(0.05) );
        
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> extension_stiffness_mat = section.stiffness_A_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_ext;
        extension_stiffness_mat->operator()(point, time, D_sec_ext);
        
        libMesh::out << "D_sec_ext\n" << D_sec_ext << std::endl;
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_ext_true = RealMatrixX::Zero(2,2);
        D_sec_ext_true(0,0) = E() * area_true;
        D_sec_ext_true(1,1) = G   * torsion_constant_true;
        
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_ext);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_ext_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth).epsilon(0.05) );
    }
    
    
    SECTION("1D bending section stiffness matrix")
    {
        /** 
         * As of Dec. 17, 2019, stiffness_D_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * stiffness_D_matrix to be obtained without any input arguments.
         */
        RealMatrixX I;
        const MAST::FieldFunction<RealMatrixX>& Inertias = section.I();
        Inertias(point, time, I);
        REQUIRE( I(0,1) == I(1,0) );
        Real Izz = I(0,0);
        Real Iyy = I(1,1);
        Real Izy = I(0,1);
        REQUIRE( Izz == Approx(second_area_moment_zz_true) );
        REQUIRE( Iyy == Approx(second_area_moment_yy_true) );
        REQUIRE( Izy == Approx(second_area_moment_zy_true) );
        
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> bending_stiffness_mat = section.stiffness_D_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_bnd;
        bending_stiffness_mat->operator()(point, time, D_sec_bnd);
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_bnd_true = RealMatrixX::Zero(2,2);
        D_sec_bnd_true(0,0) = E() * second_area_moment_zz_true;
        D_sec_bnd_true(1,1) = E() * second_area_moment_yy_true;
        D_sec_bnd_true(0,1) = E() * second_area_moment_zy_true;
        D_sec_bnd_true(1,0) = E() * second_area_moment_zy_true;
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_bnd);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_bnd_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
    
    SECTION("1D extension-bending section stiffness matrix")
    {
        /** 
         * As of Dec. 17, 2019, stiffness_B_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * stiffness_B_matrix to be obtained without any input arguments.
         */
        Real first_area_moment_y;
        const MAST::FieldFunction<Real>& Ay = section.Ay();
        Ay(point, time, first_area_moment_y);
        CHECK( first_area_moment_y == Approx(first_area_moment_y_true) );
        
        Real first_area_moment_z;
        const MAST::FieldFunction<Real>& Az = section.Az();
        Az(point, time, first_area_moment_z);
        CHECK( first_area_moment_z == Approx(first_area_moment_z_true) );
        
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> bndext_stiffness_mat = section.stiffness_B_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_bndext;
        bndext_stiffness_mat->operator()(point, time, D_sec_bndext);
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_bndext_true = RealMatrixX::Zero(2,2);
        D_sec_bndext_true(0,0) = E() * first_area_moment_z_true;
        D_sec_bndext_true(0,1) = E() * first_area_moment_y_true;
        // TODO: Add checks for torsion bending coupling when added to MAST.
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_bndext);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_bndext_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
    
    SECTION("1D transverse shear section stiffness matrix")
    {
        /** 
         * As of Dec. 17, 2019, transverse_shear_stiffness_matrix requires the input of an
         * MAST::ElementBase object, but does not actually use it. To get 
         * around requiring the creation of such an object (and therefore 
         * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
         * and MAST::GeomElem objects as well).
         * 
         * To remedy this, an additional method was added to MAST which allows
         * transverse_shear_stiffness_matrix to be obtained without any input arguments.
         */
        Real area;
        const MAST::FieldFunction<Real>& Area = section.A();
        Area(point, time, area);
        REQUIRE( area == Approx(area_true) );
        
        std::unique_ptr<MAST::FieldFunction<RealMatrixX>> trans_shear_stiffness_mat = section.transverse_shear_stiffness_matrix();
        
        const libMesh::Point point(2.3, 3.1, 5.2);
        const Real time = 2.34;
        RealMatrixX D_sec_shr;
        trans_shear_stiffness_mat->operator()(point, time, D_sec_shr);
        
        // Hard-coded value of the section's extension stiffness
        RealMatrixX D_sec_shr_true = RealMatrixX::Zero(2,2);
        D_sec_shr_true(0,0) = G*area_true*kappa_z_true;
        D_sec_shr_true(1,1) = G*area_true*kappa_y_true;
        
        // Convert the test and truth Eigen::Matrix objects to std::vector
        // since Catch2 has built in methods to compare vectors
        std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_shr);
        std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_shr_true);
        
        // Floating point approximations are diffcult to compare since the
        // values typically aren't exactly equal due to numerical error.
        // Therefore, we use the Approx comparison instead of Equals
        CHECK_THAT( test, Catch::Approx<double>(truth) );
    }
    
    
//     SECTION("1D spring section stiffness matrix")
//     {
//         /** 
//          * As of Dec. 17, 2019, spring_stiffness_matrix requires the input of an
//          * MAST::ElementBase object, but does not actually use it. To get 
//          * around requiring the creation of such an object (and therefore 
//          * the creation of a MAST::SystemInitialization, MAST::AssemblyBase,
//          * and MAST::GeomElem objects as well).
//          * 
//          * To remedy this, an additional method was added to MAST which allows
//          * transverse_shear_stiffness_matrix to be obtained without any input arguments.
//          */
//         std::unique_ptr<MAST::FieldFunction<RealMatrixX>> spring_stiffness_mat = section.stiffness_S_matrix();
//         
//         const libMesh::Point point(2.3, 3.1, 5.2);
//         const Real time = 2.34;
//         RealMatrixX D_sec_spring;
//         spring_stiffness_mat->operator()(point, time, D_sec_spring);
//         
//         Hard-coded value of the section's extension stiffness
//         NOTE: Should be all zero's for non-bushing sections
//         RealMatrixX D_sec_spring_true = RealMatrixX::Zero(4,6);
//         
//         Convert the test and truth Eigen::Matrix objects to std::vector
//         since Catch2 has built in methods to compare vectors
//         std::vector<double> test =  eigen_matrix_to_std_vector(D_sec_spring);
//         std::vector<double> truth = eigen_matrix_to_std_vector(D_sec_spring_true);
//         
//         Floating point approximations are diffcult to compare since the
//         values typically aren't exactly equal due to numerical error.
//         Therefore, we use the Approx comparison instead of Equals
//         CHECK_THAT( test, Catch::Approx<double>(truth) );
//     }
}
