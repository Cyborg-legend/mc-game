#include "bg.h"

#include <math.h>
#include <string.h>

#define MOB_GRAVITY 24.0f
#define MOB_MAX_HOSTILE 8
#define MOB_MAX_GRAZERS 6
#define MOB_DESPAWN_DISTANCE 90.0f

/* ----------------------------------------------------------- properties */

const char *mob_name(MobType type) {
    switch (type) {
        case MOB_GRAZER:       return "Grazer";
        case MOB_CINDER_HOUND: return "Cinder Hound";
        case MOB_ASH_WISP:     return "Ash Wisp";
        default:               return "Mob";
    }
}

float mob_half_width(MobType type) {
    switch (type) {
        case MOB_GRAZER:       return 0.45f;
        case MOB_CINDER_HOUND: return 0.40f;
        case MOB_ASH_WISP:     return 0.35f;
        default:               return 0.4f;
    }
}

float mob_height(MobType type) {
    switch (type) {
        case MOB_GRAZER:       return 1.25f;
        case MOB_CINDER_HOUND: return 1.00f;
        case MOB_ASH_WISP:     return 0.90f;
        default:               return 1.0f;
    }
}

bool mob_hostile(MobType type) {
    return type == MOB_CINDER_HOUND || type == MOB_ASH_WISP;
}

static int mob_health(MobType type) {
    switch (type) {
        case MOB_GRAZER:       return 10;
        case MOB_CINDER_HOUND: return 14;
        case MOB_ASH_WISP:     return 8;
        default:               return 8;
    }
}

static int mob_contact_damage(MobType type) {
    switch (type) {
        case MOB_CINDER_HOUND: return 3;
        case MOB_ASH_WISP:     return 2;
        default:               return 0;
    }
}

static float mob_speed(MobType type) {
    switch (type) {
        case MOB_GRAZER:       return 1.35f;
        case MOB_CINDER_HOUND: return 3.10f;
        case MOB_ASH_WISP:     return 2.20f;
        default:               return 1.0f;
    }
}

/* ---------------------------------------------------------------- random */

static uint32_t next_rand(BgMobWorld *mobs) {
    uint32_t x = mobs->rng ? mobs->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mobs->rng = x;
    return x;
}

static float rand_unit(BgMobWorld *mobs) {
    return (float)(next_rand(mobs) & 0xFFFFFFu) / 16777215.0f;
}

/* --------------------------------------------------------------- physics */

static bool solid_at(const World *world, int x, int y, int z) {
    return block_collidable(world_get_block(world, x, y, z));
}

