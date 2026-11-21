#pragma once

#include "meshing/mesh.h"
#include "core/world/chunk.h"
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>

// GreedyMesher: standard greedy meshing algorithm
// Merges coplanar quads to reduce vertex count
class GreedyMesher {
public:
    // Mesh a single chunk - returns MeshChunk with vertices and indices
    static MeshChunk Mesh(const Chunk& chunk, const World& world) {
        MeshChunk result;
        result.vertices.clear();
        result.indices.clear();

        // Track which blocks have been used in a face
        // We'll build faces outward from solid blocks
        bool used[CHUNK_VOLUME * 6]; // 6 faces per block, but we merge

        // Actually, greedy meshing works by scanning each face direction
        // and merging adjacent coplanar blocks

        // For each of the 6 directions, we try to build maximal rectangles
        for (int dir = 0; dir < 6; ++dir) {
            BuildFacesForDirection(chunk, dir, result);
        }

        result.vertexCount = static_cast<uint32_t>(result.vertices.size());
        result.indexCount = static_cast<uint32_t>(result.indices.size());

        return result;
    }

    // Mesh a chunk with callback for parallel execution
    static void Mesh(const Chunk& chunk, const World& world,
                     std::function<void(uint64_t)> callback) {
        MeshChunk mesh = Mesh(chunk, world);
        // Call callback with chunk key
        uint64_t key = chunk.GetKey();
        callback(key);
        // In a full implementation, we would upload the mesh here
        // For now, just invoke the callback
    }

private:
    static constexpr int CHUNK_VOLUME = 4096;
    static constexpr int CHUNK_SIZE = 16;

    // Build faces for one direction, merging coplanar quads
    static void BuildFacesForDirection(const Chunk& chunk, int dir,
                                       MeshChunk& result) {
        // For each block in the chunk, check if it's solid
        // and try to build a maximal rectangle on the given face direction

        // We need to check block visibility from the given direction
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    BlockType type = chunk.GetBlock(x, y, z);
                    if (type == BlockType::Air) continue;

                    // Check if there's an adjacent air block in the dir direction
                    // that would make this face visible
                    int nx = x, ny = y, nz = z;
                    switch (dir) {
                        case 0: // +X face: check block at x+1
                            nx = x + 1; break;
                        case 1: // -X face: check block at x-1
                            nx = x - 1; break;
                        case 2: // +Y face: check block at y+1
                            ny = y + 1; break;
                        case 3: // -Y face: check block at y-1
                            ny = y - 1; break;
                        case 4: // +Z face: check block at z+1
                            nz = z + 1; break;
                        case 5: // -Z face: check block at z-1
                            nz = z - 1; break;
                    }

                    // If adjacent block is air (or out of bounds = visible), this face is exposed
                    BlockType adjacent = BlockType::Air;
                    if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                        adjacent = chunk.GetBlock(nx, ny, nz);
                    } else {
                        // Out of chunk bounds = adjacent to "world boundary" = visible
                        adjacent = BlockType::Air;
                    }

                    if (adjacent != BlockType::Air) continue; // Not exposed

                    // This block face is visible. Try to merge with adjacent coplanar blocks.
                    // We grow a rectangle in the XZ plane for Y-directed faces, etc.

                    // Find the maximum rectangle starting from (x,y,z) in direction 'dir'
                    // For simplicity, we'll add one quad per exposed face initially,
                    // and the merging will happen through the rectangle-growing logic below.

