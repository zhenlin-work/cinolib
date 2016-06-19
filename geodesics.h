/****************************************************************************
* Italian National Research Council                                         *
* Institute for Applied Mathematics and Information Technologies, Genoa     *
* IMATI-GE / CNR                                                            *
*                                                                           *
* Author: Marco Livesu (marco.livesu@gmail.com)                             *
*                                                                           *
* Copyright(C) 2016                                                         *
* All rights reserved.                                                      *
*                                                                           *
* This file is part of CinoLib                                              *
*                                                                           *
* CinoLib is free software; you can redistribute it and/or modify           *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 3 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
****************************************************************************/
#ifndef GEODESICS_H
#define GEODESICS_H

#include <vector>

#include "cinolib.h"
#include "scalar_field.h"
#include "vector_field.h"
#include "gradient.h"
#include "laplacian.h"
#include "vertex_mass.h"
#include "linear_solvers.h"
#include "timer.h"

namespace cinolib
{

/* Compute approximated geodesics as explained in
 *
 * Geodesics in Heat: A New Approach to Computing Distance Based on Heat Flow
 * KEENAN CRANE, CLARISSE WEISCHEDEL and MAX WARDETZKY
 * ACM Transactions on Graphics, 2013
 *
 * First step: solve the heat flow problem with some boundary condition u0
 * (the point(s) from which you want to compute the geodesic distances)
 *
 *                  (M - t * L) u = u0
 *
 * Second step: solve a Poisson problem to determine the function phy whose
 * divergence coincides with the normalized gradient of the heat flow
 *
 *              L phy = div( grad(u)/|grad(u)| )
 *
 * which can also be written as
 *
 *              L phy = grad^T * ( grad(u)/|grad(u)| )
 *
 * phy is the scalar field encoding the geodesic distances.
*/

template<class Mesh>
CINO_INLINE
void compute_geodesics(const Mesh             & m,
                       const std::vector<int> & heat_charges,
                             ScalarField      & geodesics,
                       const int                laplacian_mode = COTANGENT,
                       const float              time_scalar = 1.0)
{
    timer_start("Compute geodesics");

    // use h^2 as time step, as suggested in the original paper
    Mesh m_copy = m;
    m_copy.normalize_area();
    m_copy.center_bbox();
    double time = m_copy.avg_edge_length();
    time *= time;
    time *= time_scalar;

    Eigen::SparseMatrix<double> L   = laplacian<Mesh>(m_copy, laplacian_mode);
    Eigen::SparseMatrix<double> M   = mass_matrix<Mesh>(m_copy);
    Eigen::SparseMatrix<double> G   = gradient(m_copy);
    Eigen::VectorXd             rhs = Eigen::VectorXd::Zero(m_copy.num_vertices());

    std::map<int,double> bc;
    for(int i=0; i<(int)heat_charges.size(); ++i)
    {
        bc[heat_charges[i]] = 1.0;
    }

    ScalarField heat(m_copy.num_vertices());
    solve_square_system_with_bc(M - time * L, rhs, heat, bc);

    VectorField grad = G * heat;
    grad.normalize();

    solve_square_system_with_bc(-L, G.transpose() * grad, geodesics, bc);

    geodesics.normalize_in_01();

    timer_stop("Compute geodesics");
}

}

#endif // GEODESICS_H