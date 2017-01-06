#pragma once

#include <iostream>

#include <opencv2/core.hpp>

#include "core/vc_defines.h"

namespace volcart
{
namespace meshing
{
/**
 * @class CalculateNormals
 * @author Hannah Hatch
 * @date 7/26/16
 *
 * @brief Calculate vertex normals for ITK Meshes.
 *
 * Given an ITK mesh, generates a copy of that mesh with embedded vertex
 * normals.
 *
 * @ingroup Meshing
 */
class CalculateNormals
{
public:
    //** @name Constructors */
    //@{
    CalculateNormals();

    /**
     * @param mesh Input Mesh whose normals you want computed
     */
    CalculateNormals(ITKMesh::Pointer mesh);
    //@}

    //** @name Input/Output */
    //@{
    /**
     * @brief Set the input mesh.
     * @param mesh The mesh for which normals will be calculated
     */
    void setMesh(ITKMesh::Pointer mesh);

    /**
     * @brief Get the output mesh with computed normals
     */
    ITKMesh::Pointer getMesh() const;
    //@}

    /**
     * @brief Compute vertex normals for the mesh.
     */
    void compute();

private:
    /**
     * @brief Compute normals for each vertex.
     *
     * For each face, computes the normal to that face and adds the resulting
     * vector to a sum vector for each vertex in that face.
     */
    void _computeNormals();

    /**
     * @brief Assign the summed normals to the output mesh.
     *
     * Takes the summed normals for each vertex and assigns them to the
     * corresponding vertex in the output mesh.
     */
    void _assignToMesh();

    /** Mesh for which normals will be calculated. */
    ITKMesh::Pointer _input;

    /** Mesh with calculated normals. */
    ITKMesh::Pointer _output;

    /** Storage for summed normals, organized by vertex ID */
    std::vector<cv::Vec3d> _vertex_normals;
};
}  // meshing
}  // volcart