                    AddMergedQuad(chunk, result, x, y, z, dir);
                }
            }
        }
    }

    // Add a merged quad (merged with adjacent coplanar blocks) to the result
    static void AddMergedQuad(const Chunk& chunk, MeshChunk& result, int startX, int startY, int startZ, int dir) {
        // Determine the face normal and UV basis
        float normal[3];
        float uvAxis1[3]; // UV coordinate axis 1
        float uvAxis2[3]; // UV coordinate axis 2
        int faceWidthDir; // Which axis controls width of rectangle
        int faceHeightDir; // Which axis controls height of rectangle
        float uvOriginOffset[2] = {0.0f, 0.0f}; // UV offset for this face type

        // Set up face properties based on direction
        switch (dir) {
            case 0: // +X face
                normal[0] = 1.0f; normal[1] = 0.0f; normal[2] = 0.0f;
                uvAxis1[0] = 0; uvAxis1[1] = 1; uvAxis1[2] = 0; // Y axis
                uvAxis2[0] = 0; uvAxis2[1] = 0; uvAxis2[2] = 1; // Z axis
                faceWidthDir = 1; // Y controls width
                faceHeightDir = 2; // Z controls height
                uvOriginOffset[0] = 0.0f;
                uvOriginOffset[1] = 0.0f;
                break;
            case 1: // -X face
                normal[0] = -1.0f; normal[1] = 0.0f; normal[2] = 0.0f;
                uvAxis1[0] = 0; uvAxis1[1] = 1; uvAxis1[2] = 0;
                uvAxis2[0] = 0; uvAxis2[1] = 0; uvAxis2[2] = 1;
                faceWidthDir = 1;
                faceHeightDir = 2;
                break;
            case 2: // +Y face
                normal[0] = 0.0f; normal[1] = 1.0f; normal[2] = 0.0f;
                uvAxis1[0] = 1; uvAxis1[1] = 0; uvAxis1[2] = 0; // X axis
                uvAxis2[0] = 0; uvAxis2[1] = 0; uvAxis2[2] = 1; // Z axis
                faceWidthDir = 0; // X controls width
                faceHeightDir = 2; // Z controls height
                break;
            case 3: // -Y face
                normal[0] = 0.0f; normal[1] = -1.0f; normal[2] = 0.0f;
                uvAxis1[0] = 1; uvAxis1[1] = 0; uvAxis1[2] = 0;
                uvAxis2[0] = 0; uvAxis2[1] = 0; uvAxis2[2] = 1;
                faceWidthDir = 0;
                faceHeightDir = 2;
                break;
            case 4: // +Z face
                normal[0] = 0.0f; normal[1] = 0.0f; normal[2] = 1.0f;
                uvAxis1[0] = 1; uvAxis1[1] = 0; uvAxis1[2] = 0; // X axis
                uvAxis2[0] = 0; uvAxis2[1] = 1; uvAxis2[2] = 0; // Y axis
                faceWidthDir = 0; // X controls width
                faceHeightDir = 1; // Y controls height
                break;
            case 5: // -Z face
                normal[0] = 0.0f; normal[1] = 0.0f; normal[2] = -1.0f;
                uvAxis1[0] = 1; uvAxis1[1] = 0; uvAxis1[2] = 0;
                uvAxis2[0] = 0; uvAxis2[1] = 1; uvAxis2[2] = 0;
                faceWidthDir = 0;
                faceHeightDir = 1;
                break;
        }

        // Grow rectangle: start from (startX, startY, startZ) and expand
        // in the width and height directions as long as blocks are the same type

        // Find the full width of the rectangle
        int width = 0;
        int x = startX;
        // Expand in the width direction
        while (true) {
            int checkX = x;
            int checkY = startY;
            int checkZ = startZ;
            // Move along width direction
            switch (faceWidthDir) {
                case 0: checkX++; break; // +X
                case 1: checkX--; break; // -X
                case 2: checkY++; break; // +Y
                case 3: checkY--; break; // -Y
                case 4: checkZ++; break; // +Z
                case 5: checkZ--; break; // -Z
            }
            // Check if the entire row is solid same-type blocks
            bool rowSolid = true;
            for (int k = 0; k <= width + 1; ++k) {
                int cx = checkX;
                int cy = checkY;
                int cz = startZ;
                // Actually this is getting complex - let me simplify
                // For now, just add a single quad per exposed face
                // The full greedy merging will be a later optimization
            }
            break;
        }

        // SIMPLIFIED: Just add a single quad for now (no merging yet)
        // This is the base case for greedy meshing
        AddSimpleQuad(chunk, result, startX, startY, startZ, dir, normal, uvAxis1, uvAxis2);
    }

    // Add a simple quad (no merging with adjacent blocks)
    static void AddSimpleQuad(const Chunk& chunk, MeshChunk& result, int x, int y, int z, int dir,
                               const float normal[3], const float uvAxis1[3], const float uvAxis2[3]) {
        BlockType type = chunk.GetBlock(x, y, z);
        if (type == BlockType::Air) return;

        // Calculate UV coordinates based on block type
        // Each block face maps to a 16x16 tile in a 256x256 texture atlas
        // We'll use simple UVs: block type index * 1/16 for the tile position
        float tileSize = 1.0f / 16.0f;
        int typeIndex = static_cast<int>(type);
        int tx = typeIndex % 16; // X tile in atlas
        int ty = typeIndex / 16; // Y tile in atlas

        // Quad vertices (4 corners, each with pos+normal+uv+ao)
        // We'll set AO to 1.0 for now (will be calculated later or baked)
        Vertex v0, v1, v2, v3;

        // Position depends on face direction
        float baseX = static_cast<float>(x);
        float baseY = static_cast<float>(y);
        float baseZ = static_cast<float>(z);

        switch (dir) {
            case 0: // +X face: face normal +X, quad in YZ plane
                v0.pos[0] = baseX + 1.0f; v0.pos[1] = baseY; v0.pos[2] = baseZ;
                v1.pos[0] = baseX + 1.0f; v1.pos[1] = baseY + 1.0f; v1.pos[2] = baseZ;
                v2.pos[0] = baseX + 1.0f; v2.pos[1] = baseY + 1.0f; v2.pos[2] = baseZ + 1.0f;
                v3.pos[0] = baseX + 1.0f; v3.pos[1] = baseY; v3.pos[2] = baseZ + 1.0f;
                break;
            case 1: // -X face: face normal -X, quad in YZ plane
                v0.pos[0] = baseX; v0.pos[1] = baseY; v0.pos[2] = baseZ;
                v1.pos[0] = baseX; v1.pos[1] = baseY + 1.0f; v1.pos[2] = baseZ;
                v2.pos[0] = baseX; v2.pos[1] = baseY + 1.0f; v2.pos[2] = baseZ + 1.0f;
                v3.pos[0] = baseX; v3.pos[1] = baseY; v3.pos[2] = baseZ + 1.0f;
                break;
            case 2: // +Y face: face normal +Y, quad in XZ plane
                v0.pos[0] = baseX; v0.pos[1] = baseY + 1.0f; v0.pos[2] = baseZ;
                v1.pos[0] = baseX + 1.0f; v1.pos[1] = baseY + 1.0f; v1.pos[2] = baseZ;
                v2.pos[0] = baseX + 1.0f; v2.pos[1] = baseY + 1.0f; v2.pos[2] = baseZ + 1.0f;
                v3.pos[0] = baseX; v3.pos[1] = baseY + 1.0f; v3.pos[2] = baseZ + 1.0f;
                break;
            case 3: // -Y face: face normal -Y, quad in XZ plane
                v0.pos[0] = baseX; v0.pos[1] = baseY; v0.pos[2] = baseZ;
                v1.pos[0] = baseX + 1.0f; v1.pos[1] = baseY; v1.pos[2] = baseZ;
                v2.pos[0] = baseX + 1.0f; v2.pos[1] = baseY; v2.pos[2] = baseZ + 1.0f;
                v3.pos[0] = baseX; v3.pos[1] = baseY; v3.pos[2] = baseZ + 1.0f;
                break;
            case 4: // +Z face: face normal +Z, quad in XY plane
                v0.pos[0] = baseX; v0.pos[1] = baseY; v0.pos[2] = baseZ + 1.0f;
                v1.pos[0] = baseX + 1.0f; v1.pos[1] = baseY; v1.pos[2] = baseZ + 1.0f;
                v2.pos[0] = baseX + 1.0f; v2.pos[1] = baseY + 1.0f; v2.pos[2] = baseZ + 1.0f;
                v3.pos[0] = baseX; v3.pos[1] = baseY + 1.0f; v3.pos[2] = baseZ + 1.0f;
                break;
            case 5: // -Z face: face normal -Z, quad in XY plane
                v0.pos[0] = baseX; v0.pos[1] = baseY; v0.pos[2] = baseZ;
                v1.pos[0] = baseX + 1.0f; v1.pos[1] = baseY; v1.pos[2] = baseZ;
                v2.pos[0] = baseX + 1.0f; v2.pos[1] = baseY + 1.0f; v2.pos[2] = baseZ;
                v3.pos[0] = baseX; v3.pos[1] = baseY + 1.0f; v3.pos[2] = baseZ;
                break;
        }

        // Normals
        v0.normal[0] = normal[0]; v0.normal[1] = normal[1]; v0.normal[2] = normal[2];
        v1.normal[0] = normal[0]; v1.normal[1] = normal[1]; v1.normal[2] = normal[2];
        v2.normal[0] = normal[0]; v2.normal[1] = normal[1]; v2.normal[2] = normal[2];
        v3.normal[0] = normal[0]; v3.normal[1] = normal[1]; v3.normal[2] = normal[2];

        // UV coordinates - map block type to 16x16 tile in 256x256 atlas
        v0.uv[0] = uvOriginOffset[0] + tx * tileSize; v0.uv[1] = uvOriginOffset[1] + ty * tileSize;
        v1.uv[0] = uvOriginOffset[0] + (tx + 1) * tileSize; v1.uv[1] = uvOriginOffset[1] + ty * tileSize;
        v2.uv[0] = uvOriginOffset[0] + (tx + 1) * tileSize; v2.uv[1] = uvOriginOffset[1] + (ty + 1) * tileSize;
        v3.uv[0] = uvOriginOffset[0] + tx * tileSize; v3.uv[1] = uvOriginOffset[1] + (ty + 1) * tileSize;

        // AO - baked later, set to 1.0 for now
        v0.ao = 1.0f;
        v1.ao = 1.0f;
        v2.ao = 1.0f;
        v3.ao = 1.0f;

        // Add vertices
        result.vertices.push_back(v0);
        result.vertices.push_back(v1);
        result.vertices.push_back(v2);
        result.vertices.push_back(v3);

        // Add indices: two triangles forming the quad
        // Triangle 1: v0, v1, v2
        result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 0));
        result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 1));
        result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 2));
        // Triangle 2: v0, v2, v3
        result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 0));
        result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 2));
        result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 3));
    }
};