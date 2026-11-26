#include "bg.h"

#include <stdlib.h>

/* Face order: +X, -X, +Y, -Y, +Z, -Z. */
static const float kFaceNormals[6][3] = {
    {  1.0f,  0.0f,  0.0f },
    { -1.0f,  0.0f,  0.0f },
    {  0.0f,  1.0f,  0.0f },
    {  0.0f, -1.0f,  0.0f },
    {  0.0f,  0.0f,  1.0f },
    {  0.0f,  0.0f, -1.0f }
};

/* Quad corners per face, wound counter-clockwise seen from outside. */
static const int kCornerOffsets[6][4][3] = {
    { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } },
    { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } },
    { { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 } },
    { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } },
    { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
    { { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 } }
};

/* Tangent axes, used to place the three ambient-occlusion probes per corner. */
static const int kFaceTangentU[6] = { 1, 1, 0, 0, 0, 0 };
static const int kFaceTangentV[6] = { 2, 2, 2, 2, 1, 1 };

static bool solid_at(const World *world, int x, int y, int z) {
    return block_collidable(world_get_block(world, x, y, z));
}

/* Isotropic tiles survive any quarter turn, so a field of stone or dirt can be
 * spun per block instead of repeating one stamp over and over. */
static bool tile_rotates(BlockId block) {
    switch (block) {
        case BG_STONE:
        case BG_DIRT:
        case BG_SAND:
        case BG_LEAVES:
        case BG_BEDROCK:
        case BG_EMBERSTONE:
        case BG_ASHSOIL:
        case BG_EMBERLODE:
        case BG_IRONLODE:
        case BG_PALE_QUARTZ:
        case BG_GLOWMOSS:
        case BG_CINDERSTONE:
        case BG_ASHBLOCK:
        case BG_BLOODLEAVES:
            return true;
        default:
            return false;
    }
}

static uint32_t face_spin(int x, int y, int z, int face) {
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^
                 (uint32_t)z * 83492791u ^ (uint32_t)face * 2654435761u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h & 3u;
}

static bool reserve_vertices(BgMesh *mesh, size_t *capacity, size_t needed) {
    if (*capacity >= needed) {
        return true;
    }
    size_t next = *capacity ? *capacity : 2048;
    while (next < needed) {
        next *= 2;
    }
    BgVertex *grown = (BgVertex *)realloc(mesh->vertices, next * sizeof(BgVertex));
    if (!grown) {
        return false;
    }
    mesh->vertices = grown;
    *capacity = next;
    return true;
}

static bool reserve_indices(BgMesh *mesh, size_t *capacity, size_t needed) {
    if (*capacity >= needed) {
        return true;
    }
    size_t next = *capacity ? *capacity : 3072;
    while (next < needed) {
        next *= 2;
    }
    uint32_t *grown = (uint32_t *)realloc(mesh->indices, next * sizeof(uint32_t));
    if (!grown) {
        return false;
    }
    mesh->indices = grown;
    *capacity = next;
    return true;
}

void mesh_reset(BgMesh *mesh) {
    mesh->vertex_count = 0;
    mesh->index_count = 0;
}

void mesh_free(BgMesh *mesh) {
    free(mesh->vertices);
    free(mesh->indices);
    mesh->vertices = NULL;
    mesh->indices = NULL;
    mesh->vertex_count = 0;
    mesh->vertex_capacity = 0;
    mesh->index_count = 0;
    mesh->index_capacity = 0;
}

