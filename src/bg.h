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

typedef struct BgPlayer BgPlayer;

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

/* Open, dry ground near the origin, where a fresh Upperworld begins. */
void world_spawn_site(const World *world, int *spawn_x, int *spawn_z);

/* Seed-derived shrine site, well away from the spawn so the rift has to be
 * found by exploring instead of standing on the doorstep. */
void world_gate_site(const World *world, int *gate_x, int *gate_z);

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

/* ---- Items and inventory --------------------------------------------- */

/* The pack is the nine-slot hotbar, so every item is placeable. */
#define INVENTORY_SLOTS 9
#define HOTBAR_SLOTS 9
#define MAX_STACK 64

/* Item ids below BG_BLOCK_COUNT are the placeable blocks; the crafting-only
 * items follow them. */
typedef enum {
    BG_ITEM_STICK = BG_BLOCK_COUNT,
    BG_ITEM_EMBER_SHARD,
    BG_ITEM_ASH_DUST,
    BG_ITEM_MEAT,
    BG_ITEM_COUNT
} ItemId;

typedef struct BgStack {
    uint16_t item;
    uint8_t count;
} BgStack;

const char *item_name(uint16_t item);
bool item_block(uint16_t item, BlockId *block);

/* What breaking a block puts in the inventory; false when it drops nothing. */
bool block_drop(BlockId block, uint16_t *item, int *count);

int inventory_count(const BgStack *slots, uint16_t item);
bool inventory_add(BgStack *slots, uint16_t item, int count);
bool inventory_remove(BgStack *slots, uint16_t item, int count);
void inventory_clear(BgStack *slots);

/* ---- Crafting -------------------------------------------------------- */

#define MAX_RECIPE_INPUTS 4

typedef struct BgIngredient {
    uint16_t item;
    uint8_t count;
} BgIngredient;

typedef struct BgRecipe {
    BgIngredient inputs[MAX_RECIPE_INPUTS];
    int input_count;
    BgStack output;
    bool needs_table;
} BgRecipe;

int recipe_count(void);
const BgRecipe *recipe_at(int index);
bool recipe_can_craft(const BgStack *slots, const BgRecipe *recipe, bool near_table);
bool recipe_craft(BgStack *slots, const BgRecipe *recipe, bool near_table);

/* ---- Mob ------------------------------------------------------------- */

#define MAX_MOBS 64

typedef enum {
    MOB_GRAZER = 0,
    MOB_CINDER_HOUND,
    MOB_ASH_WISP,
    MOB_TYPE_COUNT
} MobType;

typedef struct BgMob {
    uint8_t type;
    bool alive;
    bool on_ground;
    bool idle;
    float x, y, z;
    float vx, vy, vz;
    float yaw;
    int health;
    float hurt_timer;
    float attack_timer;
    float wander_timer;
    float wander_yaw;
} BgMob;

typedef struct BgMobWorld {
    BgMob mobs[MAX_MOBS];
    int count;
    float spawn_timer;
    uint32_t rng;
} BgMobWorld;

const char *mob_name(MobType type);
float mob_half_width(MobType type);
float mob_height(MobType type);
bool mob_hostile(MobType type);

void mobs_init(BgMobWorld *mobs);
void mobs_clear(BgMobWorld *mobs);
void mobs_update(BgMobWorld *mobs, World *world, BgPlayer *player, float dt);

/* Index of the mob under the crosshair, -1 when there is none or a block is
 * in the way. */
int mobs_pick(const BgMobWorld *mobs, const World *world, const BgPlayer *player,
              float max_dist);

/* Applies damage and knockback. Returns true when the hit was lethal, with
 * the dead mob's type in killed_type (call with a valid index only). */
bool mobs_damage(BgMobWorld *mobs, int index, int damage, float dir_x, float dir_z,
                 MobType *killed_type);

/* Drops for a dead mob; returns how many entries of out are filled. */
int mob_drops(MobType type, BgStack out[2]);

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
    size_t vertex_capacity;
    uint32_t *indices;
    size_t index_count;
    size_t index_capacity;
} BgMesh;

/* Builds a fresh mesh; any previous contents of *mesh are discarded. */
void mesh_chunk(const World *world, const Chunk *chunk, BgMesh *mesh);

/* Appends one axis-aligned box to the mesh, skinned with a block's faces. */
void mesh_box(BgMesh *mesh, const float center[3], const float half[3], BlockId skin);
void mesh_reset(BgMesh *mesh);
void mesh_free(BgMesh *mesh);

/* ---- Input and player ------------------------------------------------ */

typedef struct BgInput {
    bool keys[INPUT_KEY_COUNT];
    int16_t mouse_dx;
    int16_t mouse_dy;
    bool mouse[5];
} BgInput;

struct BgPlayer {
    float x, y, z;
    float vx, vy, vz;
    float yaw, pitch;
    bool on_ground;
    int health;
    int selected;
    float invuln;
    BgStack inventory[INVENTORY_SLOTS];
};

bool player_near_table(const World *world, const BgPlayer *player);
void player_init(BgPlayer *player);
void player_update(BgPlayer *player, float dt, const BgInput *input, const World *world);
bool player_raycast(const BgPlayer *player, const World *world, float max_dist,
                    int *bx, int *by, int *bz, BlockId *type, int normal[3]);
bool player_place(BgPlayer *player, World *world, BlockId block, int *px, int *py, int *pz);

/* ---- Save ------------------------------------------------------------ */

bool save_world(const char *path, const World *world, const BgPlayer *player);
bool load_world(const char *path, World *world, BgPlayer *player);

#endif /* BLOCKFORGE_C_BG_H */
