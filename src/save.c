#include "bg.h"

#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC 0x43524642u /* "BFRC" */
#define SAVE_VERSION 3u
#define LEGACY_VERSION 2u
#define OLDEST_VERSION 1u

static void chunk_key_parts(uint64_t key, int *cx, int *cy, int *cz) {
    *cx = (int32_t)((uint32_t)(key & 0x1FFFFFu) << 11) >> 11;
    *cy = (int32_t)((uint32_t)((key >> 21) & 0x1FFFFFu) << 11) >> 11;
    *cz = (int32_t)((uint32_t)((key >> 42) & 0x1FFFFFu) << 11) >> 11;
}

/* ------------------------------------------------------------ inventory */

static void write_inventory(FILE *file, const BgStack *slots) {
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        const uint16_t item = slots[i].count ? slots[i].item : BG_AIR;
        const uint8_t count = slots[i].count;
        const uint8_t damage = slots[i].damage;
        fwrite(&item, sizeof(item), 1, file);
        fwrite(&count, sizeof(count), 1, file);
        fwrite(&damage, sizeof(damage), 1, file);
    }
}

static bool read_inventory(FILE *file, BgStack *slots, int slot_count, bool has_damage) {
    inventory_clear(slots);
    for (int i = 0; i < slot_count && i < INVENTORY_SLOTS; ++i) {
        uint16_t item = 0;
        uint8_t count = 0;
        uint8_t damage = 0;
        if (fread(&item, sizeof(item), 1, file) != 1 ||
            fread(&count, sizeof(count), 1, file) != 1) {
            return false;
        }
        if (has_damage && fread(&damage, sizeof(damage), 1, file) != 1) {
            return false;
        }
        if (item >= BG_ITEM_COUNT || count == 0) {
            continue;
        }
        slots[i].item = item;
        slots[i].count = count > MAX_STACK ? MAX_STACK : count;
        slots[i].damage = damage;
    }
    return true;
}

/* Version 1 stored a plain BlockId per slot with no counts. */
static bool read_legacy_inventory(FILE *file, BgStack *slots) {
    inventory_clear(slots);
    for (int i = 0; i < 9; ++i) {
        uint32_t block = 0;
        if (fread(&block, sizeof(block), 1, file) != 1) {
            return false;
        }
        if (block == 0 || block >= BG_BLOCK_COUNT) {
            continue;
        }
        slots[i].item = (uint16_t)block;
        slots[i].count = 1;
    }
    return true;
}

/* -------------------------------------------------------------- chunks */

/* Chunks are mostly one block repeated, so v3 stores runs of identical ids
 * instead of 4096 raw bytes. */
static void write_chunk(FILE *file, const Chunk *chunk) {
    uint32_t runs = 0;
    for (int i = 0; i < CHUNK_VOLUME; ++i) {
        if (i == 0 || chunk->blocks[i] != chunk->blocks[i - 1]) {
            runs++;
        }
    }
    fwrite(&runs, sizeof(runs), 1, file);
    for (int i = 0; i < CHUNK_VOLUME;) {
        const uint8_t block = (uint8_t)chunk->blocks[i];
        int run = 1;
        while (i + run < CHUNK_VOLUME && chunk->blocks[i + run] == chunk->blocks[i]) {
            run++;
        }
        const uint16_t count = (uint16_t)run;
        fwrite(&count, sizeof(count), 1, file);
        fwrite(&block, sizeof(block), 1, file);
        i += run;
    }
}

static bool read_chunk(FILE *file, uint32_t version, Chunk *chunk) {
    if (version >= 3u) {
        uint32_t runs = 0;
        if (fread(&runs, sizeof(runs), 1, file) != 1 || runs > CHUNK_VOLUME) {
            return false;
        }
        int filled = 0;
        for (uint32_t i = 0; i < runs; ++i) {
            uint16_t count = 0;
            uint8_t block = 0;
            if (fread(&count, sizeof(count), 1, file) != 1 ||
                fread(&block, sizeof(block), 1, file) != 1) {
                return false;
            }
            if (count == 0 || filled + (int)count > CHUNK_VOLUME ||
                block >= BG_BLOCK_COUNT) {
                return false;
            }
            for (int n = 0; n < (int)count; ++n) {
                chunk->blocks[filled++] = (BlockId)block;
            }
        }
        if (filled != CHUNK_VOLUME) {
            return false;
        }
        return true;
    }

    return fread(chunk->blocks, 1, sizeof(chunk->blocks), file) == sizeof(chunk->blocks);
}

