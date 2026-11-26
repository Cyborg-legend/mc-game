#include "bg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- blocks */

const char *dimension_name(Dimension dimension) {
    return dimension == BG_UNDERWORLD ? "Underworld" : "Upperworld";
}

bool block_liquid(BlockId block) {
    return block == BG_WATER;
}

bool block_portal(BlockId block) {
    return block == BG_PORTAL;
}

bool block_collidable(BlockId block) {
    return block != BG_AIR && !block_liquid(block) && !block_portal(block);
}

bool block_breakable(BlockId block) {
    return block != BG_AIR && block != BG_BEDROCK;
}

/* ----------------------------------------------------------------- noise */

static uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static float hash2f(int x, int z, uint64_t seed) {
    uint32_t h = hash_u32((uint32_t)x * 374761393U ^ (uint32_t)z * 668265263U ^ (uint32_t)seed);
    return (float)(h & 0xFFFFFF) / 16777215.0f;
}

static float hash3f(int x, int y, int z, uint64_t seed) {
    uint32_t h = hash_u32((uint32_t)x * 374761393U ^ (uint32_t)y * 2246822519U ^
                          (uint32_t)z * 668265263U ^ (uint32_t)seed);
    return (float)(h & 0xFFFFFF) / 16777215.0f;
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float smooth(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float value_noise2(float x, float z, uint64_t seed) {
    const int xi = (int)floorf(x);
    const int zi = (int)floorf(z);
    const float fx = smooth(x - (float)xi);
    const float fz = smooth(z - (float)zi);
    const float a = hash2f(xi, zi, seed);
    const float b = hash2f(xi + 1, zi, seed);
    const float c = hash2f(xi, zi + 1, seed);
    const float d = hash2f(xi + 1, zi + 1, seed);
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fz);
}

static float fbm2(float x, float z, uint64_t seed, int octaves) {
    float total = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        total += value_noise2(x * frequency, z * frequency, seed + (uint64_t)i * 97u) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return norm > 0.0f ? total / norm : 0.0f;
}

static float value_noise3(float x, float y, float z, uint64_t seed) {
    const int xi = (int)floorf(x);
    const int yi = (int)floorf(y);
    const int zi = (int)floorf(z);
    const float fx = smooth(x - (float)xi);
    const float fy = smooth(y - (float)yi);
    const float fz = smooth(z - (float)zi);

    const float c000 = hash3f(xi, yi, zi, seed);
    const float c100 = hash3f(xi + 1, yi, zi, seed);
    const float c010 = hash3f(xi, yi + 1, zi, seed);
    const float c110 = hash3f(xi + 1, yi + 1, zi, seed);
    const float c001 = hash3f(xi, yi, zi + 1, seed);
    const float c101 = hash3f(xi + 1, yi, zi + 1, seed);
    const float c011 = hash3f(xi, yi + 1, zi + 1, seed);
    const float c111 = hash3f(xi + 1, yi + 1, zi + 1, seed);

    const float x00 = lerp(c000, c100, fx);
    const float x10 = lerp(c010, c110, fx);
    const float x01 = lerp(c001, c101, fx);
    const float x11 = lerp(c011, c111, fx);
    return lerp(lerp(x00, x10, fy), lerp(x01, x11, fy), fz);
}

/* ---------------------------------------------------------------- chunk map */

uint64_t chunk_key_of(int cx, int cy, int cz) {
    return ((uint64_t)((uint32_t)cx & 0x1FFFFFu)) |
           (((uint64_t)((uint32_t)cy & 0x1FFFFFu)) << 21) |
           (((uint64_t)((uint32_t)cz & 0x1FFFFFu)) << 42);
}

static size_t key_index(uint64_t key, size_t capacity) {
    return (size_t)(hash_u32((uint32_t)key ^ (uint32_t)(key >> 32)) & (uint32_t)(capacity - 1));
}

static bool ensure_capacity(World *world, size_t extra) {
    if (world->capacity == 0) {
        world->capacity = 1024;
        world->slots = (ChunkSlot *)calloc(world->capacity, sizeof(ChunkSlot));
        return world->slots != NULL;
    }
    if ((world->count + extra) * 10 < world->capacity * 7) {
        return true;
    }

    const size_t new_capacity = world->capacity * 2;
    ChunkSlot *slots = (ChunkSlot *)calloc(new_capacity, sizeof(ChunkSlot));
    if (!slots) {
        return false;
    }
    for (size_t i = 0; i < world->capacity; ++i) {
        if (!world->slots[i].used) {
            continue;
        }
        size_t index = key_index(world->slots[i].key, new_capacity);
        while (slots[index].used) {
            index = (index + 1) & (new_capacity - 1);
        }
        slots[index] = world->slots[i];
    }
    free(world->slots);
    world->slots = slots;
    world->capacity = new_capacity;
    return true;
}

void world_init(World *world) {
    memset(world, 0, sizeof(*world));
    world->dimension = BG_UPPERWORLD;
    world->seed = 1337u;
}

void world_free(World *world) {
    free(world->slots);
    memset(world, 0, sizeof(*world));
}

void world_clear(World *world) {
    if (world->slots) {
        memset(world->slots, 0, world->capacity * sizeof(ChunkSlot));
    }
    world->count = 0;
}

void world_set_dimension(World *world, Dimension dimension) {
    world->dimension = dimension;
}

void world_set_seed(World *world, uint64_t seed) {
    world->seed = seed;
}

Chunk *world_chunk(World *world, int cx, int cy, int cz, bool create) {
    const uint64_t key = chunk_key_of(cx, cy, cz);
    if (world->capacity > 0) {
        size_t index = key_index(key, world->capacity);
        for (size_t probes = 0; probes < world->capacity; ++probes) {
            if (!world->slots[index].used) {
                break;
            }
            if (world->slots[index].key == key) {
                return &world->slots[index].chunk;
            }
            index = (index + 1) & (world->capacity - 1);
        }
    }
    if (!create) {
        return NULL;
    }
    if (!ensure_capacity(world, 1)) {
        return NULL;
    }

    size_t index = key_index(key, world->capacity);
    while (world->slots[index].used) {
        index = (index + 1) & (world->capacity - 1);
    }
    Chunk *chunk = &world->slots[index].chunk;
    memset(chunk->blocks, 0, sizeof(chunk->blocks));
    chunk->cx = cx;
    chunk->cy = cy;
    chunk->cz = cz;
    world->slots[index].key = key;
    world->slots[index].used = true;
    world->count++;
    return chunk;
}

static Chunk *chunk_find(const World *world, int cx, int cy, int cz) {
    if (world->capacity == 0) {
        return NULL;
    }
    const uint64_t key = chunk_key_of(cx, cy, cz);
    size_t index = key_index(key, world->capacity);
    for (size_t probes = 0; probes < world->capacity; ++probes) {
        if (!world->slots[index].used) {
            return NULL;
        }
        if (world->slots[index].key == key) {
            return (Chunk *)&world->slots[index].chunk;
        }
        index = (index + 1) & (world->capacity - 1);
    }
    return NULL;
}

BlockId world_get_block(const World *world, int x, int y, int z) {
    if (y < 0 || y >= UNDERWORLD_TOP) {
        return BG_AIR;
    }
    const int cx = x >> 4;
    const int cy = y >> 4;
    const int cz = z >> 4;
    const Chunk *chunk = chunk_find(world, cx, cy, cz);
    if (!chunk) {
        return BG_AIR;
    }
    return chunk->blocks[((y & 15) * CHUNK_SIZE + (z & 15)) * CHUNK_SIZE + (x & 15)];
}

void world_set_block(World *world, int x, int y, int z, BlockId block) {
    if (y < 0 || y >= UNDERWORLD_TOP) {
        return;
    }
    Chunk *chunk = world_chunk(world, x >> 4, y >> 4, z >> 4, true);
    if (!chunk) {
        return;
    }
    chunk->blocks[((y & 15) * CHUNK_SIZE + (z & 15)) * CHUNK_SIZE + (x & 15)] = block;
}

size_t world_chunk_count(const World *world) {
    return world->count;
}

Chunk *world_iter_next(WorldIter *iter) {
    World *world = iter->world;
    while (iter->index < world->capacity) {
        ChunkSlot *slot = &world->slots[iter->index++];
        if (slot->used) {
            return &slot->chunk;
        }
    }
    return NULL;
}

/* --------------------------------------------------------------- terrain */

static int upperworld_height(int x, int z, uint64_t seed) {
    const float base = fbm2((float)x * 0.012f, (float)z * 0.012f, seed, 4);
    const float detail = fbm2((float)x * 0.06f, (float)z * 0.06f, seed + 31u, 2);
    return 26 + (int)(base * 32.0f) + (int)(detail * 5.0f);
}

static int underworld_height(int x, int z, uint64_t seed) {
    const float base = fbm2((float)x * 0.02f, (float)z * 0.02f, seed + 101u, 4);
    return 62 + (int)(base * 20.0f);
}

static void generate_upperworld_chunk(World *world, Chunk *chunk) {
    const int ox = chunk->cx * CHUNK_SIZE;
    const int oy = chunk->cy * CHUNK_SIZE;
    const int oz = chunk->cz * CHUNK_SIZE;

    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            const int wx = ox + lx;
            const int wz = oz + lz;
            const int height = upperworld_height(wx, wz, world->seed);

            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                const int wy = oy + ly;
                BlockId block = BG_AIR;

                if (wy == 0) {
                    block = BG_BEDROCK;
                } else if (wy <= height) {
                    block = (wy > height - 4)
                                ? ((height <= SEA_LEVEL + 1) ? BG_SAND : BG_DIRT)
                                : BG_STONE;

                    if (wy > 4 &&
                        value_noise3((float)wx * 0.07f, (float)wy * 0.09f, (float)wz * 0.07f,
                                     world->seed + 11u) > 0.63f) {
                        block = BG_AIR;
                    }
                    if (block == BG_STONE) {
                        const float ore = hash3f(wx, wy, wz, world->seed + 5u);
                        if (ore > 0.987f) {
                            block = BG_EMBERLODE;
                        } else if (ore > 0.975f && wy < 30) {
                            block = BG_IRONLODE;
                        }
                    }
                    if (wy == height && block != BG_AIR) {
                        block = (height <= SEA_LEVEL + 1) ? BG_SAND : BG_GRASS;
                    }
                } else if (wy <= SEA_LEVEL) {
                    block = BG_WATER;
                }

                chunk->blocks[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx] = block;
            }
        }
    }
}

