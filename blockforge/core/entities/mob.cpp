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
            // Transition to wander after a short time
            if (timer > 2.0f) {
                state = MobState::Wander;
                timer = 0.0f;
            }
            break;

        case MobState::Wander:
            // Wander randomly
            if (wanderTimer > 1.5f) {
                // Change direction every 1.5 seconds
                wanderTimer = 0.0f;
                // Random direction
                float angle = static_cast<float>(rng()) / static_cast<float>(rng.max()) * 3.14159f * 2.0f;
                vx = std::cosf(angle) * 2.0f;
                vz = std::sinf(angle) * 2.0f;
            }

            // Update position
            x += vx * dt;
            z += vz * dt;

            // Check if player is close enough to chase
            // Simplified: check if player is within 16 blocks
            // In a full implementation, would check actual player position
            // if (DistanceToPlayer < 16.0f) state = MobState::Chase;

            // Randomly transition back to idle
            if (rng() % 100 < 1) { // 1% chance per frame
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;

        case MobState::Chase:
            // Chase the player
            // In a full implementation, would calculate direction to player
            // For now, just continue wandering with faster speed
            vx *= 1.5f;
            vz *= 1.5f;

            x += vx * dt;
            z += vz * dt;

            // Lose player if too far
            if (timer > 10.0f) {
                state = MobState::Wander;
                timer = 0.0f;
                wanderTimer = 0.0f;
            }
            break;

        case MobState::Attack:
            // Attack player
            // Reduce player health
            // timer -= dt;
            // if (timer <= 0.0f) state = MobState::Wander;
            break;

        case MobState::Flee:
            // Flee from player or danger
            // Move away from player
            // vx and vz would be negated from player direction
            // After some time, return to wander
            if (timer > 3.0f) {
                state = MobState::Wander;
                timer = 0.0f;
                wanderTimer = 0.0f;
            }
            break;

        case MobState::Burn:
            // Burn in daylight
            // Take damage over time
            // if (timer > 10.0f) // mob dies
            //     state = MobState::Idle;
            timer -= dt;
            if (timer <= 0.0f) {
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;
    }

    // Ground check and gravity
    if (y > 0.0f) {
        y += vy * dt;
    }
    if (!onGround) {
        vy -= 20.0f * dt; // Gravity
    }
}

void Mob::Move(float dt, World& world) {
    // Apply movement
    x += vx * dt;
    z += vz * dt;

    // Simple ground check
    if (y <= 0.0f) {
        y = 0.0f;
        vy = 0.0f;
    }
}

bool Mob::ShouldDespawn(float playerDistSq) const {
    // Despawn beyond 48 chunks
    // 1 chunk = 16 blocks, so 48 * 16 = 768 blocks
    const float despawnDistSq = 768.0f * 768.0f;
    return playerDistSq > despawnDistSq;
}