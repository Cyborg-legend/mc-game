#include "mob.h"
#include "world/chunk.h"
#include <cmath>
#include <random>

static std::mt19937 rng(std::random_device{}());

Mob::Mob() : x(0.0f), y(0.0f), z(0.0f), vx(0.0f), vy(0.0f), vz(0.0f),
    type(MobType::None), state(MobState::Idle), timer(0.0f),
    wanderTimer(0.0f), wanderRadius(10.0f), homeChunk{0, 0} {}

void Mob::Update(float dt, const World& world) {
    // Update timers
    timer += dt;
    wanderTimer += dt;

    // State machine
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
                float angle = static_cast<float>(rng()) / static_cast<float>(rng.max()) * 3.14159f * 2.0f;
                vx = std::cosf(angle) * 2.0f;
                vz = std::sinf(angle) * 2.0f;
            }
            x += vx * dt;
            z += vz * dt;
            if (rng() % 100 < 1) {
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;

        case MobState::Chase:
            vx *= 1.5f;
            vz *= 1.5f;
            x += vx * dt;
            z += vz * dt;
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
            timer -= dt;
            if (timer <= 0.0f) {
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;
    }

    if (y > 0.0f) {
        y += vy * dt;
    }
    if (!onGround) {
        vy -= 20.0f * dt;
    }
}

void Mob::Move(float dt, World& world) {
    x += vx * dt;
    z += vz * dt;
    if (y <= 0.0f) {
        y = 0.0f;
        vy = 0.0f;
    }
}

bool Mob::ShouldDespawn(float playerDistSq) const {
    const float despawnDistSq = 768.0f * 768.0f;
    return playerDistSq > despawnDistSq;
}