static void generate_underworld_chunk(World *world, Chunk *chunk) {
    const int ox = chunk->cx * CHUNK_SIZE;
    const int oy = chunk->cy * CHUNK_SIZE;
    const int oz = chunk->cz * CHUNK_SIZE;
    const int lava_level = 36;

    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            const int wx = ox + lx;
            const int wz = oz + lz;
            const int surface = underworld_height(wx, wz, world->seed);
            const bool pillar = hash2f(wx / 3, wz / 3, world->seed + 21u) > 0.996f;

            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                const int wy = oy + ly;
                BlockId block = BG_AIR;

                if (wy <= 3 || wy >= UNDERWORLD_CEILING) {
                    block = BG_BEDROCK;
                } else if (wy <= surface) {
                    block = (wy == surface) ? BG_ASHSOIL : BG_EMBERSTONE;
                    if (wy > 6 &&
                        value_noise3((float)wx * 0.07f, (float)wy * 0.09f, (float)wz * 0.07f,
                                     world->seed + 13u) > 0.60f) {
                        block = BG_AIR;
                    }
                    if (block == BG_EMBERSTONE) {
                        const float vein = hash3f(wx, wy, wz, world->seed + 17u);
                        if (vein > 0.993f) {
                            block = BG_LUMENSTONE;
                        } else if (vein > 0.985f) {
                            block = BG_GLOWMOSS;
                        } else if (vein > 0.974f) {
                            block = BG_PALE_QUARTZ;
                        } else if (vein > 0.960f) {
                            block = BG_EMBERLODE;
                        }
                    }
                    if (pillar && wy > 8) {
                        block = BG_VOIDSTONE;
                    }
                }

                if (block == BG_AIR && wy <= lava_level) {
                    block = BG_MAGMA;
                }
                if (block == BG_MAGMA && wy > lava_level - 4 && hash3f(wx, wy, wz, world->seed + 3u) > 0.85f) {
                    block = BG_AIR; /* bubbles in the sea surface */
                }

                chunk->blocks[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx] = block;
            }
        }
    }
}