static bool blocked_at(const World *world, float x, float feet_y, float z, float half,
                       float height) {
    const int x0 = (int)floorf(x - half);
    const int x1 = (int)floorf(x + half);
    const int z0 = (int)floorf(z - half);
    const int z1 = (int)floorf(z + half);
    const int y0 = (int)floorf(feet_y + 0.001f);
    const int y1 = (int)floorf(feet_y + height - 0.001f);

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

/* Walks the mob along its wish direction, sliding on walls and hopping low
 * obstacles. This is the whole "pathing": no graph search, just steering with
 * obstacle probes, which is enough for open voxel terrain. */
static void move_ground_mob(BgMob *mob, const World *world, float wish_x, float wish_z,
                            float speed, float dt) {
    const float half = mob_half_width((MobType)mob->type);
    const float height = mob_height((MobType)mob->type);

    const float length = sqrtf(wish_x * wish_x + wish_z * wish_z);
    if (length > 0.0001f) {
        wish_x /= length;
        wish_z /= length;
    }
    mob->vx = wish_x * speed;
    mob->vz = wish_z * speed;

    bool bumped = false;
    const float step_x = mob->vx * dt;
    if (step_x != 0.0f) {
        if (!blocked_at(world, mob->x + step_x, mob->y, mob->z, half, height)) {
            mob->x += step_x;
        } else {
            bumped = true;
        }
    }
    const float step_z = mob->vz * dt;
    if (step_z != 0.0f) {
        if (!blocked_at(world, mob->x, mob->y, mob->z + step_z, half, height)) {
            mob->z += step_z;
        } else {
            bumped = true;
        }
    }
    if (length > 0.0001f) {
        mob->yaw = atan2f(wish_z, wish_x);
    }
    if (bumped && mob->on_ground) {
        mob->vy = 8.0f;
        mob->on_ground = false;
    }

    mob->vy -= MOB_GRAVITY * dt;
    if (mob->vy < -45.0f) {
        mob->vy = -45.0f;
    }
    const float step_y = mob->vy * dt;
    mob->on_ground = false;
    if (step_y != 0.0f) {
        if (blocked_at(world, mob->x, mob->y + step_y, mob->z, half, height)) {
            if (step_y < 0.0f) {
                mob->on_ground = true;
            }
            mob->vy = 0.0f;
        } else {
            mob->y += step_y;
        }
    }

    if (!mob->on_ground && mob->vy <= 0.0f) {
        const int bx = (int)floorf(mob->x);
        const int bz = (int)floorf(mob->z);
        const int feet = (int)floorf(mob->y);
        for (int by = feet; by >= feet - 2 && by >= 0; --by) {
            if (solid_at(world, bx, by, bz) && mob->y <= (float)(by + 1) + 0.02f) {
                mob->y = (float)(by + 1);
                mob->vy = 0.0f;
                mob->on_ground = true;
                break;
            }
        }
    }
    if (mob->y < 0.0f) {
        mob->alive = false;
    }
}

static void move_flying_mob(BgMob *mob, const World *world, float wish_x, float wish_z,
                            float target_y, float speed, float dt) {
    const float half = mob_half_width((MobType)mob->type);
    const float height = mob_height((MobType)mob->type);

    const float length = sqrtf(wish_x * wish_x + wish_z * wish_z);
    if (length > 0.0001f) {
        wish_x /= length;
        wish_z /= length;
        mob->vx = wish_x * speed;
        mob->vz = wish_z * speed;
        mob->yaw = atan2f(wish_z, wish_x);
    } else {
        mob->vx = 0.0f;
        mob->vz = 0.0f;
    }

    const float step_x = mob->vx * dt;
    if (step_x != 0.0f && !blocked_at(world, mob->x + step_x, mob->y, mob->z, half, height)) {
        mob->x += step_x;
    }
    const float step_z = mob->vz * dt;
    if (step_z != 0.0f && !blocked_at(world, mob->x, mob->y, mob->z + step_z, half, height)) {
        mob->z += step_z;
    }

    float rise = (target_y - mob->y) * 1.8f;
    if (rise > 2.4f) rise = 2.4f;
    if (rise < -2.4f) rise = -2.4f;
    mob->vy = rise;
    const float step_y = mob->vy * dt;
    if (step_y != 0.0f && !blocked_at(world, mob->x, mob->y + step_y, mob->z, half, height)) {
        mob->y += step_y;
    }
}

/* --------------------------------------------------------------- spawning */

void mobs_init(BgMobWorld *mobs) {
    memset(mobs, 0, sizeof(*mobs));
    mobs->rng = 0x9E3779B9u;
    mobs->spawn_timer = 2.0f;
}

void mobs_clear(BgMobWorld *mobs) {
    memset(mobs->mobs, 0, sizeof(mobs->mobs));
    mobs->count = 0;
    mobs->spawn_timer = 2.0f;
}

static BgMob *alloc_mob(BgMobWorld *mobs) {
    for (int i = 0; i < MAX_MOBS; ++i) {
        if (!mobs->mobs[i].alive) {
            return &mobs->mobs[i];
        }
    }
    return NULL;
}

static void place_mob(BgMob *mob, MobType type, float x, float y, float z,
                      BgMobWorld *mobs) {
    memset(mob, 0, sizeof(*mob));
    mob->type = (uint8_t)type;
    mob->alive = true;
    mob->x = x;
    mob->y = y;
    mob->z = z;
    mob->health = mob_health(type);
    mob->wander_timer = 0.5f + rand_unit(mobs) * 2.0f;
    mob->wander_yaw = rand_unit(mobs) * 6.2831853f;
    mob->attack_timer = 1.0f;
    mob->yaw = mob->wander_yaw;
    mob->on_ground = type != MOB_ASH_WISP;
}

static void try_spawn(BgMobWorld *mobs, World *world, const BgPlayer *player) {
    int grazers = 0;
    int hostiles = 0;
    for (int i = 0; i < MAX_MOBS; ++i) {
        if (!mobs->mobs[i].alive) {
            continue;
        }
        if (mobs->mobs[i].type == (uint8_t)MOB_GRAZER) {
            grazers++;
        } else {
            hostiles++;
        }
    }

    const bool underworld = world->dimension == BG_UNDERWORLD;
    if (underworld ? hostiles >= MOB_MAX_HOSTILE : grazers >= MOB_MAX_GRAZERS) {
        return;
    }

    for (int attempt = 0; attempt < 12; ++attempt) {
        const float angle = rand_unit(mobs) * 6.2831853f;
        const float distance = 14.0f + rand_unit(mobs) * 22.0f;
        const int x = (int)floorf(player->x + cosf(angle) * distance);
        const int z = (int)floorf(player->z + sinf(angle) * distance);
        const int surface = world_surface_y(world, x, z);
        if (surface < 4) {
            continue;
        }
        const BlockId ground = world_get_block(world, x, surface - 1, z);

        if (!underworld) {
            if (ground != BG_GRASS && ground != BG_DIRT) {
                continue;
            }
            if (solid_at(world, x, surface, z) || solid_at(world, x, surface + 1, z)) {
                continue;
            }
            BgMob *mob = alloc_mob(mobs);
            if (!mob) {
                return;
            }
            place_mob(mob, MOB_GRAZER, (float)x + 0.5f, (float)surface, (float)z + 0.5f,
                      mobs);
            return;
        }

        const bool floor_ok = ground == BG_EMBERSTONE || ground == BG_ASHSOIL ||
                              ground == BG_CINDERSTONE;
        if (!floor_ok) {
            continue;
        }
        BgMob *mob = alloc_mob(mobs);
        if (!mob) {
            return;
        }
        if (rand_unit(mobs) < 0.35f) {
            const int height = surface + 4 + (int)(rand_unit(mobs) * 3.0f);
            if (solid_at(world, x, height, z) || solid_at(world, x, surface + 1, z)) {
                continue;
            }
            place_mob(mob, MOB_ASH_WISP, (float)x + 0.5f, (float)height, (float)z + 0.5f,
                      mobs);
        } else {
            if (solid_at(world, x, surface, z) || solid_at(world, x, surface + 1, z)) {
                continue;
            }
            place_mob(mob, MOB_CINDER_HOUND, (float)x + 0.5f, (float)surface,
                      (float)z + 0.5f, mobs);
        }
        return;
    }
}

/* -------------------------------------------------------------- behaviour */

static void update_grazer(BgMob *mob, const World *world, const BgPlayer *player,
                          BgMobWorld *mobs, float dt) {
    const float dx = player->x - mob->x;
    const float dz = player->z - mob->z;
    const float distance = sqrtf(dx * dx + dz * dz);

    if (distance < 7.0f && fabsf(player->y - mob->y) < 4.0f) {
        /* Skittish: walk away from anything close. */
        const float inv = distance > 0.001f ? 1.0f / distance : 0.0f;
        move_ground_mob(mob, world, -dx * inv, -dz * inv, mob_speed(MOB_GRAZER) * 2.0f, dt);
        mob->idle = false;
        return;
    }

    mob->wander_timer -= dt;
    if (mob->wander_timer <= 0.0f) {
        mob->wander_timer = 2.5f + rand_unit(mobs) * 4.0f;
        mob->wander_yaw = rand_unit(mobs) * 6.2831853f;
        mob->idle = rand_unit(mobs) < 0.4f;
    }

    if (mob->idle) {
        move_ground_mob(mob, world, 0.0f, 0.0f, 0.0f, dt);
        return;
    }
    move_ground_mob(mob, world, cosf(mob->wander_yaw), sinf(mob->wander_yaw),
                    mob_speed(MOB_GRAZER), dt);
}

static void update_hound(BgMob *mob, const World *world, const BgPlayer *player,
                         BgMobWorld *mobs, float dt) {
    const float dx = player->x - mob->x;
    const float dz = player->z - mob->z;
    const float distance = sqrtf(dx * dx + dz * dz);

    if (distance < 20.0f && fabsf(player->y - mob->y) < 5.0f) {
        mob->wander_timer = 1.5f;
        if (distance > 1.1f) {
            const float inv = 1.0f / distance;
            move_ground_mob(mob, world, dx * inv, dz * inv, mob_speed(MOB_CINDER_HOUND), dt);
        } else {
            move_ground_mob(mob, world, 0.0f, 0.0f, 0.0f, dt);
        }
        return;
    }

    mob->wander_timer -= dt;
    if (mob->wander_timer <= 0.0f) {
        mob->wander_timer = 2.0f + rand_unit(mobs) * 3.0f;
        mob->wander_yaw = rand_unit(mobs) * 6.2831853f;
        mob->idle = rand_unit(mobs) < 0.35f;
    }
    if (mob->idle) {
        move_ground_mob(mob, world, 0.0f, 0.0f, 0.0f, dt);
        return;
    }
    move_ground_mob(mob, world, cosf(mob->wander_yaw), sinf(mob->wander_yaw),
                    mob_speed(MOB_CINDER_HOUND) * 0.5f, dt);
}

static void update_wisp(BgMob *mob, const World *world, const BgPlayer *player,
                        BgMobWorld *mobs, float dt) {
    const float dx = player->x - mob->x;
    const float dz = player->z - mob->z;
    const float distance = sqrtf(dx * dx + dz * dz);

    if (distance < 18.0f) {
        const float inv = distance > 0.001f ? 1.0f / distance : 0.0f;
        move_flying_mob(mob, world, dx * inv, dz * inv,
                        player->y + 0.9f + sinf(mob->wander_timer * 2.3f) * 0.35f,
                        mob_speed(MOB_ASH_WISP), dt);
        mob->wander_timer += dt;
        return;
    }

    mob->wander_timer -= dt;
    if (mob->wander_timer <= 0.0f) {
        mob->wander_timer = 2.0f + rand_unit(mobs) * 4.0f;
        mob->wander_yaw = rand_unit(mobs) * 6.2831853f;
        mob->idle = rand_unit(mobs) < 0.4f;
    }
    if (mob->idle) {
        move_flying_mob(mob, world, 0.0f, 0.0f, mob->y, 0.0f, dt);
        return;
    }
    move_flying_mob(mob, world, cosf(mob->wander_yaw), sinf(mob->wander_yaw),
                    mob->y + sinf(mob->wander_timer) * 0.4f,
                    mob_speed(MOB_ASH_WISP) * 0.45f, dt);
}

static void mob_attack_player(BgMob *mob, const World *world, BgPlayer *player) {
    const MobType type = (MobType)mob->type;
    const int damage = mob_contact_damage(type);
    if (damage <= 0 || mob->attack_timer > 0.0f) {
        return;
    }
    const float dx = player->x - mob->x;
    const float dz = player->z - mob->z;
    const float distance = sqrtf(dx * dx + dz * dz);
    const float reach = 1.35f + mob_half_width(type);
    if (distance > reach || fabsf(player->y - mob->y) > 1.9f) {
        return;
    }

    mob->attack_timer = 1.1f;
    if (player->invuln > 0.0f) {
        return;
    }
    player->health -= damage;
    player->invuln = 0.65f;

    /* Nudge the player away without tunnelling into terrain. */
    const float inv = distance > 0.001f ? 1.0f / distance : 0.0f;
    const float push_x = dx * inv * 0.45f;
    const float push_z = dz * inv * 0.45f;
    if (!blocked_at(world, player->x + push_x, player->y, player->z, 0.3f, 1.8f)) {
        player->x += push_x;
    }
    if (!blocked_at(world, player->x, player->y, player->z + push_z, 0.3f, 1.8f)) {
        player->z += push_z;
    }
}

void mobs_update(BgMobWorld *mobs, World *world, BgPlayer *player, float dt) {
    if (dt > 0.05f) {
        dt = 0.05f;
    }
    if (player->invuln > 0.0f) {
        player->invuln -= dt;
    }

    mobs->spawn_timer -= dt;
    if (mobs->spawn_timer <= 0.0f) {
        mobs->spawn_timer = 1.3f + rand_unit(mobs) * 1.8f;
        try_spawn(mobs, world, player);
    }

    int alive_count = 0;
    for (int i = 0; i < MAX_MOBS; ++i) {
        BgMob *mob = &mobs->mobs[i];
        if (!mob->alive) {
            continue;
        }
        if (mob->hurt_timer > 0.0f) {
            mob->hurt_timer -= dt;
        }
        if (mob->attack_timer > 0.0f) {
            mob->attack_timer -= dt;
        }

        const float dx = mob->x - player->x;
        const float dz = mob->z - player->z;
        const float distance = sqrtf(dx * dx + dz * dz);
        if (distance > MOB_DESPAWN_DISTANCE) {
            mob->alive = false;
            continue;
        }

        switch ((MobType)mob->type) {
            case MOB_GRAZER:
                update_grazer(mob, world, player, mobs, dt);
                break;
            case MOB_CINDER_HOUND:
                update_hound(mob, world, player, mobs, dt);
                break;
            case MOB_ASH_WISP:
                update_wisp(mob, world, player, mobs, dt);
                break;
            default:
                break;
        }

        mob_attack_player(mob, world, player);
        if (mob->alive) {
            alive_count++;
        }
    }
    mobs->count = alive_count;
}

/* ------------------------------------------------------------------ combat */

int mobs_pick(const BgMobWorld *mobs, const World *world, const BgPlayer *player,
              float max_dist) {
    const float dir_x = cosf(player->pitch) * cosf(player->yaw);
    const float dir_y = sinf(player->pitch);
    const float dir_z = cosf(player->pitch) * sinf(player->yaw);
    const float eye_x = player->x;
    const float eye_y = player->y + 1.62f;
    const float eye_z = player->z;

    for (float t = 0.0f; t <= max_dist; t += 0.08f) {
        const float px = eye_x + dir_x * t;
        const float py = eye_y + dir_y * t;
        const float pz = eye_z + dir_z * t;
        if (t > 0.0f &&
            block_collidable(world_get_block(world, (int)floorf(px), (int)floorf(py),
                                             (int)floorf(pz)))) {
            return -1;
        }
        for (int i = 0; i < MAX_MOBS; ++i) {
            const BgMob *mob = &mobs->mobs[i];
            if (!mob->alive) {
                continue;
            }
            const MobType type = (MobType)mob->type;
            const float half = mob_half_width(type) + 0.12f;
            const float height = mob_height(type) + 0.12f;
            if (px > mob->x - half && px < mob->x + half &&
                pz > mob->z - half && pz < mob->z + half &&
                py > mob->y && py < mob->y + height) {
                return i;
            }
        }
    }
    return -1;
}

bool mobs_damage(BgMobWorld *mobs, int index, int damage, float dir_x, float dir_z,
                 MobType *killed_type) {
    if (index < 0 || index >= MAX_MOBS) {
        return false;
    }
    BgMob *mob = &mobs->mobs[index];
    if (!mob->alive) {
        return false;
    }

    mob->health -= damage;
    mob->hurt_timer = 0.3f;
    mob->attack_timer = 0.4f;

    const float length = sqrtf(dir_x * dir_x + dir_z * dir_z);
    if (length > 0.001f) {
        mob->x += dir_x / length * 0.4f;
        mob->z += dir_z / length * 0.4f;
    }

    if (mob->health <= 0) {
        mob->alive = false;
        if (mobs->count > 0) {
            mobs->count--;
        }
        if (killed_type) {
            *killed_type = (MobType)mob->type;
        }
        return true;
    }
    return false;
}

int mob_drops(MobType type, BgStack out[2]) {
    switch (type) {
        case MOB_GRAZER:
            out[0].item = BG_ITEM_MEAT;
            out[0].count = 2;
            return 1;
        case MOB_CINDER_HOUND:
            out[0].item = BG_ITEM_EMBER_SHARD;
            out[0].count = 1;
            out[1].item = BG_ITEM_ASH_DUST;
            out[1].count = 2;
            return 2;
        case MOB_ASH_WISP:
            out[0].item = BG_ITEM_ASH_DUST;
            out[0].count = 2;
            return 1;
        default:
            return 0;
    }
}
