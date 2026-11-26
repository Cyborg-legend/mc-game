#include "bg.h"

#include <math.h>
#include <string.h>

static bool drop_solid_at(const World *world, int x, int y, int z) {
    return block_collidable(world_get_block(world, x, y, z));
}

bool world_drop_item(BgDrop *drops, int *drop_count, const BgPlayer *player, uint16_t item,
                     int count) {
    if (!drops || !drop_count || !player || item == BG_AIR || count <= 0) {
        return false;
    }

    int slot = -1;
    for (int i = 0; i < MAX_DROPS; ++i) {
        if (!drops[i].alive) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return false;
    }

    BgDrop *drop = &drops[slot];
    memset(drop, 0, sizeof(*drop));
    drop->alive = true;
    drop->item = item;
    drop->count = (uint8_t)(count > MAX_STACK ? MAX_STACK : count);
    drop->x = player->x + cosf(player->yaw) * 0.7f;
    drop->y = player->y + 1.3f;
    drop->z = player->z + sinf(player->yaw) * 0.7f;
    drop->vx = cosf(player->yaw) * 2.4f;
    drop->vy = 2.4f;
    drop->vz = sinf(player->yaw) * 2.4f;

    if (*drop_count <= slot) {
        *drop_count = slot + 1;
    }
    return true;
}

void drops_update(BgDrop *drops, int *drop_count, const World *world, BgPlayer *player,
                  float dt) {
    if (!drops || !drop_count || !world || !player) {
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    for (int i = 0; i < *drop_count && i < MAX_DROPS; ++i) {
        BgDrop *drop = &drops[i];
        if (!drop->alive) {
            continue;
        }
        drop->age += dt;

        drop->vy -= 22.0f * dt;
        if (drop->vy < -22.0f) {
            drop->vy = -22.0f;
        }

        const float nx = drop->x + drop->vx * dt;
        if (!drop_solid_at(world, (int)floorf(nx), (int)floorf(drop->y + 0.1f),
                           (int)floorf(drop->z))) {
            drop->x = nx;
        } else {
            drop->vx = 0.0f;
        }
        const float nz = drop->z + drop->vz * dt;
        if (!drop_solid_at(world, (int)floorf(drop->x), (int)floorf(drop->y + 0.1f),
                           (int)floorf(nz))) {
            drop->z = nz;
        } else {
            drop->vz = 0.0f;
        }

        const float ny = drop->y + drop->vy * dt;
        if (drop->vy <= 0.0f) {
            const int bx = (int)floorf(drop->x);
            const int bz = (int)floorf(drop->z);
            const int by = (int)floorf(ny);
            if (drop_solid_at(world, bx, by, bz)) {
                drop->y = (float)(by + 1) + 0.01f;
                drop->vy = 0.0f;
                drop->vx *= 0.72f;
                drop->vz *= 0.72f;
            } else {
                drop->y = ny;
            }
        } else if (drop_solid_at(world, (int)floorf(drop->x), (int)floorf(ny + 0.3f),
                                 (int)floorf(drop->z))) {
            drop->vy = 0.0f;
        } else {
            drop->y = ny;
        }
        if (drop->y < 0.0f) {
            drop->y = 0.0f;
            drop->vy = 0.0f;
        }

        const float dx = player->x - drop->x;
        const float dy = (player->y + 0.9f) - drop->y;
        const float dz = player->z - drop->z;
        if (drop->age > 0.35f && dx * dx + dy * dy + dz * dz < 2.4f * 2.4f) {
            if (inventory_add(player->inventory, drop->item, drop->count)) {
                drop->alive = false;
            }
        }

        if (drop->age > 180.0f) {
            drop->alive = false;
        }
    }
}