static void decorate_upperworld(World *world, int x0, int z0, int x1, int z1) {
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            if (hash2f(x, z, world->seed + 77u) < 0.987f) {
                continue;
            }
            const int ground = world_surface_y(world, x, z);
            if (ground <= SEA_LEVEL) {
                continue;
            }
            if (world_get_block(world, x, ground - 1, z) != BG_GRASS) {
                continue;
            }
            const int trunk = 4 + (int)(hash2f(x, z, world->seed + 78u) * 3.0f);
            for (int i = 0; i < trunk; ++i) {
                world_set_block(world, x, ground + i, z, BG_WOOD);
            }
            const int top = ground + trunk;
            for (int dy = -2; dy <= 1; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dz = -2; dz <= 2; ++dz) {
                        if (dx * dx + dz * dz > 5) {
                            continue;
                        }
                        if (dx == 0 && dz == 0 && dy < 1) {
                            continue;
                        }
                        if (world_get_block(world, x + dx, top + dy, z + dz) == BG_AIR) {
                            world_set_block(world, x + dx, top + dy, z + dz, BG_LEAVES);
                        }
                    }
                }
            }
        }
    }
}

void world_generate_around(World *world, int center_cx, int center_cz, int radius) {
    const int top = (world->dimension == BG_UPPERWORLD) ? UPPERWORLD_TOP : UNDERWORLD_TOP;
    const int chunk_rows = top / CHUNK_SIZE;

    for (int cx = center_cx - radius; cx <= center_cx + radius; ++cx) {
        for (int cz = center_cz - radius; cz <= center_cz + radius; ++cz) {
            for (int cy = 0; cy < chunk_rows; ++cy) {
                Chunk *chunk = world_chunk(world, cx, cy, cz, true);
                if (!chunk) {
                    continue;
                }
                if (world->dimension == BG_UPPERWORLD) {
                    generate_upperworld_chunk(world, chunk);
                } else {
                    generate_underworld_chunk(world, chunk);
                }
            }
        }
    }

    if (world->dimension == BG_UPPERWORLD) {
        decorate_upperworld(world,
                            (center_cx - radius) * CHUNK_SIZE,
                            (center_cz - radius) * CHUNK_SIZE,
                            (center_cx + radius + 1) * CHUNK_SIZE - 1,
                            (center_cz + radius + 1) * CHUNK_SIZE - 1);
    }
}

