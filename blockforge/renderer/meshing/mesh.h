#pragma once

#include <cstdint>
#include <vector>

// Vertex format: position, normal, uv, ao
struct Vertex {
    float pos[3];     // 3 floats for position
    float normal[3];  // 3 floats for normal
    float uv[2];      // 2 floats for UV
    float ao;         // 1 float for ambient occlusion
};

// MeshChunk result from greedy meshing
struct MeshChunk {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Face normal directions for 6 faces of a cube
// Order: +X, -X, +Y, -Y, +Z, -Z
extern const std::array<std::array<float, 3>, 6> faceNormals;

// Face UV coordinate mappings for each of the 6 faces
// Each face has 4 UV coords corresponding to the quad vertices
// UV layout maps a single block face to a 16x16 tile in a 256x256 texture atlas
extern const std::array<std::array<float, 4>, 6> faceUvCoords[6];