/* -------------------------------------------------------------- writing */

bool save_world(const char *path, const World *world, const BgPlayer *player) {
    /* Write beside the target first, then swap it in: a crash halfway through
     * leaves the previous save intact instead of a truncated file. */
    char temporary[160];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        return false;
    }

    const uint32_t magic = SAVE_MAGIC;
    const uint32_t version = SAVE_VERSION;
    const uint64_t seed = world->seed;
    const uint8_t dimension = (uint8_t)world->dimension;
    const uint32_t chunk_count = (uint32_t)world->count;

    fwrite(&magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&seed, sizeof(seed), 1, file);
    fwrite(&dimension, sizeof(dimension), 1, file);
    fwrite(&chunk_count, sizeof(chunk_count), 1, file);

    for (size_t i = 0; i < world->capacity; ++i) {
        const ChunkSlot *slot = &world->slots[i];
        if (!slot->used) {
            continue;
        }
        fwrite(&slot->key, sizeof(slot->key), 1, file);
        write_chunk(file, &slot->chunk);
    }

    write_inventory(file, player->inventory);
    fwrite(&player->selected, sizeof(player->selected), 1, file);
    const int health = player->health < 0 ? 0 : (player->health > 20 ? 20 : player->health);
    fwrite(&health, sizeof(health), 1, file);
    fwrite(&player->x, sizeof(player->x), 1, file);
    fwrite(&player->y, sizeof(player->y), 1, file);
    fwrite(&player->z, sizeof(player->z), 1, file);
    fwrite(&player->yaw, sizeof(player->yaw), 1, file);
    fwrite(&player->pitch, sizeof(player->pitch), 1, file);

    fwrite(&world->time_of_day, sizeof(world->time_of_day), 1, file);
    fwrite(&world->quest_stage, sizeof(world->quest_stage), 1, file);
    const uint8_t boss = world->boss_defeated ? 1u : 0u;
    fwrite(&boss, sizeof(boss), 1, file);

    const uint32_t container_count = (uint32_t)world->container_count;
    fwrite(&container_count, sizeof(container_count), 1, file);
    for (size_t i = 0; i < world->container_count; ++i) {
        const BgContainer *container = &world->containers[i];
        fwrite(&container->x, sizeof(container->x), 1, file);
        fwrite(&container->y, sizeof(container->y), 1, file);
        fwrite(&container->z, sizeof(container->z), 1, file);
        for (int s = 0; s < CHEST_SLOTS; ++s) {
            const uint16_t item = container->slots[s].count ? container->slots[s].item
                                                            : BG_AIR;
            const uint8_t count = container->slots[s].count;
            const uint8_t damage = container->slots[s].damage;
            fwrite(&item, sizeof(item), 1, file);
            fwrite(&count, sizeof(count), 1, file);
            fwrite(&damage, sizeof(damage), 1, file);
        }
    }

    const bool ok = ferror(file) == 0;
    if (fclose(file) != 0 || !ok) {
        remove(temporary);
        return false;
    }

    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        return false;
    }
    return true;
}

/* -------------------------------------------------------------- loading */