int world_surface_y(const World *world, int x, int z) {
    const int start = (world->dimension == BG_UNDERWORLD) ? (UNDERWORLD_CEILING - 20)
                                                          : (UPPERWORLD_TOP - 2);
    for (int y = start; y >= 0; --y) {
        if (block_collidable(world_get_block(world, x, y, z)) &&
            !block_collidable(world_get_block(world, x, y + 1, z)) &&
            !block_collidable(world_get_block(world, x, y + 2, z))) {
            return y + 1;
        }
    }
    return 2;
}

/* ----------------------------------------------------------- world sites */

/* Walks outward from an angle until the terrain there stands clear of the sea,
 * so the first spawn is never carved under water. */
static bool find_dry_site(const World *world, float angle, int start_distance, int step,
                          int *out_x, int *out_z) {
    int distance = start_distance;
    for (int attempt = 0; attempt < 48; ++attempt) {
        const int x = (int)lroundf(cosf(angle) * (float)distance);
        const int z = (int)lroundf(sinf(angle) * (float)distance);
        const int height = upperworld_height(x, z, world->seed);
        if (height > SEA_LEVEL + 2 && height < UPPERWORLD_TOP - 10) {
            *out_x = x;
            *out_z = z;
            return true;
        }
        angle += 0.61803398875f * 6.28318530718f;
        distance += step;
    }
    return false;
}

