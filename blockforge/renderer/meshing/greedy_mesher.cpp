#include "greedy_mesher.h"
#include "core/world/chunk.h"
#include <cstring>

MeshChunk GreedyMesher::Mesh(const Chunk& chunk, const World& world) {
    MeshChunk result;
    result.vertices.clear();
    result.indices.clear();

    int baseX = chunk.cx * CHUNK_SIZE;
    int baseY = chunk.cy * CHUNK_SIZE;
    int baseZ = chunk.cz * CHUNK_SIZE;

    // 6 face directions
    static const int dirs[6][3] = {
        {0, -1, 0}, {0, 1, 0},
        {-1, 0, 0}, {1, 0, 0},
        {0, 0, -1}, {0, 0, 1}
    };

    for (int d = 0; d < 6; ++d) {
        int dx = dirs[d][0], dy = dirs[d][1], dz = dirs[d][2];

        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    BlockType bt = chunk.GetBlock(x, y, z);
                    if (bt == BlockType::Air) continue;

                    int nx = baseX + x + dx;
                    int ny = baseY + y + dy;
                    int nz = baseZ + z + dz;
                    BlockType neighbor = world.GetBlock(nx, ny, nz);

                    if (neighbor != BlockType::Air && neighbor != BlockType::Water) continue;

                    // Check if this face is part of a larger quad
                    bool canExpandX = false, canExpandZ = false;
                    if (dy != 0) {
                        if (x + 1 < CHUNK_SIZE) {
                            BlockType b2 = chunk.GetBlock(x+1, y, z);
                            if (b2 == bt) {
                                BlockType n2 = world.GetBlock(baseX+x+1+dx, ny, nz);
                                if (n2 == BlockType::Air || n2 == BlockType::Water) canExpandX = true;
                            }
                        }
                        if (z + 1 < CHUNK_SIZE) {
                            BlockType b3 = chunk.GetBlock(x, y, z+1);
                            if (b3 == bt) {
                                BlockType n3 = world.GetBlock(nx, ny, baseZ+z+1+dz);
                                if (n3 == BlockType::Air || n3 == BlockType::Water) canExpandZ = true;
                            }
                        }
                    } else if (dx != 0) {
                        if (y + 1 < CHUNK_SIZE) {
                            BlockType b2 = chunk.GetBlock(x, y+1, z);
                            if (b2 == bt) {
                                BlockType n2 = world.GetBlock(nx, baseY+y+1+dy, nz);
                                if (n2 == BlockType::Air || n2 == BlockType::Water) canExpandX = true;
                            }
                        }
                        if (z + 1 < CHUNK_SIZE) {
                            BlockType b3 = chunk.GetBlock(x, y, z+1);
                            if (b3 == bt) {
                                BlockType n3 = world.GetBlock(nx, ny, baseZ+z+1+dz);
                                if (n3 == BlockType::Air || n3 == BlockType::Water) canExpandZ = true;
                            }
                        }
                    } else {
                        if (x + 1 < CHUNK_SIZE) {
                            BlockType b2 = chunk.GetBlock(x+1, y, z);
                            if (b2 == bt) {
                                BlockType n2 = world.GetBlock(baseX+x+1+dx, ny, nz);
                                if (n2 == BlockType::Air || n2 == BlockType::Water) canExpandX = true;
                            }
                        }
                        if (y + 1 < CHUNK_SIZE) {
                            BlockType b3 = chunk.GetBlock(x, y+1, z);
                            if (b3 == bt) {
                                BlockType n3 = world.GetBlock(nx, baseY+y+1+dy, nz);
                                if (n3 == BlockType::Air || n3 == BlockType::Water) canExpandZ = true;
                            }
                        }
                    }

                    float wx = (float)(baseX + x);
                    float wy = (float)(baseY + y);
                    float wz = (float)(baseZ + z);

                    // Simple quad (1x1) - greedy merge skipped for simplicity
                    float ax = wx + (dx != 0 ? dx : 0);
                    float ay = wy + (dy != 0 ? dy : 0);
                    float az = wz + (dz != 0 ? dz : 0);

                    uint32_t base = (uint32_t)result.vertices.size();

                    if (dy != 0) {
                        result.vertices.push_back({ax, ay, az, (float)dx, (float)dy, (float)dz, 0, 0, 1.0f});
                        result.vertices.push_back({ax+1, ay, az, (float)dx, (float)dy, (float)dz, 1, 0, 1.0f});
                        result.vertices.push_back({ax+1, ay, az+1, (float)dx, (float)dy, (float)dz, 1, 1, 1.0f});
                        result.vertices.push_back({ax, ay, az+1, (float)dx, (float)dy, (float)dz, 0, 1, 1.0f});
                    } else if (dx != 0) {
                        result.vertices.push_back({ax, ay, az, (float)dx, (float)dy, (float)dz, 0, 0, 1.0f});
                        result.vertices.push_back({ax, ay+1, az, (float)dx, (float)dy, (float)dz, 0, 1, 1.0f});
                        result.vertices.push_back({ax, ay+1, az+1, (float)dx, (float)dy, (float)dz, 1, 1, 1.0f});
                        result.vertices.push_back({ax, ay, az+1, (float)dx, (float)dy, (float)dz, 1, 0, 1.0f});
                    } else {
                        result.vertices.push_back({ax, ay, az, (float)dx, (float)dy, (float)dz, 0, 0, 1.0f});
                        result.vertices.push_back({ax+1, ay, az, (float)dx, (float)dy, (float)dz, 1, 0, 1.0f});
                        result.vertices.push_back({ax+1, ay+1, az, (float)dx, (float)dy, (float)dz, 1, 1, 1.0f});
                        result.vertices.push_back({ax, ay+1, az, (float)dx, (float)dy, (float)dz, 0, 1, 1.0f});
                    }

                    result.indices.push_back(base);
                    result.indices.push_back(base + 1);
                    result.indices.push_back(base + 2);
                    result.indices.push_back(base);
                    result.indices.push_back(base + 2);
                    result.indices.push_back(base + 3);
                }
            }
        }
    }

    result.vertexCount = (uint32_t)result.vertices.size();
    result.indexCount = (uint32_t)result.indices.size();
    return result;
}   