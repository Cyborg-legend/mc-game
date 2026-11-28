#include "bg.h"

#include <stdlib.h>
#include <string.h>

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

/* Tangent axes, used to place the three ambient-occlusion probes per corner
 * and as the two axes a merged face sweeps along. */
static const int kFaceTangentU[6] = { 1, 1, 0, 0, 0, 0 };
static const int kFaceTangentV[6] = { 2, 2, 2, 2, 1, 1 };

static bool solid_at(const World *world, int x, int y, int z) {
    return block_collidable(world_get_block(world, x, y, z));
}

/* Baked light for a face, sampled from the air cell it looks into. */
static float light_factor(const World *world, int x, int y, int z) {
    const uint8_t value = world_light_at(world, x, y, z);
    const int sky = value >> 4;
    const int block = value & 15;
    const int level = sky > block ? sky : block;
    return 0.18f + 0.82f * (float)level / 15.0f;
}

static bool reserve_vertices(BgMesh *mesh, size_t *capacity, size_t needed);
static bool reserve_indices(BgMesh *mesh, size_t *capacity, size_t needed);

/* A torch is a thin post rather than a cube, so it gets its own geometry. */
static bool emit_torch(BgMesh *mesh, int wx, int wy, int wz) {
    const float min[3] = { (float)wx + 0.42f, (float)wy, (float)wz + 0.42f };
    const float max[3] = { (float)wx + 0.58f, (float)wy + 0.62f, (float)wz + 0.58f };
    const float corner_u[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float corner_v[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    const int tile = block_tile(BG_TORCH, 0);

    for (int face = 0; face < 6; ++face) {
        if (!reserve_vertices(mesh, &mesh->vertex_capacity, mesh->vertex_count + 4) ||
            !reserve_indices(mesh, &mesh->index_capacity, mesh->index_count + 6)) {
            return false;
        }
        const uint32_t base = (uint32_t)mesh->vertex_count;
        for (int c = 0; c < 4; ++c) {
            BgVertex *vertex = &mesh->vertices[mesh->vertex_count++];
            for (int axis = 0; axis < 3; ++axis) {
                const float t = (float)kCornerOffsets[face][c][axis];
                vertex->pos[axis] = min[axis] + (max[axis] - min[axis]) * t;
                vertex->normal[axis] = kFaceNormals[face][axis];
            }
            vertex->uv[0] = corner_u[c];
            vertex->uv[1] = corner_v[c];
            vertex->tile[0] = (float)(tile % 16);
            vertex->tile[1] = (float)(tile / 16);
            vertex->ao = 1.0f;
            vertex->light = 1.0f;
        }
        mesh->indices[mesh->index_count++] = base + 0;
        mesh->indices[mesh->index_count++] = base + 1;
        mesh->indices[mesh->index_count++] = base + 2;
        mesh->indices[mesh->index_count++] = base + 0;
        mesh->indices[mesh->index_count++] = base + 2;
        mesh->indices[mesh->index_count++] = base + 3;
    }
    return true;
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

/* One exposed block face waiting to be merged into a larger quad. Two cells
 * may only join when they agree on texture, baked light and all four corner
 * occlusions, so a merged quad is always flat, evenly lit and evenly shaded. */
typedef struct GreedyCell {
    int tile;
    float light;
    float ao[4];
    bool present;
} GreedyCell;

static bool cells_match(const GreedyCell *a, const GreedyCell *b) {
    return a->present && b->present && a->tile == b->tile && a->light == b->light &&
           a->ao[0] == b->ao[0] && a->ao[1] == b->ao[1] && a->ao[2] == b->ao[2] &&
           a->ao[3] == b->ao[3];
}

/* Emits one rectangle of merged faces. Corners keep the per-face winding of a
 * single quad, so culling and shading are unchanged; the UVs carry how many
 * blocks the quad spans and the tile index rides along in its own attribute. */
static bool emit_greedy_quad(BgMesh *mesh, int face, int slice, const int origin[3],
                             int u0, int v0, int w, int h, const GreedyCell *cell) {
    if (!reserve_vertices(mesh, &mesh->vertex_capacity, mesh->vertex_count + 4) ||
        !reserve_indices(mesh, &mesh->index_capacity, mesh->index_count + 6)) {
        return false;
    }

    const int axis_d = face / 2;
    const int axis_u = kFaceTangentU[face];
    const int axis_v = kFaceTangentV[face];
    const float tile_x = (float)(cell->tile % 16);
    const float tile_y = (float)(cell->tile / 16);

    const uint32_t base = (uint32_t)mesh->vertex_count;
    for (int c = 0; c < 4; ++c) {
        const int cu = kCornerOffsets[face][c][axis_u];
        const int cv = kCornerOffsets[face][c][axis_v];
        BgVertex *vertex = &mesh->vertices[mesh->vertex_count++];
        vertex->pos[axis_u] = (float)(origin[axis_u] + u0 + cu * w);
        vertex->pos[axis_v] = (float)(origin[axis_v] + v0 + cv * h);
        vertex->pos[axis_d] = (float)(origin[axis_d] + slice + kCornerOffsets[face][c][axis_d]);
        vertex->normal[0] = kFaceNormals[face][0];
        vertex->normal[1] = kFaceNormals[face][1];
        vertex->normal[2] = kFaceNormals[face][2];
        vertex->uv[0] = (float)(cu * w);
        vertex->uv[1] = (float)(cv * h);
        vertex->tile[0] = tile_x;
        vertex->tile[1] = tile_y;
        vertex->ao = cell->ao[c];
        vertex->light = cell->light;
    }

    /* Split along the darker diagonal so AO gradients stay smooth. */
    if (cell->ao[0] + cell->ao[2] < cell->ao[1] + cell->ao[3]) {
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
    return true;
}

/* Builds the exposed-face mask for one slice of one face direction, merges it
 * into rectangles and emits them. */
static bool greedy_slice(const World *world, const Chunk *chunk, int face, int slice,
                         const int origin[3], BgMesh *mesh,
                         GreedyCell mask[CHUNK_SIZE][CHUNK_SIZE]) {
    const int axis_d = face / 2;
    const int axis_u = kFaceTangentU[face];
    const int axis_v = kFaceTangentV[face];
    const int nx = (int)kFaceNormals[face][0];
    const int ny = (int)kFaceNormals[face][1];
    const int nz = (int)kFaceNormals[face][2];
    int step_u[3] = { 0, 0, 0 };
    int step_v[3] = { 0, 0, 0 };
    step_u[axis_u] = 1;
    step_v[axis_v] = 1;

    for (int u = 0; u < CHUNK_SIZE; ++u) {
        for (int v = 0; v < CHUNK_SIZE; ++v) {
            int local[3];
            local[axis_d] = slice;
            local[axis_u] = u;
            local[axis_v] = v;
            const BlockId block =
                chunk->blocks[(local[1] * CHUNK_SIZE + local[2]) * CHUNK_SIZE + local[0]];
            if (block == BG_AIR || block == BG_TORCH) {
                continue;
            }

            const int wx = origin[0] + local[0];
            const int wy = origin[1] + local[1];
            const int wz = origin[2] + local[2];
            const BlockId neighbor = world_get_block(world, wx + nx, wy + ny, wz + nz);
            if (block_collidable(neighbor) || neighbor == block) {
                continue;
            }

            GreedyCell *cell = &mask[u][v];
            cell->present = true;
            cell->tile = block_tile(block, face);
            cell->light = light_factor(world, wx + nx, wy + ny, wz + nz);
            for (int c = 0; c < 4; ++c) {
                const int su = kCornerOffsets[face][c][axis_u] * 2 - 1;
                const int sv = kCornerOffsets[face][c][axis_v] * 2 - 1;
                const bool side_u = solid_at(world,
                    wx + nx + su * step_u[0], wy + ny + su * step_u[1], wz + nz + su * step_u[2]);
                const bool side_v = solid_at(world,
                    wx + nx + sv * step_v[0], wy + ny + sv * step_v[1], wz + nz + sv * step_v[2]);
                const bool diagonal = solid_at(world,
                    wx + nx + su * step_u[0] + sv * step_v[0],
                    wy + ny + su * step_u[1] + sv * step_v[1],
                    wz + nz + su * step_u[2] + sv * step_v[2]);

                const int occlusion = (side_u && side_v)
                                          ? 3
                                          : (side_u ? 1 : 0) + (side_v ? 1 : 0) + (diagonal ? 1 : 0);
                cell->ao[c] = 1.0f - 0.2f * (float)occlusion;
            }
        }
    }

    for (int v = 0; v < CHUNK_SIZE; ++v) {
        int u = 0;
        while (u < CHUNK_SIZE) {
            const GreedyCell *start = &mask[u][v];
            if (!start->present) {
                ++u;
                continue;
            }
            int w = 1;
            while (u + w < CHUNK_SIZE && cells_match(&mask[u + w][v], start)) {
                ++w;
            }
            int h = 1;
            while (v + h < CHUNK_SIZE) {
                bool row_matches = true;
                for (int k = 0; k < w; ++k) {
                    if (!cells_match(&mask[u + k][v + h], start)) {
                        row_matches = false;
                        break;
                    }
                }
                if (!row_matches) {
                    break;
                }
                ++h;
            }

            if (!emit_greedy_quad(mesh, face, slice, origin, u, v, w, h, start)) {
                return false;
            }
            for (int dv = 0; dv < h; ++dv) {
                for (int du = 0; du < w; ++du) {
                    mask[u + du][v + dv].present = false;
                }
            }
            u += w;
        }
    }
    return true;
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

    const int origin[3] = { chunk->cx * CHUNK_SIZE, chunk->cy * CHUNK_SIZE,
                            chunk->cz * CHUNK_SIZE };

    /* One mask reused by every slice: the merge step consumes every present
     * cell, so it is empty again by the time the next slice needs it. */
    GreedyCell mask[CHUNK_SIZE][CHUNK_SIZE];
    memset(mask, 0, sizeof(mask));

    /* Torches are posts, not cubes, so they skip the greedy pass. */
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                if (chunk->blocks[(y * CHUNK_SIZE + z) * CHUNK_SIZE + x] != BG_TORCH) {
                    continue;
                }
                if (!emit_torch(mesh, origin[0] + x, origin[1] + y, origin[2] + z)) {
                    return;
                }
            }
        }
    }

    for (int face = 0; face < 6; ++face) {
        for (int slice = 0; slice < CHUNK_SIZE; ++slice) {
            if (!greedy_slice(world, chunk, face, slice, origin, mesh, mask)) {
                return;
            }
        }
    }
}

void mesh_box(BgMesh *mesh, const float center[3], const float half[3], BlockId skin) {
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

        const uint32_t base = (uint32_t)mesh->vertex_count;
        for (int c = 0; c < 4; ++c) {
            BgVertex *vertex = &mesh->vertices[mesh->vertex_count++];
            for (int axis = 0; axis < 3; ++axis) {
                const float sign = (float)kCornerOffsets[face][c][axis] * 2.0f - 1.0f;
                vertex->pos[axis] = center[axis] + sign * half[axis];
                vertex->normal[axis] = kFaceNormals[face][axis];
            }
            vertex->uv[0] = corner_u[c];
            vertex->uv[1] = corner_v[c];
            vertex->tile[0] = (float)(tile % 16);
            vertex->tile[1] = (float)(tile / 16);
            vertex->ao = face_ao[face];
            vertex->light = 1.0f;
        }

        mesh->indices[mesh->index_count++] = base + 0;
        mesh->indices[mesh->index_count++] = base + 1;
        mesh->indices[mesh->index_count++] = base + 2;
        mesh->indices[mesh->index_count++] = base + 0;
        mesh->indices[mesh->index_count++] = base + 2;
        mesh->indices[mesh->index_count++] = base + 3;
    }
}
