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
#define CINDERHOLD_CEILING 78
#define CINDERHOLD_TOP 96
#define SEA_LEVEL 34

/* The Cinderhold's arena sits on a fixed promontory so the gate can be found
 * again from either side of a save. */
#define CINDERHOLD_FLOOR 34
#define CINDERHOLD_ARENA_HALF 24

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
    BG_TORCH,
    BG_CHEST,
    BG_CINDER_GATE,
    BG_BLOCK_COUNT
} BlockId;

typedef enum {
    BG_UPPERWORLD = 0,
    BG_UNDERWORLD = 1,
    BG_CINDERHOLD = 2
} Dimension;

typedef struct BgPlayer BgPlayer;

const char *dimension_name(Dimension dimension);

bool block_collidable(BlockId block);
bool block_liquid(BlockId block);
bool block_portal(BlockId block);
/* The Emberheart's gate into the Cinderhold; walking into it travels there. */
bool block_cinder_gate(BlockId block);
bool block_breakable(BlockId block);

/* Atlas tile index for one block face; face order is +X, -X, +Y, -Y, +Z, -Z. */
int block_tile(BlockId block, int face);

typedef struct Chunk {
    BlockId blocks[CHUNK_VOLUME];
    /* Sky light in the high nibble, block light in the low one. Derived data:
     * it is rebuilt from the blocks whenever a save is loaded. */
    uint8_t light[CHUNK_VOLUME];
    int cx, cy, cz;
} Chunk;

typedef struct ChunkSlot {
    uint64_t key;
    Chunk chunk;
    bool used;
} ChunkSlot;

#define CHEST_SLOTS 27

typedef struct BgContainer BgContainer;

