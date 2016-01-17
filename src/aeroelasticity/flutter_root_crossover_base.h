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

#ifndef __mast__flutter_root_crossover_base_h__
#define __mast__flutter_root_crossover_base_h__

// C++ includes
#include <ostream>



namespace MAST {
    
    // Forward declerations
    class FlutterSolutionBase;
    class TimeDomainFlutterRootBase;
    
    
    class FlutterRootCrossoverBase {
        
    public:
        FlutterRootCrossoverBase():
        crossover_solutions(NULL, NULL),
        root_num(0),
        root(NULL)
        { }
        
        void print(std::ostream& output) const;
        
        std::pair<MAST::FlutterSolutionBase*, MAST::FlutterSolutionBase*>
        crossover_solutions;
        
        unsigned int root_num;
        
        MAST::TimeDomainFlutterRootBase* root;
    };
    
}

#endif // __mast__flutter_root_crossover_base_h__