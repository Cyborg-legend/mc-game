#include "bg.h"

#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC 0x43524642u /* "BFRC" */
#define SAVE_VERSION 1u

static void chunk_key_parts(uint64_t key, int *cx, int *cy, int *cz) {
    *cx = (int32_t)((uint32_t)(key & 0x1FFFFFu) << 11) >> 11;
    *cy = (int32_t)((uint32_t)((key >> 21) & 0x1FFFFFu) << 11) >> 11;
    *cz = (int32_t)((uint32_t)((key >> 42) & 0x1FFFFFu) << 11) >> 11;
}

bool save_world(const char *path, const World *world, const BgPlayer *player) {
    FILE *file = fopen(path, "wb");
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
        fwrite(slot->chunk.blocks, 1, sizeof(slot->chunk.blocks), file);
    }

    fwrite(player->inventory, 1, sizeof(player->inventory), file);
    fwrite(&player->selected, sizeof(player->selected), 1, file);
    fwrite(&player->health, sizeof(player->health), 1, file);
    fwrite(&player->x, sizeof(player->x), 1, file);
    fwrite(&player->y, sizeof(player->y), 1, file);
    fwrite(&player->z, sizeof(player->z), 1, file);
    fwrite(&player->yaw, sizeof(player->yaw), 1, file);
    fwrite(&player->pitch, sizeof(player->pitch), 1, file);

    const bool ok = ferror(file) == 0;
    fclose(file);
    return ok;
}

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
    if (magic != SAVE_MAGIC || version != SAVE_VERSION ||
        dimension > (uint8_t)BG_UNDERWORLD || chunk_count > 200000u) {
        fclose(file);
        return false;
    }

    world_clear(world);
    world_set_seed(world, seed);
    world_set_dimension(world, (Dimension)dimension);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        uint64_t key = 0;
        if (fread(&key, sizeof(key), 1, file) != 1) {
            fclose(file);
            return false;
        }
        int cx = 0, cy = 0, cz = 0;
        chunk_key_parts(key, &cx, &cy, &cz);
        Chunk *chunk = world_chunk(world, cx, cy, cz, true);
        if (!chunk) {
            fclose(file);
            return false;
        }
        if (fread(chunk->blocks, 1, sizeof(chunk->blocks), file) != sizeof(chunk->blocks)) {
            fclose(file);
            return false;
        }
    }

    if (fread(player->inventory, 1, sizeof(player->inventory), file) != sizeof(player->inventory)) {
        fclose(file);
        return false;
    }
    if (fread(&player->selected, sizeof(player->selected), 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (fread(&player->health, sizeof(player->health), 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (player->selected < 0 || player->selected > 8) {
        player->selected = 0;
    }

    /* Older saves end at the counts; a short position read just keeps the
     * spawn point the caller already set. */
    (void)fread(&player->x, sizeof(player->x), 1, file);
    (void)fread(&player->y, sizeof(player->y), 1, file);
    (void)fread(&player->z, sizeof(player->z), 1, file);
    (void)fread(&player->yaw, sizeof(player->yaw), 1, file);
    (void)fread(&player->pitch, sizeof(player->pitch), 1, file);

    fclose(file);
    return true;
}
