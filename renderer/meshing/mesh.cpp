#include "mesh.h"
#include <array>

// Face normal directions for 6 faces of a cube
// Order: +X, -X, +Y, -Y, +Z, -Z
const std::array<std::array<float, 3>, 6> faceNormals = {{
    {1.0f, 0.0f, 0.0f},   // +X face
    {-1.0f, 0.0f, 0.0f},  // -X face
    {0.0f, 1.0f, 0.0f},   // +Y face
    {0.0f, -1.0f, 0.0f},  // -Y face
    {0.0f, 0.0f, 1.0f},   // +Z face
    {0.0f, 0.0f, -1.0f}   // -Z face
}};

// Face UV coordinates for each of the 6 faces
// Each face has 4 UV coords corresponding to the quad vertices
// UV layout maps a single block face to a 16x16 tile in a 256x256 texture atlas
const std::array<std::array<float, 4>, 6> faceUvCoords[6] = {{
    // +X face: UVs for quad in YZ plane (bottom-left, top-left, top-right, bottom-right)
    {0.0f, 0.0f, 0.0f, 1.0f},
    // -X face: UVs for quad in YZ plane
    {0.0f, 0.0f, 0.0f, 1.0f},
    // +Y face: UVs for quad in XZ plane
    {0.0f, 0.0f, 1.0f, 0.0f},
    // -Y face: UVs for quad in XZ plane
    {0.0f, 0.0f, 1.0f, 0.0f},
    // +Z face: UVs for quad in XY plane
    {0.0f, 0.0f, 0.0f, 1.0f},
    // -Z face: UVs for quad in XY plane
    {0.0f, 0.0f, 0.0f, 1.0f}
}};