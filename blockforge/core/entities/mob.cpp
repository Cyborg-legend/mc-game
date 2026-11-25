#include "mob.h"
#include "world/chunk.h"

#include <cmath>
#include <random>

static std::mt19937 s_rng(std::random_device{}());

Mob::Mob() = default;

void Mob::Update(float dt, const World& world) {
    timer += dt;
    wanderTimer += dt;

    switch (state) {
        case MobState::Idle:
            if (timer > 2.0f) {
                state = MobState::Wander;
                timer = 0.0f;
            }
            break;

        case MobState::Wander:
            if (wanderTimer > 1.5f) {
                wanderTimer = 0.0f;
                const float angle = static_cast<float>(s_rng() % 6283) / 1000.0f;
                vx = std::cos(angle) * 2.0f;
                vz = std::sin(angle) * 2.0f;
            }
            x += vx * dt;
            z += vz * dt;
            if ((s_rng() % 100) < 1) {
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;

        case MobState::Chase:
            x += vx * 1.5f * dt;
            z += vz * 1.5f * dt;
            if (timer > 10.0f) {
                state = MobState::Wander;
                timer = 0.0f;
                wanderTimer = 0.0f;
            }
            break;

        case MobState::Attack:
            break;

        case MobState::Flee:
            if (timer > 3.0f) {
                state = MobState::Wander;
                timer = 0.0f;
                wanderTimer = 0.0f;
            }
            break;

        case MobState::Burn:
            timer -= dt * 2.0f;
            if (timer <= 0.0f) {
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;
    }

    // Keep mobs on top of the terrain.
    if (!onGround) {
        vy -= 20.0f * dt;
    }
    y += vy * dt;

    const int bx = static_cast<int>(std::floor(x));
    const int bz = static_cast<int>(std::floor(z));
    if (IsCollidable(world.GetBlock(bx, static_cast<int>(std::floor(y)) - 1, bz))) {
        y = std::floor(y);
        vy = 0.0f;
        onGround = true;
    } else if (y < 0.0f) {
        y = 0.0f;
        vy = 0.0f;
        onGround = true;
    } else {
        onGround = false;
    }
}

void Mob::Move(float dt, World& world) {
    (void)world;
    x += vx * dt;
    z += vz * dt;
}

bool Mob::ShouldDespawn(float playerDistSq) const {
    const float despawnDistSq = 768.0f * 768.0f;
    return playerDistSq > despawnDistSq;
}