void world_spawn_site(const World *world, int *spawn_x, int *spawn_z) {
    *spawn_x = 0;
    *spawn_z = 0;
    (void)find_dry_site(world, 0.6f, 0, 6, spawn_x, spawn_z);
}

/* ------------------------------------------------------------ rift frame */

/* A rift frame is a 4x5 rectangle of voidstone standing upright along the X or
 * Z axis. Its 2x3 opening holds the portal blocks. As with a classic portal,
 * the four corner blocks are decorative and not required. */
static bool rift_ring_ok(const World *world, int cx, int cy, int cz, int axis, bool *lit) {
    bool all_lit = true;
    for (int dy = 0; dy < 5; ++dy) {
        for (int d = 0; d < 4; ++d) {
            const bool edge_y = (dy == 0 || dy == 4);
            const bool edge_d = (d == 0 || d == 3);
            const int x = axis == 0 ? cx + d : cx;
            const int z = axis == 0 ? cz : cz + d;
            const BlockId block = world_get_block(world, x, cy + dy, z);
            if (edge_y || edge_d) {
                if (edge_y && edge_d) {
                    continue;
                }
                if (block != BG_VOIDSTONE) {
                    return false;
                }
            } else if (block == BG_PORTAL) {
                continue;
            } else if (block != BG_AIR) {
                return false;
            } else {
                all_lit = false;
            }
        }
    }
    if (lit) {
        *lit = all_lit;
    }
    return true;
}

