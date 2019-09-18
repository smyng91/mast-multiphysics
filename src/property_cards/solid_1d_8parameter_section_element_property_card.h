/*
 * MAST: Multidisciplinary-design Adaptation and Sensitivity Toolkit
 * Copyright (C) 2013-2019  Manav Bhatia
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

#ifndef __mast__solid_1d_8parameter_section_element_property_card__
#define __mast__solid_1d_8parameter_section_element_property_card__


// MAST includes
#include "property_cards/solid_1d_section_element_property_card.h"
#include "base/field_function_base.h"


namespace MAST {
    namespace Solid1D8ParameterSectionProperty {
        
        class Area: public MAST::FieldFunction<Real> {
        public:
            Area(const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                          Real&, Real&, Real&, Real&)> func,
                 const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                          Real&, Real&, Real&, Real&, Real&, 
                                          Real&, Real&, Real&, Real&, Real&,
                                          Real&, Real&)> dfunc,
                 const MAST::FieldFunction<Real>& DIM1,
                 const MAST::FieldFunction<Real>& DIM2,
                 const MAST::FieldFunction<Real>& DIM3,
                 const MAST::FieldFunction<Real>& DIM4,
                 const MAST::FieldFunction<Real>& DIM5,
                 const MAST::FieldFunction<Real>& DIM6,
                 const MAST::FieldFunction<Real>& DIM7,
                 const MAST::FieldFunction<Real>& DIM8);
            
            virtual ~Area() { }
            
            virtual void operator() (const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
            virtual void derivative (const MAST::FunctionBase& f,
                                     const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
        protected:
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&)> _func;
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&,
                                     Real&, Real&)> _dfunc;
            const MAST::FieldFunction<Real> &_DIM1, &_DIM2, &_DIM3, &_DIM4,
                                            &_DIM5, &_DIM6, &_DIM7, &_DIM8;
        };
        
        
        class TorsionalConstant: public MAST::FieldFunction<Real> {
        public:
            TorsionalConstant(
                 const std::function<void(Real&, Real&, Real&,Real&, Real&, 
                                          Real&, Real&, Real&, Real&)> func,
                 const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                          Real&, Real&, Real&, Real&, Real&, 
                                          Real&, Real&, Real&, Real&, Real&,
                                          Real&, Real&)> dfunc,
                 const MAST::FieldFunction<Real>& DIM1,
                 const MAST::FieldFunction<Real>& DIM2,
                 const MAST::FieldFunction<Real>& DIM3,
                 const MAST::FieldFunction<Real>& DIM4,
                 const MAST::FieldFunction<Real>& DIM5,
                 const MAST::FieldFunction<Real>& DIM6,
                 const MAST::FieldFunction<Real>& DIM7,
                 const MAST::FieldFunction<Real>& DIM8);
            
            virtual ~TorsionalConstant() { }
            
            virtual void operator() (const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
            virtual void derivative (    const MAST::FunctionBase& f,
                                     const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
        protected:
            
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&)> _func;
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&,
                                     Real&, Real&)> _dfunc;
            const MAST::FieldFunction<Real> &_DIM1, &_DIM2, &_DIM3, &_DIM4,
                                            &_DIM5, &_DIM6, &_DIM7, &_DIM8;
        };
        
        
        class PolarInertia: public MAST::FieldFunction<Real> {
        public:
            PolarInertia(const std::function<void(Real&, Real&, Real&,Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> func,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> dfunc,
                         const std::function<void(Real&, Real&, Real&,Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> Afunc,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> dAfunc,
                         const MAST::FieldFunction<Real>& DIM1,
                         const MAST::FieldFunction<Real>& DIM2,
                         const MAST::FieldFunction<Real>& DIM3,
                         const MAST::FieldFunction<Real>& DIM4,
                         const MAST::FieldFunction<Real>& DIM5,
                         const MAST::FieldFunction<Real>& DIM6,
                         const MAST::FieldFunction<Real>& DIM7,
                         const MAST::FieldFunction<Real>& DIM8,
                         const MAST::FieldFunction<Real>&  hy_offset,
                         const MAST::FieldFunction<Real>&  hz_offset);
            
            virtual ~PolarInertia() { }
            
            virtual void operator() (const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
            virtual void derivative (    const MAST::FunctionBase& f,
                                     const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
        protected:
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&)> _func, _Afunc;
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&,
                                     Real&, Real&)> _dfunc, _dAfunc;
            const MAST::FieldFunction<Real> &_DIM1, &_DIM2, &_DIM3, &_DIM4,
                                            &_DIM5, &_DIM6, &_DIM7, &_DIM8, 
                                            &_hy_offset, &_hz_offset;
        };
        
        
        /*!
         *   calculates the area moment about the Y-axis due to an offset 
         *   along the Z-axis
         */
        class AreaYMoment: public MAST::FieldFunction<Real> {
        public:
            AreaYMoment(const std::function<void(Real&, Real&, Real&,Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> Afunc,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> dAfunc,
                        const MAST::FieldFunction<Real>& DIM1,
                        const MAST::FieldFunction<Real>& DIM2,
                        const MAST::FieldFunction<Real>& DIM3,
                        const MAST::FieldFunction<Real>& DIM4,
                        const MAST::FieldFunction<Real>& DIM5,
                        const MAST::FieldFunction<Real>& DIM6,
                        const MAST::FieldFunction<Real>& DIM7,
                        const MAST::FieldFunction<Real>& DIM8,
                        const MAST::FieldFunction<Real>&  hz_offset);
            
            
            virtual ~AreaYMoment() { }
            
            virtual void operator() (const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
            
            virtual void derivative (    const MAST::FunctionBase& f,
                                     const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
        protected:
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&)> _Afunc;
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&,
                                     Real&, Real&)> _dAfunc;
            const MAST::FieldFunction<Real> &_DIM1, &_DIM2, &_DIM3, &_DIM4,
                                            &_DIM5, &_DIM6, &_DIM7, &_DIM8, 
                                            &_hz_offset;
        };

        
        /*!
         *   calculates the area moment about the Z-axis due to an offset
         *   along the Y-axis
         */
        class AreaZMoment: public MAST::FieldFunction<Real> {
        public:
            AreaZMoment(const std::function<void(Real&, Real&, Real&,Real&, 
                                                 Real&, Real&, Real&, Real&, 
                                                 Real&)> Afunc,
                        const std::function<void(Real&, Real&, Real&, Real&, 
                                                 Real&, Real&, Real&, Real&, 
                                                 Real&, Real&, Real&, Real&, 
                                                 Real&, Real&, Real&, Real&, 
                                                 Real&)> dAfunc,
                        const MAST::FieldFunction<Real>& DIM1,
                        const MAST::FieldFunction<Real>& DIM2,
                        const MAST::FieldFunction<Real>& DIM3,
                        const MAST::FieldFunction<Real>& DIM4,
                        const MAST::FieldFunction<Real>& DIM5,
                        const MAST::FieldFunction<Real>& DIM6,
                        const MAST::FieldFunction<Real>& DIM7,
                        const MAST::FieldFunction<Real>& DIM8,
                        const MAST::FieldFunction<Real>&  hy_offset);
            
            virtual ~AreaZMoment() { }
            
            virtual void operator() (const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
            
            virtual void derivative (    const MAST::FunctionBase& f,
                                     const libMesh::Point& p,
                                     const Real t,
                                     Real& m) const;
            
        protected:
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&)> _Afunc;
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&,
                                     Real&, Real&)> _dAfunc;
            const MAST::FieldFunction<Real> &_DIM1, &_DIM2, &_DIM3, &_DIM4,
                                            &_DIM5, &_DIM6, &_DIM7, &_DIM8, 
                                            &_hy_offset;
        };
        
        
        /*!
         *   calculates the 2x2 matrix of area inertia for the section with 
         *   individual entries as 
         *
         *   0 x 0 = int_omega  (y+yoff)^2 dy dz
         *   0 x 1 = int_omega  (y+yoff) (z+zoff) dy dz
         *   1 x 0 = int_omega  (y+yoff) (z+zoff) dy dz
         *   1 x 1 = int_omega  (z+zoff)^2 dy dz
         */
        class AreaInertiaMatrix: public MAST::FieldFunction<RealMatrixX> {
        public:
            AreaInertiaMatrix(const std::function<void(Real&, Real&,Real&,Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> Izfunc,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> dIzfunc,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> Iyfunc,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> dIyfunc,
                          const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> Afunc,
                         const std::function<void(Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&, Real&, Real&, Real&, 
                                                  Real&)> dAfunc,
                          const MAST::FieldFunction<Real>& DIM1,
                          const MAST::FieldFunction<Real>& DIM2,
                          const MAST::FieldFunction<Real>& DIM3,
                          const MAST::FieldFunction<Real>& DIM4,
                          const MAST::FieldFunction<Real>& DIM5,
                          const MAST::FieldFunction<Real>& DIM6,
                          const MAST::FieldFunction<Real>& DIM7,
                          const MAST::FieldFunction<Real>& DIM8,
                          const MAST::FieldFunction<Real>&  hy_offset,
                          const MAST::FieldFunction<Real>&  hz_offset);
            
            virtual ~AreaInertiaMatrix() { }
            
            virtual void operator() (const libMesh::Point& p,
                                     const Real t,
                                     RealMatrixX& m) const;
            
            
            virtual void derivative (    const MAST::FunctionBase& f,
                                     const libMesh::Point& p,
                                     const Real t,
                                     RealMatrixX& m) const;
            
        protected:
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&)> _Izfunc, 
                                     _Iyfunc, _Afunc;
            const std::function<void(Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&, 
                                     Real&, Real&, Real&, Real&, Real&,
                                     Real&, Real&)> _dIzfunc, _dIyfunc, _dAfunc;
            const MAST::FieldFunction<Real> &_DIM1, &_DIM2, &_DIM3, &_DIM4,
                                            &_DIM5, &_DIM6, &_DIM7, &_DIM8, 
                                            &_hy_offset, &_hz_offset;
        };
    }
}


namespace MAST {
    
    class Solid1D8ParameterSectionElementPropertyCard : public MAST::Solid1DSectionElementPropertyCard
    {
    public:
        Solid1D8ParameterSectionElementPropertyCard():
        MAST::Solid1DSectionElementPropertyCard() {}
        
        /*!
         *   virtual destructor
         */
        virtual ~Solid1D8ParameterSectionElementPropertyCard() { }
    };
}


#endif // __mast__solid_1d_8parameter_section_element_property_card__