typedef struct World {
    ChunkSlot *slots;
    size_t capacity;
    size_t count;
    Dimension dimension;
    uint64_t seed;
    struct BgContainer *containers;
    size_t container_count;
    size_t container_capacity;
    /* Paused while a whole region is being generated or rebuilt, so the light
     * flood fill runs once instead of once per block. */
    bool defer_light;
    float time_of_day;   /* 0 = dawn, 0.25 = noon, 0.5 = dusk, 0.75 = midnight */
    uint8_t quest_stage;
    bool boss_defeated;
    /* The Emberheart's gate has been lit and the Cinderhold's tyrant put down. */
    bool cinder_open;
    bool cinder_boss_defeated;
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

/* ---- Lighting -------------------------------------------------------- */

/* How much light a block gives off, 0 for none. */
int block_light_emit(BlockId block);

/* Combined sky/block light (high nibble / low nibble) at a cell. Unloaded
 * cells report full sky light so the edge of the view is never black. */
uint8_t world_light_at(const World *world, int x, int y, int z);

/* Rebuilds sky and block light for everything inside the box. */
void world_relight(World *world, int x0, int z0, int x1, int z1);

/* Rebuilds light for every chunk currently loaded. */
void world_relight_all(World *world);

/* ---- Chests ---------------------------------------------------------- */

BgContainer *world_container_at(World *world, int x, int y, int z, bool create);
void world_container_remove(World *world, int x, int y, int z);

/* Open, dry ground near the origin, where a fresh Upperworld begins. */
void world_spawn_site(const World *world, int *spawn_x, int *spawn_z);

/* Iteration order is unspecified; entries stay valid until the world mutates. */
typedef struct WorldIter {
    World *world;
    size_t index;
} WorldIter;

Chunk *world_iter_next(WorldIter *iter);

/* ---- Rift ------------------------------------------------------------ */

typedef enum {
    RIFT_NO_FRAME = 0,
    RIFT_OPENED,
    RIFT_ALREADY_OPEN
} RiftFire;

/* Locates the 4x5 voidstone frame around a block (a frame block or a block in
 * its opening) and reports its minimum corner and axis (0 = along X, 1 = along Z). */
bool rift_frame_at(const World *world, int x, int y, int z, int *cx, int *cy, int *cz,
                   int *axis);

/* Fills a complete frame's 2x3 opening with portal blocks. */
RiftFire rift_ignite(World *world, int x, int y, int z, int *cx, int *cy, int *cz, int *axis);

/* Carves and builds a lit twin frame at a known corner, used for the far side. */
void rift_build(World *world, int cx, int cy, int cz, int axis);

/* Strikes the Emberheart against a voidstone frame: the 2x3 opening fills with
 * cinder gate blocks instead of rift portal. False when no hollow frame is
 * there. */
bool cinder_gate_open(World *world, int x, int y, int z);

/* Raises the Cinderhold's arena: a paved promontory over the magma sea, walled
 * in, with its gate set into the southern wall and four lit pillars. */
void cinderhold_build_arena(World *world);

/* ---- Items and inventory --------------------------------------------- */

/* Nine hotbar slots on top, eighteen backpack slots behind them. */
#define INVENTORY_SLOTS 27
#define HOTBAR_SLOTS 9
#define MAX_STACK 64

/* Item ids below BG_BLOCK_COUNT are the placeable blocks; the crafting-only
 * items follow them. */
typedef enum {
    BG_ITEM_STICK = BG_BLOCK_COUNT,
    BG_ITEM_EMBER_SHARD,
    BG_ITEM_ASH_DUST,
    BG_ITEM_MEAT,
    BG_ITEM_WOOD_PICK,
    BG_ITEM_STONE_PICK,
    BG_ITEM_EMBER_PICK,
    BG_ITEM_RIFT_KEY,
    BG_ITEM_EMBERHEART,
    BG_ITEM_COUNT
} ItemId;

typedef struct BgStack {
    uint16_t item;
    uint8_t count;
    /* Points of wear on a tool; 0 is fresh. */
    uint8_t damage;
} BgStack;

/* A chest's contents live beside the world rather than in a chunk, so they
 * survive the block staying put and are saved in one small list. */
typedef struct BgContainer {
    int x, y, z;
    BgStack slots[CHEST_SLOTS];
} BgContainer;

const char *item_name(uint16_t item);
bool item_block(uint16_t item, BlockId *block);

/* What breaking a block puts in the inventory; false when it drops nothing. */
bool block_drop(BlockId block, uint16_t *item, int *count);

/* ---- Tools ----------------------------------------------------------- */

/* Mining tiers: 0 is bare hands, pickaxes are 1 (wood), 2 (stone), 3 (ember).
 * A block with a tier requirement above the held pick's tier cannot be mined. */
int item_pick_tier(uint16_t item);
int block_pick_tier(BlockId block);

/* Seconds a block takes at bare-hand speed; 0 for blocks that never break. */
float block_hardness(BlockId block);

/* How much faster than bare hands the held pick is; 1.0 when it is no pick. */
float item_pick_speed(uint16_t item);

/* Stack limit (1 for tools) and the wear a fresh tool can take. */
int item_max_stack(uint16_t item);
int item_durability_max(uint16_t item);

/* One point of wear; true when the tool just broke. */
bool item_wear(BgStack *stack, int amount);

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
    MOB_CINDER_ALPHA,
    MOB_CINDER_SOVEREIGN,
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
    /* Crouched before a floor slam lands; the player can step clear of it. */
    float windup;
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

/* Places the Cinder Alpha near the player (Underworld only); false when there
 * is no room for it. */
bool mobs_spawn_boss(BgMobWorld *mobs, World *world, float x, float y, float z);

/* Places the Cinderhold's sovereign at a spot in the arena. */
bool mobs_spawn_cinder_boss(BgMobWorld *mobs, World *world, float x, float y, float z);
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
    float light;
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
    bool mouse[5];      /* pressed since the last frame */
    bool mouse_held[5]; /* still down, for held-to-mine */
} BgInput;

struct BgPlayer {
    float x, y, z;
    float vx, vy, vz;
    float yaw, pitch;
    bool on_ground;
    int health;
    int selected;
    float invuln;
    /* Transient state: how far the current fall has dropped, and how long the
     * head has been under water. Neither is saved. */
    float fall_distance;
    float drown_timer;
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

/* ---- Dropped items --------------------------------------------------- */

#define MAX_DROPS 64

typedef struct BgDrop {
    float x, y, z;
    float vx, vy, vz;
    float age;
    uint16_t item;
    uint8_t count;
    bool alive;
} BgDrop;

/* Throws a stack on the ground in front of the player. */
bool world_drop_item(BgDrop *drops, int *drop_count, const BgPlayer *player,
                     uint16_t item, int count);

/* Applies gravity, picks items up when the player is close. */
void drops_update(BgDrop *drops, int *drop_count, const World *world, BgPlayer *player,
                  float dt);

#endif /* BLOCKFORGE_C_BG_H */
