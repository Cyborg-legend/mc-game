#include "greedy_mesher.h"
#include "meshing/mesh.h"
#include "core/world/chunk.h"
#include <array>
#include <vector>
#include <cstdint>

// ============================================================
// GreedyMesher::Mesh - Main entry point
// Meshes a single chunk using greedy meshing algorithm
// Returns a MeshChunk containing vertices and indices
// ============================================================
MeshChunk GreedyMesher::Mesh(const Chunk& chunk) {
    MeshChunk result;

    // Visibility tracking: for each block position, track which faces are exposed
    // We use a flat array: index = (z * 16 + y) * 16 + x, face 0-5
    // Faces: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    std::vector<char> exposed(CHUNK_VOLUME * 6, 0);

    // First pass: determine which faces of each solid block are exposed
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                BlockType type = chunk.GetBlock(x, y, z);
                if (type == BlockType::Air) continue;

                int idx = ((z * CHUNK_SIZE) + y) * CHUNK_SIZE + x;

                // Check +X face: block at x+1 must be air or out of bounds
                if (x + 1 >= CHUNK_SIZE || chunk.GetBlock(x + 1, y, z) == BlockType::Air) {
                    exposed[idx * 6 + 0] = 1; // +X face exposed
                }

                // Check -X face: block at x-1 must be air or out of bounds
                if (x - 1 < 0 || chunk.GetBlock(x - 1, y, z) == BlockType::Air) {
                    exposed[idx * 6 + 1] = 1; // -X face exposed
                }

                // Check +Y face: block at y+1 must be air or out of bounds
                if (y + 1 >= CHUNK_SIZE || chunk.GetBlock(x, y + 1, z) == BlockType::Air) {
                    exposed[idx * 6 + 2] = 1; // +Y face exposed
                }

                // Check -Y face: block at y-1 must be air or out of bounds
                if (y - 1 < 0 || chunk.GetBlock(x, y - 1, z) == BlockType::Air) {
                    exposed[idx * 6 + 3] = 1; // -Y face exposed
                }

                // Check +Z face: block at z+1 must be air or out of bounds
                if (z + 1 >= CHUNK_SIZE || chunk.GetBlock(x, y, z + 1) == BlockType::Air) {
                    exposed[idx * 6 + 4] = 1; // +Z face exposed
                }

                // Check -Z face: block at z-1 must be air or out of bounds
                if (z - 1 < 0 || chunk.GetBlock(x, y, z - 1) == BlockType::Air) {
                    exposed[idx * 6 + 5] = 1; // -Z face exposed
                }
            }
        }
    }

    // Second pass: greedy meshing - merge coplanar faces
    // Track which blocks have been consumed (assigned to a face)
    std::vector<char> consumed(CHUNK_VOLUME, 0);

    // Process each of the 6 face directions
    for (int dir = 0; dir < 6; ++dir) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    int idx = ((z * CHUNK_SIZE) + y) * CHUNK_SIZE + x;

                    // Skip if block is air or already consumed
                    if (chunk.GetBlock(x, y, z) == BlockType::Air) continue;
                    if (consumed[idx]) continue;

                    // Skip if this face direction is not exposed
                    if (!exposed[idx * 6 + dir]) continue;

                    // This block face is visible. Try to build a maximal rectangle.
                    // Determine rectangle dimensions
                    int width = 0, height = 0;

                    // Starting position
                    int startX = x, startY = y, startZ = z;

                    // Grow width along the appropriate axis for this face direction
                    int wx = startX, wy = startY, wz = startZ;
                    bool canGrowWidth = true;
                    while (canGrowWidth) {
                        // Check next block in width direction based on face direction
                        int checkX = wx, checkY = wy, checkZ = wz;
                        switch (dir) {
                        case 0: // +X face: width along Y axis
                            checkY++; break;
                        case 1: // -X face: width along Y axis
                            checkY++; break;
                        case 2: // +Y face: width along X axis
                            checkX++; break;
                        case 3: // -Y face: width along X axis
                            checkX++; break;
                        case 4: // +Z face: width along X axis
                            checkX++; break;
                        case 5: // -Z face: width along X axis
                            checkX++; break;
                        }

                        // Check if the block at (checkX, checkY, checkZ) is solid same type
                        // and hasn't been consumed
                        BlockType btype = chunk.GetBlock(checkX, checkY, checkZ);
                        if (btype != BlockType::Air && !consumed[((checkZ * CHUNK_SIZE) + checkY) * CHUNK_SIZE + checkX]) {
                            // Verify the entire row is the same type
                            bool rowOk = true;
                            for (int sx = startX; sx <= checkX; ++sx) {
                                for (int sy = startY; sy <= checkY; ++sy) {
                                    if (chunk.GetBlock(sx, sy, startZ) != btype) {
                                        rowOk = false;
                                        break;
                                    }
                                }
                                if (!rowOk) break;
                            }
                            if (rowOk) {
                                wx = checkX;
                                wy = checkY;
                                wz = startZ;
                                canGrowWidth = true;
                            }
                            else {
                                canGrowWidth = false;
                            }
                        }
                        else {
                            canGrowWidth = false;
                        }
                    }

                    // Grow height along the appropriate axis for this face direction
                    int hx = startX, hy = startY, hz = startZ;
                    bool canGrowHeight = true;
                    while (canGrowHeight) {
                        // Check next block in height direction based on face direction
                        int checkX = hx, checkY = hy, checkZ = hz;
                        switch (dir) {
                        case 0: // +X face: height along Z axis
                        case 1: // -X face: height along Z axis
                        case 2: // +Y face: height along Z axis
                        case 3: // -Y face: height along Z axis
                            checkZ++; break;
                        case 4: // +Z face: height along Y axis
                            checkY++; break;
                        case 5: // -Z face: height along Y axis
                            checkY++; break;
                        }

                        // Check if the block at (checkX, checkY, checkZ) is solid same type
                        // and hasn't been consumed
                        BlockType btype = chunk.GetBlock(checkX, checkY, checkZ);
                        if (btype != BlockType::Air && !consumed[((checkZ * CHUNK_SIZE) + checkY) * CHUNK_SIZE + checkX]) {
                            // Verify column is same type (all blocks from startX to wx, startY to hy)
                            bool colOk = true;
                            for (int sy = startY; sy <= checkY; ++sy) {
                                for (int sx = startX; sx <= wx; ++sx) {
                                    if (chunk.GetBlock(sx, sy, startZ) != btype) {
                                        colOk = false;
                                        break;
                                    }
                                }
                                if (!colOk) break;
                            }
                            if (colOk) {
                                hy = checkY;
                                hz = checkZ;
                                canGrowHeight = true;
                            }
                            else {
                                canGrowHeight = false;
                            }
                        }
                        else {
                            canGrowHeight = false;
                        }
                    }

                    // Height is how many rows we grew
                    height = (hy - startY + 1);

                    // Clamp dimensions
                    if (width > 16) width = 16;
                    if (height > 16) height = 16;

                    // Skip if rectangle is too small
                    if (width < 1 || height < 1) continue;

                    // Third pass: add the merged quad to the mesh
                    // Calculate AO for this rectangle (simple approximation)
                    float avgAo = 0.5f; // Default AO

                    // Add the quad vertices and indices
                    // Calculate UV origin based on block type
                    BlockType blockType = chunk.GetBlock(startX, startY, startZ);
                    float tileSize = 1.0f / 16.0f;
                    int typeIndex = static_cast<int>(blockType);
                    int tx = typeIndex % 16;
                    int ty = typeIndex / 16;

                    // Set up face properties based on direction
                    float normal[3];
                    float uvAxis1[3];
                    float uvAxis2[3];
                    int faceWidthDir, faceHeightDir;

                    // Set up face properties based on direction
                    switch (dir) {
                    case 0: // +X face
                        normal[0] = 1.0f; normal[1] = 0.0f; normal[2] = 0.0f;
                        uvAxis1[0] = 0; uvAxis1[1] = 1; uvAxis1[2] = 0; // Y axis
                        uvAxis2[0] = 0; uvAxis2[1] = 0; uvAxis2[2] = 1; // Z axis
                        faceWidthDir = 1; // Y controls width
                        faceHeightDir = 2; // Z controls height
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

                    // Rectangle dimensions in the face's coordinate system
                    int rectWidth = width;
                    int rectHeight = height;

                    // Determine the 4 corner positions of the rectangle
                    float fx0, fy0, fz0; // corner 0
                    float fx1, fy1, fz1; // corner 1
                    float fx2, fy2, fz2; // corner 2
                    float fx3, fy3, fz3; // corner 3

                    // Base position
                    float baseX = static_cast<float>(startX);
                    float baseY = static_cast<float>(startY);
                    float baseZ = static_cast<float>(startZ);

                    // Set up quad vertices based on face direction
                    switch (dir) {
                    case 0: // +X face: quad in YZ plane, width=Y, height=Z
                        fx0 = baseX + 1.0f; fy0 = baseY + 0.0f; fz0 = baseZ + 0.0f;
                        fx1 = baseX + 1.0f; fy1 = baseY + height; fz1 = baseZ + 0.0f;
                        fx2 = baseX + 1.0f; fy2 = baseY + height; fz2 = baseZ + width;
                        fx3 = baseX + 1.0f; fy3 = baseY + 0.0f; fz3 = baseZ + width;
                        break;

                    case 1: // -X face: quad in YZ plane, width=Y, height=Z
                        fx0 = baseX; fy0 = baseY + 0.0f; fz0 = baseZ + 0.0f;
                        fx1 = baseX; fy1 = baseY + height; fz1 = baseZ + 0.0f;
                        fx2 = baseX; fy2 = baseY + height; fz2 = baseZ + width;
                        fx3 = baseX; fy3 = baseY + 0.0f; fz3 = baseZ + width;
                        break;

                    case 2: // +Y face: quad in XZ plane, width=X, height=Z
                        fx0 = baseX + 0.0f; fy0 = baseY + 1.0f; fz0 = baseZ + 0.0f;
                        fx1 = baseX + width; fy1 = baseY + 1.0f; fz1 = baseZ + 0.0f;
                        fx2 = baseX + width; fy2 = baseY + 1.0f; fz2 = baseZ + height;
                        fx3 = baseX + 0.0f; fy3 = baseY + 1.0f; fz3 = baseZ + height;
                        break;

                    case 3: // -Y face: quad in XZ plane, width=X, height=Z
                        fx0 = baseX + 0.0f; fy0 = baseY + 0.0f; fz0 = baseZ + 0.0f;
                        fx1 = baseX + width; fy1 = baseY + 0.0f; fz1 = baseZ + 0.0f;
                        fx2 = baseX + width; fy2 = baseY + 0.0f; fz2 = baseZ + height;
                        fx3 = baseX + 0.0f; fy3 = baseY + 0.0f; fz3 = baseZ + height;
                        break;

                    case 4: // +Z face: quad in XY plane, width=X, height=Y
                        fx0 = baseX + 0.0f; fy0 = baseY + 0.0f; fz0 = baseZ + 1.0f;
                        fx1 = baseX + width; fy1 = baseY + 0.0f; fz1 = baseZ + 1.0f;
                        fx2 = baseX + width; fy2 = baseY + height; fz2 = baseZ + 1.0f;
                        fx3 = baseX + 0.0f; fy3 = baseY + height; fz3 = baseZ + 1.0f;
                        break;

                    case 5: // -Z face: quad in XY plane, width=X, height=Y
                        fx0 = baseX + 0.0f; fy0 = baseY + 0.0f; fz0 = baseZ;
                        fx1 = baseX + width; fy1 = baseY + 0.0f; fz1 = baseZ;
                        fx2 = baseX + width; fy2 = baseY + height; fz2 = baseZ;
                        fx3 = baseX + 0.0f; fy3 = baseY + height; fz3 = baseZ;
                        break;
                    }

                    // Add 4 vertices (each has pos+normal+uv+ao)
                    // Vertex 0
                    Vertex v0;
                    v0.pos[0] = fx0; v0.pos[1] = fy0; v0.pos[2] = fz0;
                    v0.normal[0] = normal[0]; v0.normal[1] = normal[1]; v0.normal[2] = normal[2];
                    // UV: map to 16x16 tile in 256x256 atlas
                    v0.uv[0] = (tx + 0.5f) / 16.0f; // Center of tile
                    v0.uv[1] = (ty + 0.5f) / 16.0f;
                    v0.ao = avgAo;
                    result.vertices.push_back(v0);

                    // Vertex 1
                    Vertex v1;
                    v1.pos[0] = fx1; v1.pos[1] = fy1; v1.pos[2] = fz1;
                    v1.normal[0] = normal[0]; v1.normal[1] = normal[1]; v1.normal[2] = normal[2];
                    // UV: offset by 1 row in V direction for height
                    v1.uv[0] = (tx + 0.5f) / 16.0f;
                    v1.uv[1] = (ty + 1.5f) / 16.0f;
                    v1.ao = avgAo;
                    result.vertices.push_back(v1);

                    // Vertex 2
                    Vertex v2;
                    v2.pos[0] = fx2; v2.pos[1] = fy2; v2.pos[2] = fz2;
                    v2.normal[0] = normal[0]; v2.normal[1] = normal[1]; v2.normal[2] = normal[2];
                    // UV: offset by 1 in both directions
                    v2.uv[0] = (tx + 1.5f) / 16.0f;
                    v2.uv[1] = (ty + 1.5f) / 16.0f;
                    v2.ao = avgAo;
                    result.vertices.push_back(v2);

                    // Vertex 3
                    Vertex v3;
                    v3.pos[0] = fx3; v3.pos[1] = fy3; v3.pos[2] = fz3;
                    v3.normal[0] = normal[0]; v3.normal[1] = normal[1]; v3.normal[2] = normal[2];
                    v3.uv[0] = (tx + 0.5f) / 16.0f;
                    v3.uv[1] = (ty + 1.5f) / 16.0f;
                    v3.ao = avgAo;
                    result.vertices.push_back(v3);

                    // Add 6 indices: two triangles forming the quad
                    // Triangle 1: v0, v1, v2
                    result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 0));
                    result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 1));
                    result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 2));
                    // Triangle 2: v0, v2, v3
                    result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 0));
                    result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 2));
                    result.indices.push_back(static_cast<uint32_t>(result.vertices.size() - 4 + 3));

                    // Mark all blocks in this rectangle as consumed
                    for (int sy = 0; sy < height; ++sy) {
                        for (int sx = 0; sx < width; ++sx) {
                            // Calculate the consumed block position based on direction
                            int cx = startX;
                            int cy = startY + sy;
                            int cz = startZ;
                            // Mark as consumed
                            consumed[((startZ * CHUNK_SIZE) + (startY + sy)) * CHUNK_SIZE + (startX + sx)] = 1;
                        }
                    }
                }
            }
        }
    }

    result.vertexCount = static_cast<uint32_t>(result.vertices.size());
    result.indexCount = static_cast<uint32_t>(result.indices.size());

    return result;
}

// ============================================================
// GreedyMesher::Mesh with callback
// ============================================================
void GreedyMesher::Mesh(const Chunk& chunk, const World& world,
    std::function<void(uint64_t)> callback) {
    Mesh mesh = GreedyMesher::Mesh(chunk);

    // Calculate chunk key based on chunk position
    // For now use default values - would need actual chunk coords
    int cx = 0, cy = 0, cz = 0;
    uint64_t key = 0;

    // Invoke callback
    if (callback) {
        callback(key);
    }
}