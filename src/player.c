#include "bg.h"

#include <GLFW/glfw3.h>

#include <math.h>

#define EYE_HEIGHT 1.62f
#define GRAVITY 24.0f
#define WALK_SPEED 4.5f
#define SPRINT_SPEED 7.0f
#define JUMP_SPEED 8.5f
#define PLAYER_HALF_WIDTH 0.3f
#define PLAYER_HEIGHT 1.8f

static bool solid_at(const World *world, int x, int y, int z) {
    return block_collidable(world_get_block(world, x, y, z));
}

/* True when the player's box would intersect a collidable block at this spot. */
static bool blocked_at(const World *world, float x, float feet_y, float z) {
    const int x0 = (int)floorf(x - PLAYER_HALF_WIDTH);
    const int x1 = (int)floorf(x + PLAYER_HALF_WIDTH);
    const int z0 = (int)floorf(z - PLAYER_HALF_WIDTH);
    const int z1 = (int)floorf(z + PLAYER_HALF_WIDTH);
    const int y0 = (int)floorf(feet_y + 0.001f);
    const int y1 = (int)floorf(feet_y + PLAYER_HEIGHT - 0.001f);

    for (int by = y0; by <= y1; ++by) {
        for (int bz = z0; bz <= z1; ++bz) {
            for (int bx = x0; bx <= x1; ++bx) {
                if (solid_at(world, bx, by, bz)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void player_init(BgPlayer *player) {
    player->x = 0.5f;
    player->y = 80.0f;
    player->z = 0.5f;
    player->vx = player->vy = player->vz = 0.0f;
    player->yaw = 0.0f;
    player->pitch = 0.0f;
    player->on_ground = false;
    player->health = 20;
    player->selected = 0;
    player->invuln = 0.0f;

    /* A small starter kit: enough timber to reach a crafting table, then
     * everything else has to be gathered. */
    inventory_clear(player->inventory);
    inventory_add(player->inventory, BG_PLANKS, 12);
    inventory_add(player->inventory, BG_ITEM_STICK, 6);
}

void player_update(BgPlayer *player, float dt, const BgInput *input, const World *world) {
    if (dt > 0.05f) {
        dt = 0.05f;
    }
    if (!player->on_ground) {
        player->vy -= GRAVITY * dt;
    }

    const float forward_x = cosf(player->yaw);
    const float forward_z = sinf(player->yaw);
    const float right_x = -sinf(player->yaw);
    const float right_z = cosf(player->yaw);

    float wish_x = 0.0f;
    float wish_z = 0.0f;
    if (input->keys[GLFW_KEY_W]) { wish_x += forward_x; wish_z += forward_z; }
    if (input->keys[GLFW_KEY_S]) { wish_x -= forward_x; wish_z -= forward_z; }
    if (input->keys[GLFW_KEY_D]) { wish_x += right_x; wish_z += right_z; }
    if (input->keys[GLFW_KEY_A]) { wish_x -= right_x; wish_z -= right_z; }

    const float length = sqrtf(wish_x * wish_x + wish_z * wish_z);
    if (length > 0.0001f) {
        wish_x /= length;
        wish_z /= length;
    }

    const float speed = input->keys[GLFW_KEY_LEFT_SHIFT] ? SPRINT_SPEED : WALK_SPEED;
    player->vx = wish_x * speed;
    player->vz = wish_z * speed;

    if (input->keys[GLFW_KEY_SPACE] && player->on_ground) {
        player->vy = JUMP_SPEED;
        player->on_ground = false;
    }

    const float step_x = player->vx * dt;
    if (step_x != 0.0f && !blocked_at(world, player->x + step_x, player->y, player->z)) {
        player->x += step_x;
    }
    const float step_z = player->vz * dt;
    if (step_z != 0.0f && !blocked_at(world, player->x, player->y, player->z + step_z)) {
        player->z += step_z;
    }

    player->on_ground = false;
    const float step_y = player->vy * dt;
    if (step_y != 0.0f) {
        if (blocked_at(world, player->x, player->y + step_y, player->z)) {
            if (step_y < 0.0f) {
                player->on_ground = true;
            }
            player->vy = 0.0f;
        } else {
            player->y += step_y;
        }
    }

    /* Snap onto the block under the feet when standing still and falling. */
    if (!player->on_ground && player->vy <= 0.0f) {
        const int bx = (int)floorf(player->x);
        const int bz = (int)floorf(player->z);
        const int feet = (int)floorf(player->y);
        for (int by = feet; by >= feet - 2 && by >= 0; --by) {
            if (solid_at(world, bx, by, bz) && player->y <= (float)(by + 1) + 0.02f) {
                player->y = (float)(by + 1);
                player->vy = 0.0f;
                player->on_ground = true;
                break;
            }
        }
    }

    if (player->y < 0.0f) {
        player->y = 0.0f;
        player->vy = 0.0f;
        player->on_ground = true;
    }
}

bool player_raycast(const BgPlayer *player, const World *world, float max_dist,
                    int *bx, int *by, int *bz, BlockId *type, int normal[3]) {
    const float dir_x = cosf(player->pitch) * cosf(player->yaw);
    const float dir_y = sinf(player->pitch);
    const float dir_z = cosf(player->pitch) * sinf(player->yaw);

    const float eye_x = player->x;
    const float eye_y = player->y + EYE_HEIGHT;
    const float eye_z = player->z;

    int prev_x = (int)floorf(eye_x);
    int prev_y = (int)floorf(eye_y);
    int prev_z = (int)floorf(eye_z);

    for (float t = 0.0f; t <= max_dist; t += 0.05f) {
        const int cx = (int)floorf(eye_x + dir_x * t);
        const int cy = (int)floorf(eye_y + dir_y * t);
        const int cz = (int)floorf(eye_z + dir_z * t);

        const BlockId hit = world_get_block(world, cx, cy, cz);
        if (hit != BG_AIR && hit != BG_WATER) {
            *bx = cx;
            *by = cy;
            *bz = cz;
            *type = hit;
            normal[0] = prev_x - cx;
            normal[1] = prev_y - cy;
            normal[2] = prev_z - cz;
            return true;
        }
        prev_x = cx;
        prev_y = cy;
        prev_z = cz;
    }
    return false;
}

bool player_place(BgPlayer *player, World *world, BlockId block, int *px, int *py, int *pz) {
    int bx = 0, by = 0, bz = 0;
    BlockId type = BG_AIR;
    int normal[3] = { 0, 0, 0 };
    if (!player_raycast(player, world, 6.0f, &bx, &by, &bz, &type, normal)) {
        return false;
    }

    const int tx = bx + normal[0];
    const int ty = by + normal[1];
    const int tz = bz + normal[2];
    if (ty < 0 || ty >= UNDERWORLD_TOP) {
        return false;
    }
    const BlockId existing = world_get_block(world, tx, ty, tz);
    if (existing != BG_AIR && existing != BG_WATER) {
        return false;
    }

    /* Never place a block inside the player. */
    if (block_collidable(block)) {
        const float half = PLAYER_HALF_WIDTH;
        if ((float)tx + 1.0f > player->x - half && (float)tx < player->x + half &&
            (float)tz + 1.0f > player->z - half && (float)tz < player->z + half &&
            (float)ty + 1.0f > player->y && (float)ty < player->y + PLAYER_HEIGHT) {
            return false;
        }
    }

    world_set_block(world, tx, ty, tz, block);
    if (px) *px = tx;
    if (py) *py = ty;
    if (pz) *pz = tz;
    return true;
}
