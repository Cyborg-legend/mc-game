#ifndef BLOCKFORGE_C_BG_H
#define BLOCKFORGE_C_BG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHUNK_SIZE 16
#define CHUNK_VOLUME 4096
#define UPPERWORLD_TOP 96
#define UNDERWORLD_CEILING 120
#define UNDERWORLD_TOP 128
#define SEA_LEVEL 34

/* GLFW key codes run past 255 (arrows, Enter, Escape), so the table must too. */
#define INPUT_KEY_COUNT 512

/* Block palette. Every texture is painted at runtime; no image assets exist. */
typedef enum {
    BG_AIR = 0,
    BG_STONE,
    BG_DIRT,
    BG_GRASS,
    BG_WOOD,
    BG_LEAVES,
    BG_SAND,
    BG_WATER,
    BG_BEDROCK,
    BG_PLANKS,
    BG_STICKS,
    BG_CRAFTING_TABLE,
    BG_BRICK,
    BG_EMBERSTONE,
    BG_ASHSOIL,
    BG_LUMENSTONE,
    BG_MAGMA,
    BG_VOIDSTONE,
    BG_CINDERBRICK,
    BG_PALE_QUARTZ,
    BG_CRIMSON_WOOD,
    BG_BLOODLEAVES,
    BG_EMBERLODE,
    BG_IRONLODE,
    BG_PORTAL,
    BG_GLOWMOSS,
    BG_CINDERSTONE,
    BG_ASHBLOCK,
    BG_BLOCK_COUNT
} BlockId;

typedef enum {
    BG_UPPERWORLD = 0,
    BG_UNDERWORLD = 1
} Dimension;

const char *dimension_name(Dimension dimension);

bool block_collidable(BlockId block);
bool block_liquid(BlockId block);
bool block_portal(BlockId block);
bool block_breakable(BlockId block);

/* Atlas tile index for one block face; face order is +X, -X, +Y, -Y, +Z, -Z. */
int block_tile(BlockId block, int face);

typedef struct Chunk {
    BlockId blocks[CHUNK_VOLUME];
    int cx, cy, cz;
} Chunk;

typedef struct ChunkSlot {
    uint64_t key;
    Chunk chunk;
    bool used;
} ChunkSlot;

typedef struct World {
    ChunkSlot *slots;
    size_t capacity;
    size_t count;
    Dimension dimension;
    uint64_t seed;
} World;

void world_init(World *world);
void world_free(World *world);
void world_clear(World *world);

void world_set_dimension(World *world, Dimension dimension);
void world_set_seed(World *world, uint64_t seed);

/* Stable key for a chunk coordinate; the renderer uses the same key space. */
uint64_t chunk_key_of(int cx, int cy, int cz);

Chunk *world_chunk(World *world, int cx, int cy, int cz, bool create);
BlockId world_get_block(const World *world, int x, int y, int z);
void world_set_block(World *world, int x, int y, int z, BlockId block);
void world_generate_around(World *world, int center_cx, int center_cz, int radius);
int world_surface_y(const World *world, int x, int z);
size_t world_chunk_count(const World *world);

/* Iteration order is unspecified; entries stay valid until the world mutates. */
typedef struct WorldIter {
    World *world;
    size_t index;
} WorldIter;

Chunk *world_iter_next(WorldIter *iter);

/* ---- Rift gate ------------------------------------------------------- */

/* Feet height of a standing gate, -1 when there is none. */
int find_rift_gate_y(const World *world, int gate_x, int gate_z);

/* Builds the shrine only when it is missing; an existing gate (or a gate with
 * a broken portal) is repaired in place instead of re-carved. */
int build_rift_gate(World *world, int gate_x, int gate_z);

/* ---- Mesh ------------------------------------------------------------ */

typedef struct BgVertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float ao;
} BgVertex;

typedef struct BgMesh {
    BgVertex *vertices;
    size_t vertex_count;
    uint32_t *indices;
    size_t index_count;
} BgMesh;

void mesh_chunk(const World *world, const Chunk *chunk, BgMesh *mesh);
void mesh_free(BgMesh *mesh);

/* ---- Input and player ------------------------------------------------ */

typedef struct BgInput {
    bool keys[INPUT_KEY_COUNT];
    int16_t mouse_dx;
    int16_t mouse_dy;
    bool mouse[5];
} BgInput;

typedef struct BgPlayer {
    float x, y, z;
    float vx, vy, vz;
    float yaw, pitch;
    bool on_ground;
    int health;
    int selected;
    BlockId hotbar[9];
    BlockId inventory[36];
} BgPlayer;

void player_init(BgPlayer *player);
void player_update(BgPlayer *player, float dt, const BgInput *input, const World *world);
bool player_raycast(const BgPlayer *player, const World *world, float max_dist,
                    int *bx, int *by, int *bz, BlockId *type, int normal[3]);
bool player_place(BgPlayer *player, World *world, BlockId block, int *px, int *py, int *pz);

/* ---- Save ------------------------------------------------------------ */

bool save_world(const char *path, const World *world, const BgPlayer *player);
bool load_world(const char *path, World *world, BgPlayer *player);

#endif /* BLOCKFORGE_C_BG_H */
