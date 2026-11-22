#pragma once

#include "meshing/mesh.h"
#include "core/world/chunk.h"
#include <cstdint>
#include <array>
#include <functional>
#include <mutex>

// GreedyMesher: greedy meshing algorithm
// Merges coplanar quads to reduce vertex count
// Implements: 6-face scan, merge coplanar quads, baked AO (4-tap), UV atlas mapping
class GreedyMesher {
public:
    // Mesh a single chunk - returns MeshChunk with vertices and indices
    // Implements greedy meshing: scans all 6 directions, merges adjacent coplanar blocks,
    // calculates baked AO using 4-tap sampling, and maps UVs to 256x256 atlas (16x16 tiles)
    static MeshChunk Mesh(const Chunk& chunk);

    // Mesh a chunk with callback (for potential parallelization)
    static void Mesh(const Chunk& chunk, const World& world,
                     std::function<void(uint64_t)> callback);

private:
    static constexpr int CHUNK_SIZE = 16;
    static constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 4096

    // Add a quad to the mesh result with AO calculation
    static void AddQuad(MeshChunk& result, int x, int y, int z, int dir,
                        const Chunk& chunk, const std::array<float, 16>& aoTable);

    // Calculate ambient occlusion for a face using 4-tap sampling
    // Sample the 4 neighboring faces (up to 4 adjacent faces in the hemisphere)
    static float CalculateAO(int x, int y, int z, int dir, const Chunk& chunk);

    // Grow a maximal rectangle in the given direction from the starting block
    static void GrowRectangle(const Chunk& chunk, int startX, int startY, int startZ,
                              int dir, int& outWidth, int& outHeight,
                              std::array<std::array<bool, 16>, 16>& seen);

    // Get the block type at local chunk coordinates
    static BlockType GetBlockType(const Chunk& chunk, int x, int y, int z);
};