bool load_world(const char *path, World *world, BgPlayer *player) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t seed = 0;
    uint8_t dimension = 0;
    uint32_t chunk_count = 0;

    if (fread(&magic, sizeof(magic), 1, file) != 1 ||
        fread(&version, sizeof(version), 1, file) != 1 ||
        fread(&seed, sizeof(seed), 1, file) != 1 ||
        fread(&dimension, sizeof(dimension), 1, file) != 1 ||
        fread(&chunk_count, sizeof(chunk_count), 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (magic != SAVE_MAGIC ||
        (version != OLDEST_VERSION && version != LEGACY_VERSION && version != SAVE_VERSION) ||
        dimension > (uint8_t)BG_UNDERWORLD || chunk_count > 200000u) {
        fclose(file);
        return false;
    }

    world_clear(world);
    world_set_seed(world, seed);
    world_set_dimension(world, (Dimension)dimension);
    world->defer_light = true;

    for (uint32_t i = 0; i < chunk_count; ++i) {
        uint64_t key = 0;
        if (fread(&key, sizeof(key), 1, file) != 1) {
            world->defer_light = false;
            fclose(file);
            return false;
        }
        int cx = 0, cy = 0, cz = 0;
        chunk_key_parts(key, &cx, &cy, &cz);
        Chunk *chunk = world_chunk(world, cx, cy, cz, true);
        if (!chunk || !read_chunk(file, version, chunk)) {
            world->defer_light = false;
            fclose(file);
            return false;
        }
    }

    bool inventory_ok = false;
    if (version == 1u) {
        inventory_ok = read_legacy_inventory(file, player->inventory);
    } else if (version == 2u) {
        inventory_ok = read_inventory(file, player->inventory, 9, false);
    } else {
        inventory_ok = read_inventory(file, player->inventory, INVENTORY_SLOTS, true);
    }
    if (!inventory_ok) {
        world->defer_light = false;
        fclose(file);
        return false;
    }
    if (fread(&player->selected, sizeof(player->selected), 1, file) != 1) {
        world->defer_light = false;
        fclose(file);
        return false;
    }
    if (fread(&player->health, sizeof(player->health), 1, file) != 1) {
        world->defer_light = false;
        fclose(file);
        return false;
    }
    if (player->selected < 0 || player->selected >= HOTBAR_SLOTS) {
        player->selected = 0;
    }
    if (player->health <= 0) {
        player->health = 20;
    }

    /* Older saves end at the counts; a short position read just keeps the
     * spawn point the caller already set. */
    (void)fread(&player->x, sizeof(player->x), 1, file);
    (void)fread(&player->y, sizeof(player->y), 1, file);
    (void)fread(&player->z, sizeof(player->z), 1, file);
    (void)fread(&player->yaw, sizeof(player->yaw), 1, file);
    (void)fread(&player->pitch, sizeof(player->pitch), 1, file);

    world->time_of_day = 0.18f;
    world->quest_stage = 0;
    world->boss_defeated = false;
    if (version >= 3u) {
        if (fread(&world->time_of_day, sizeof(world->time_of_day), 1, file) != 1) {
            world->time_of_day = 0.18f;
        }
        uint8_t quest = 0;
        uint8_t boss = 0;
        (void)fread(&quest, sizeof(quest), 1, file);
        (void)fread(&boss, sizeof(boss), 1, file);
        world->quest_stage = quest > 4u ? 4u : quest;
        world->boss_defeated = boss != 0;

        uint32_t container_count = 0;
        if (fread(&container_count, sizeof(container_count), 1, file) == 1 &&
            container_count <= 100000u) {
            for (uint32_t i = 0; i < container_count; ++i) {
                int x = 0, y = 0, z = 0;
                if (fread(&x, sizeof(x), 1, file) != 1 ||
                    fread(&y, sizeof(y), 1, file) != 1 ||
                    fread(&z, sizeof(z), 1, file) != 1) {
                    break;
                }
                BgContainer *container = world_container_at(world, x, y, z, true);
                if (!container) {
                    break;
                }
                bool ends_early = false;
                for (int s = 0; s < CHEST_SLOTS; ++s) {
                    uint16_t item = 0;
                    uint8_t count = 0;
                    uint8_t damage = 0;
                    if (fread(&item, sizeof(item), 1, file) != 1 ||
                        fread(&count, sizeof(count), 1, file) != 1 ||
                        fread(&damage, sizeof(damage), 1, file) != 1) {
                        ends_early = true;
                        break;
                    }
                    if (item >= BG_ITEM_COUNT || count == 0) {
                        continue;
                    }
                    container->slots[s].item = item;
                    container->slots[s].count = count > MAX_STACK ? MAX_STACK : count;
                    container->slots[s].damage = damage;
                }
                if (ends_early) {
                    break;
                }
            }
        }
    }

    fclose(file);
    world->defer_light = false;
    /* Light is derived data, so it is rebuilt from the blocks just read. */
    world_relight_all(world);
    return true;
}
