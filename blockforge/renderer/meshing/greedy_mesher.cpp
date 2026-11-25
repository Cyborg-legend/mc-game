#include "meshing/greedy_mesher.h"
#include "meshing/mesh.h"

namespace {

// Quad corner offsets for each face direction (winding is CCW seen from outside).
// Face order: +X, -X, +Y, -Y, +Z, -Z
const int kCornerOffsets[6][4][3] = {
    // +X
    {{1,0,0},{1,1,0},{1,1,1},{1,0,1}},
    // -X
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}},
    // +Y
    {{0,1,0},{0,1,1},{1,1,1},{1,1,0}},
    // -Y
    {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},
    // +Z
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},
    // -Z
    {{0,0,0},{0,1,0},{1,1,0},{1,0,0}}
};

// Tangent axes of each face, used to place the three ambient-occlusion probes
// around a quad corner: one along U, one along V and one on the diagonal.
const int kFaceTangentU[6] = { 1, 1, 0, 0, 0, 0 }; // Y, Y, X, X, X, X
const int kFaceTangentV[6] = { 2, 2, 2, 2, 1, 1 }; // Z, Z, Z, Z, Y, Y

// Samples a block at chunk-local coordinates, falling through to the world for
// border queries. The local fast path avoids a hash lookup for most faces.
BlockType Sample(const World& world, const Chunk& chunk, int lx, int ly, int lz) {
    if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE) {
        return chunk.GetBlock(lx, ly, lz);
    }
    const int wx = chunk.cx * CHUNK_SIZE + lx;
    const int wy = chunk.cy * CHUNK_SIZE + ly;
    const int wz = chunk.cz * CHUNK_SIZE + lz;
    if (wy < 0 || wy >= UNDERWORLD_TOP) {
        return BlockType::Air;
    }
    return world.GetBlock(wx, wy, wz);
}

bool Solid(const World& world, const Chunk& chunk, int lx, int ly, int lz) {
    return IsOpaque(Sample(world, chunk, lx, ly, lz));
}

} // namespace

MeshChunk GreedyMesher::Mesh(const World& world, const Chunk& chunk) {
    MeshChunk result;

    const float originX = static_cast<float>(chunk.cx * CHUNK_SIZE);
    const float originY = static_cast<float>(chunk.cy * CHUNK_SIZE);
    const float originZ = static_cast<float>(chunk.cz * CHUNK_SIZE);

    // Atlas is 16x16 tiles of 16x16 pixels; each tile is a block face texture.
    constexpr float kTilesPerRow = 16.0f;
    constexpr float kTileSize = 1.0f / kTilesPerRow;

    // Corner UV within a tile (matches the winding of kCornerOffsets).
    const float cornerU[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    const float cornerV[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const BlockType type = chunk.GetBlock(x, y, z);
                if (type == BlockType::Air) {
                    continue;
                }

                for (int dir = 0; dir < 6; ++dir) {
                    const int nx = x + static_cast<int>(faceNormals[dir][0]);
                    const int ny = y + static_cast<int>(faceNormals[dir][1]);
                    const int nz = z + static_cast<int>(faceNormals[dir][2]);

                    // Opaque neighbours hide the face; liquids and portals only
                    // cull against their own kind so their surfaces stay visible.
                    const BlockType neighbor = Sample(world, chunk, nx, ny, nz);
                    if (IsOpaque(neighbor) || neighbor == type) {
                        continue;
                    }

                    const int tile = TileForFace(type, dir);
                    const float tu = static_cast<float>(tile % 16) * kTileSize;
                    const float tv = static_cast<float>(tile / 16) * kTileSize;
                    // Half-texel inset prevents bleeding across atlas tiles.
                    const float pad = kTileSize * 0.002f;
                    const float span = kTileSize - 2.0f * pad;

                    // Per-corner ambient occlusion, like Minecraft: a vertex
                    // darkens when the blocks beside it and the block across
                    // the corner are filled.
                    float ao[4];
                    for (int c = 0; c < 4; ++c) {
                        const int su = kCornerOffsets[dir][c][kFaceTangentU[dir]] * 2 - 1;
                        const int sv = kCornerOffsets[dir][c][kFaceTangentV[dir]] * 2 - 1;

                        int offsetU[3] = { 0, 0, 0 };
                        int offsetV[3] = { 0, 0, 0 };
                        offsetU[kFaceTangentU[dir]] = su;
                        offsetV[kFaceTangentV[dir]] = sv;

                        const bool sideU = Solid(world, chunk,
                            nx + offsetU[0], ny + offsetU[1], nz + offsetU[2]);
                        const bool sideV = Solid(world, chunk,
                            nx + offsetV[0], ny + offsetV[1], nz + offsetV[2]);
                        const bool corner = Solid(world, chunk,
                            nx + offsetU[0] + offsetV[0],
                            ny + offsetU[1] + offsetV[1],
                            nz + offsetU[2] + offsetV[2]);

                        const int occlusion = (sideU && sideV)
                            ? 3
                            : (sideU ? 1 : 0) + (sideV ? 1 : 0) + (corner ? 1 : 0);
                        ao[c] = 1.0f - 0.2f * static_cast<float>(occlusion);
                    }

                    const uint32_t base = static_cast<uint32_t>(result.vertices.size());

                    for (int c = 0; c < 4; ++c) {
                        Vertex v{};
                        v.pos[0] = originX + static_cast<float>(x + kCornerOffsets[dir][c][0]);
                        v.pos[1] = originY + static_cast<float>(y + kCornerOffsets[dir][c][1]);
                        v.pos[2] = originZ + static_cast<float>(z + kCornerOffsets[dir][c][2]);
                        v.normal[0] = faceNormals[dir][0];
                        v.normal[1] = faceNormals[dir][1];
                        v.normal[2] = faceNormals[dir][2];
                        v.uv[0] = tu + pad + cornerU[c] * span;
                        v.uv[1] = tv + pad + cornerV[c] * span;
                        v.ao = ao[c];
                        result.vertices.push_back(v);
                    }

                    // Split along the darker diagonal so AO gradients stay smooth.
                    if (ao[0] + ao[2] < ao[1] + ao[3]) {
                        result.indices.push_back(base + 1);
                        result.indices.push_back(base + 2);
                        result.indices.push_back(base + 3);
                        result.indices.push_back(base + 1);
                        result.indices.push_back(base + 3);
                        result.indices.push_back(base + 0);
                    } else {
                        result.indices.push_back(base + 0);
                        result.indices.push_back(base + 1);
                        result.indices.push_back(base + 2);
                        result.indices.push_back(base + 0);
                        result.indices.push_back(base + 2);
                        result.indices.push_back(base + 3);
                    }
                }
            }
        }
    }

    result.vertexCount = static_cast<uint32_t>(result.vertices.size());
    result.indexCount = static_cast<uint32_t>(result.indices.size());
    return result;
}