void mesh_chunk(const World *world, const Chunk *chunk, BgMesh *mesh) {
    /* Chunk meshes are always built from scratch, so drop any previous
     * pointers rather than trusting the caller's struct. */
    mesh->vertices = NULL;
    mesh->vertex_count = 0;
    mesh->vertex_capacity = 0;
    mesh->indices = NULL;
    mesh->index_count = 0;
    mesh->index_capacity = 0;

    const float origin_x = (float)(chunk->cx * CHUNK_SIZE);
    const float origin_y = (float)(chunk->cy * CHUNK_SIZE);
    const float origin_z = (float)(chunk->cz * CHUNK_SIZE);

    const float tiles_per_row = 16.0f;
    const float tile_size = 1.0f / tiles_per_row;
    const float pad = tile_size * 0.002f;
    const float span = tile_size - 2.0f * pad;
    const float corner_u[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float corner_v[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const BlockId block = chunk->blocks[(y * CHUNK_SIZE + z) * CHUNK_SIZE + x];
                if (block == BG_AIR) {
                    continue;
                }
                const int world_x = chunk->cx * CHUNK_SIZE + x;
                const int world_y = chunk->cy * CHUNK_SIZE + y;
                const int world_z = chunk->cz * CHUNK_SIZE + z;

                for (int face = 0; face < 6; ++face) {
                    const int nx = world_x + (int)kFaceNormals[face][0];
                    const int ny = world_y + (int)kFaceNormals[face][1];
                    const int nz = world_z + (int)kFaceNormals[face][2];

                    const BlockId neighbor = world_get_block(world, nx, ny, nz);
                    if (block_collidable(neighbor) || neighbor == block) {
                        continue;
                    }

                    const int tile = block_tile(block, face);
                    const float tu = (float)(tile % 16) * tile_size;
                    const float tv = (float)(tile / 16) * tile_size;
                    const uint32_t spin = tile_rotates(block)
                                              ? face_spin(world_x, world_y, world_z, face)
                                              : 0u;

                    const int axis_u = kFaceTangentU[face];
                    const int axis_v = kFaceTangentV[face];
                    int step_u[3] = { 0, 0, 0 };
                    int step_v[3] = { 0, 0, 0 };
                    step_u[axis_u] = 1;
                    step_v[axis_v] = 1;

                    float ao[4];
                    for (int c = 0; c < 4; ++c) {
                        const int su = kCornerOffsets[face][c][axis_u] * 2 - 1;
                        const int sv = kCornerOffsets[face][c][axis_v] * 2 - 1;
                        const bool side_u = solid_at(world,
                            nx + su * step_u[0], ny + su * step_u[1], nz + su * step_u[2]);
                        const bool side_v = solid_at(world,
                            nx + sv * step_v[0], ny + sv * step_v[1], nz + sv * step_v[2]);
                        const bool diagonal = solid_at(world,
                            nx + su * step_u[0] + sv * step_v[0],
                            ny + su * step_u[1] + sv * step_v[1],
                            nz + su * step_u[2] + sv * step_v[2]);

                        const int occlusion = (side_u && side_v)
                                                  ? 3
                                                  : (side_u ? 1 : 0) + (side_v ? 1 : 0) + (diagonal ? 1 : 0);
                        ao[c] = 1.0f - 0.2f * (float)occlusion;
                    }

                    if (!reserve_vertices(mesh, &mesh->vertex_capacity, mesh->vertex_count + 4) ||
                        !reserve_indices(mesh, &mesh->index_capacity, mesh->index_count + 6)) {
                        return;
                    }

                    const uint32_t base = (uint32_t)mesh->vertex_count;
                    for (int c = 0; c < 4; ++c) {
                        BgVertex *vertex = &mesh->vertices[mesh->vertex_count++];
                        vertex->pos[0] = origin_x + (float)(x + kCornerOffsets[face][c][0]);
                        vertex->pos[1] = origin_y + (float)(y + kCornerOffsets[face][c][1]);
                        vertex->pos[2] = origin_z + (float)(z + kCornerOffsets[face][c][2]);
                        vertex->normal[0] = kFaceNormals[face][0];
                        vertex->normal[1] = kFaceNormals[face][1];
                        vertex->normal[2] = kFaceNormals[face][2];
                        float u = corner_u[c];
                        float v = corner_v[c];
                        if (spin == 1u) {
                            const float swap = u;
                            u = v;
                            v = 1.0f - swap;
                        } else if (spin == 2u) {
                            u = 1.0f - u;
                            v = 1.0f - v;
                        } else if (spin == 3u) {
                            const float swap = u;
                            u = 1.0f - v;
                            v = swap;
                        }
                        vertex->uv[0] = tu + pad + u * span;
                        vertex->uv[1] = tv + pad + v * span;
                        vertex->ao = ao[c];
                    }

                    /* Split along the darker diagonal so AO gradients stay smooth. */
                    if (ao[0] + ao[2] < ao[1] + ao[3]) {
                        mesh->indices[mesh->index_count++] = base + 1;
                        mesh->indices[mesh->index_count++] = base + 2;
                        mesh->indices[mesh->index_count++] = base + 3;
                        mesh->indices[mesh->index_count++] = base + 1;
                        mesh->indices[mesh->index_count++] = base + 3;
                        mesh->indices[mesh->index_count++] = base + 0;
                    } else {
                        mesh->indices[mesh->index_count++] = base + 0;
                        mesh->indices[mesh->index_count++] = base + 1;
                        mesh->indices[mesh->index_count++] = base + 2;
                        mesh->indices[mesh->index_count++] = base + 0;
                        mesh->indices[mesh->index_count++] = base + 2;
                        mesh->indices[mesh->index_count++] = base + 3;
                    }
                }
            }
        }
    }
}

void mesh_box(BgMesh *mesh, const float center[3], const float half[3], BlockId skin) {
    const float tiles_per_row = 16.0f;
    const float tile_size = 1.0f / tiles_per_row;
    const float pad = tile_size * 0.002f;
    const float span = tile_size - 2.0f * pad;
    const float corner_u[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float corner_v[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    /* Soft shading per face so flat boxes still read as solid volumes. */
    const float face_ao[6] = { 0.86f, 0.86f, 1.0f, 0.62f, 0.92f, 0.92f };

    for (int face = 0; face < 6; ++face) {
        if (!reserve_vertices(mesh, &mesh->vertex_capacity, mesh->vertex_count + 4) ||
            !reserve_indices(mesh, &mesh->index_capacity, mesh->index_count + 6)) {
            return;
        }
        const int tile = block_tile(skin, face);
        const float tu = (float)(tile % 16) * tile_size;
        const float tv = (float)(tile / 16) * tile_size;

        const uint32_t base = (uint32_t)mesh->vertex_count;
        for (int c = 0; c < 4; ++c) {
            BgVertex *vertex = &mesh->vertices[mesh->vertex_count++];
            for (int axis = 0; axis < 3; ++axis) {
                const float sign = (float)kCornerOffsets[face][c][axis] * 2.0f - 1.0f;
                vertex->pos[axis] = center[axis] + sign * half[axis];
                vertex->normal[axis] = kFaceNormals[face][axis];
            }
            vertex->uv[0] = tu + pad + corner_u[c] * span;
            vertex->uv[1] = tv + pad + corner_v[c] * span;
            vertex->ao = face_ao[face];
        }

        mesh->indices[mesh->index_count++] = base + 0;
        mesh->indices[mesh->index_count++] = base + 1;
        mesh->indices[mesh->index_count++] = base + 2;
        mesh->indices[mesh->index_count++] = base + 0;
        mesh->indices[mesh->index_count++] = base + 2;
        mesh->indices[mesh->index_count++] = base + 3;
    }
}
