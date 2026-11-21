#pragma once

#include "core/world/chunk.h"
#include <cstdint>
#include <array>
#include <vector>

// Vertex format for meshing: position, normal, uv, ao
struct Vertex {
    float pos[3];   // 3 floats for position
    float normal[3]; // 3 floats for normal
    float uv[2];     // 2 floats for UV
    float ao;        // 1 float for ambient occlusion
};

// MeshChunk result from greedy meshing
struct MeshChunk {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Face normal directions for 6 faces of a cube
extern const std::array<std::array<float, 3>, 6> faceNormals;
extern const std::array<std::array<float, 2>, 4> faceUvCoords[6];