static bool rift_find(const World *world, int x, int y, int z, int *out_cx, int *out_cy,
                      int *out_cz, int *out_axis, bool *lit) {
    for (int axis = 0; axis < 2; ++axis) {
        for (int ay = y - 4; ay <= y; ++ay) {
            for (int d = 0; d < 4; ++d) {
                const int cx = axis == 0 ? x - d : x;
                const int cz = axis == 0 ? z : z - d;
                bool frame_lit = false;
                if (rift_ring_ok(world, cx, ay, cz, axis, &frame_lit)) {
                    *out_cx = cx;
                    *out_cy = ay;
                    *out_cz = cz;
                    *out_axis = axis;
                    if (lit) {
                        *lit = frame_lit;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool rift_frame_at(const World *world, int x, int y, int z, int *cx, int *cy, int *cz,
                   int *axis) {
    bool lit = false;
    return rift_find(world, x, y, z, cx, cy, cz, axis, &lit);
}

RiftFire rift_ignite(World *world, int x, int y, int z, int *cx, int *cy, int *cz, int *axis) {
    bool lit = false;
    if (!rift_find(world, x, y, z, cx, cy, cz, axis, &lit)) {
        return RIFT_NO_FRAME;
    }
    if (lit) {
        return RIFT_ALREADY_OPEN;
    }
    for (int iy = 1; iy <= 3; ++iy) {
        for (int id = 1; id <= 2; ++id) {
            const int bx = *axis == 0 ? *cx + id : *cx;
            const int bz = *axis == 0 ? *cz : *cz + id;
            world_set_block(world, bx, *cy + iy, bz, BG_PORTAL);
        }
    }
    return RIFT_OPENED;
}

void rift_build(World *world, int cx, int cy, int cz, int axis) {
    /* Clear a shell around the gate first, since the twin side of a rift can
     * sit inside solid rock. Bedrock is left alone. */
    for (int dy = -1; dy <= 5; ++dy) {
        for (int d = -1; d <= 4; ++d) {
            for (int side = -1; side <= 1; ++side) {
                const int x = axis == 0 ? cx + d : cx + side;
                const int z = axis == 0 ? cz + side : cz + d;
                if (world_get_block(world, x, cy + dy, z) != BG_BEDROCK) {
                    world_set_block(world, x, cy + dy, z, BG_AIR);
                }
            }
        }
    }

    for (int dy = 0; dy < 5; ++dy) {
        for (int d = 0; d < 4; ++d) {
            const int x = axis == 0 ? cx + d : cx;
            const int z = axis == 0 ? cz : cz + d;
            const bool edge = (dy == 0 || dy == 4 || d == 0 || d == 3);
            world_set_block(world, x, cy + dy, z, edge ? BG_VOIDSTONE : BG_PORTAL);
        }
    }

    /* A paved footing so the gate never hangs over a drop or a lava sea. */
    if (cy > 0) {
        for (int d = -1; d <= 4; ++d) {
            for (int side = -1; side <= 1; ++side) {
                const int x = axis == 0 ? cx + d : cx + side;
                const int z = axis == 0 ? cz + side : cz + d;
                if (world_get_block(world, x, cy - 1, z) == BG_AIR) {
                    world_set_block(world, x, cy - 1, z, BG_CINDERBRICK);
                }
            }
        }
    }
}

/* ------------------------------------------------------------- atlas ids */

enum {
    TILE_STONE = 1, TILE_DIRT, TILE_GRASS_TOP, TILE_GRASS_SIDE, TILE_WOOD_SIDE,
    TILE_WOOD_TOP, TILE_LEAVES, TILE_SAND, TILE_WATER, TILE_BEDROCK, TILE_PLANKS,
    TILE_STICKS, TILE_CRAFT_TOP, TILE_CRAFT_SIDE, TILE_BRICK, TILE_EMBERSTONE,
    TILE_ASHSOIL, TILE_LUMENSTONE, TILE_MAGMA, TILE_VOIDSTONE, TILE_CINDERBRICK,
    TILE_PALE_QUARTZ, TILE_CRIMSON_SIDE, TILE_CRIMSON_TOP, TILE_BLOODLEAVES,
    TILE_EMBERLODE, TILE_IRONLODE, TILE_PORTAL, TILE_GLOWMOSS, TILE_CINDERSTONE,
    TILE_ASHBLOCK
};

int block_tile(BlockId block, int face) {
    const bool top = (face == 2);
    const bool bottom = (face == 3);

    switch (block) {
        case BG_STONE:          return TILE_STONE;
        case BG_DIRT:           return TILE_DIRT;
        case BG_GRASS:          return top ? TILE_GRASS_TOP : (bottom ? TILE_DIRT : TILE_GRASS_SIDE);
        case BG_WOOD:           return (top || bottom) ? TILE_WOOD_TOP : TILE_WOOD_SIDE;
        case BG_LEAVES:         return TILE_LEAVES;
        case BG_SAND:           return TILE_SAND;
        case BG_WATER:          return TILE_WATER;
        case BG_BEDROCK:        return TILE_BEDROCK;
        case BG_PLANKS:         return TILE_PLANKS;
        case BG_STICKS:         return TILE_STICKS;
        case BG_CRAFTING_TABLE: return top ? TILE_CRAFT_TOP : TILE_CRAFT_SIDE;
        case BG_BRICK:          return TILE_BRICK;
        case BG_EMBERSTONE:     return TILE_EMBERSTONE;
        case BG_ASHSOIL:        return TILE_ASHSOIL;
        case BG_LUMENSTONE:     return TILE_LUMENSTONE;
        case BG_MAGMA:          return TILE_MAGMA;
        case BG_VOIDSTONE:      return TILE_VOIDSTONE;
        case BG_CINDERBRICK:    return TILE_CINDERBRICK;
        case BG_PALE_QUARTZ:    return TILE_PALE_QUARTZ;
        case BG_CRIMSON_WOOD:   return (top || bottom) ? TILE_CRIMSON_TOP : TILE_CRIMSON_SIDE;
        case BG_BLOODLEAVES:    return TILE_BLOODLEAVES;
        case BG_EMBERLODE:      return TILE_EMBERLODE;
        case BG_IRONLODE:       return TILE_IRONLODE;
        case BG_PORTAL:         return TILE_PORTAL;
        case BG_GLOWMOSS:       return TILE_GLOWMOSS;
        case BG_CINDERSTONE:    return TILE_CINDERSTONE;
        case BG_ASHBLOCK:       return TILE_ASHBLOCK;
        default:                return TILE_STONE;
    }
}
