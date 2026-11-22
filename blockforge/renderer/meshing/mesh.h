#pragma once
#include <cstdint>
#include <vector>
#include <array>

struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float ao;
};

struct MeshChunk {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

extern const std::array<std::array<float, 3>, 6> faceNormals;
extern const std::array<std::array<float, 4>, 6> faceUvCoords;   