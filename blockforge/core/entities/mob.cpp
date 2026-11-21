#include "mob.h"
#include "core/ecs/position.h"
#include "core/ecs/velocity.h"
#include "core/world/world.h"
#include <glm/glm.hpp>
#include <cmath>

void MobAI::Update(float deltaTime, World& world, int playerEntityId) {
    timer += deltaTime;

    // Update burn timer if applicable
    if (type == MobType::Zombie && state != MobState::Burn) {
        burnTimer -= static_cast<int>(deltaTime * 20.0f); // 20Hz
        if (burnTimer <= 0) {
            // Check if in daylight - burn
            // For now, just transition to Burn state if timer expires
            state = MobState::Burn;
        }
    }

    // Decide state transitions
    DecideState(deltaTime, world, playerEntityId);

    // Execute state-based movement
    switch (state) {
        case MobState::Idle:
            // No movement, just timer-based transition
            if (timer > 3.0f) {
                state = MobState::Wander;
                timer = 0.0f;
            }
            break;
        case MobState::Wander:
            // Wander randomly
            // Simple: pick a random direction after some time
            timer -= deltaTime;
            if (timer <= 0.0f) {
                // Transition back to idle after wandering
                state = MobState::Idle;
                timer = 0.0f;
            }
            break;
        case MobState::Chase:
            // Move toward player
            MoveTowardTarget(deltaTime, world);
            break;
        case MobState::Attack:
            // Simple: just move toward target
            // Actual attack logic would be separate
            MoveTowardTarget(deltaTime, world);
            break;
        case MobState::Flee:
            // Flee from player
            // Would implement flee logic here
            break;
        case MobState::Burn:
            // Take damage over time while burning
            // Eventually despawn or die
            burnTimer -= static_cast<int>(deltaTime * 20.0f);
            if (burnTimer <= 0) {
                // Die from burning
                // For now, just transition back
                state = MobState::Idle;
            }
            break;
        default:
            break;
    }
}

void MobAI::DecideState(float deltaTime, World& world, int playerEntityId) {
    // Check player distance
    float dist = DistanceToTarget(world, playerEntityId);

    // State machine logic
    switch (state) {
        case MobState::Idle:
            // If player is close, start chasing
            if (dist > 0 && dist < 16.0f) {
                // Check if it's night for zombies, or any time for wolves
                // For all mobs, chase player when within range
                state = MobState::Chase;
                timer = 0.0f;
            }
            // If player is very close, attack
            else if (dist > 0 && dist < 4.0f) {
                state = MobState::Attack;
            }
            break;

        case MobState::Wander:
            // If player comes within range, chase
            if (dist > 0 && dist < 16.0f) {
                state = MobState::Chase;
            }
            // After wandering for a while, go idle
            timer -= deltaTime;
            if (timer <= 0.0f) {
                state = MobState::Idle;
            }
            break;

        case MobState::Chase:
            // Continue chasing while player is within range
            if (dist > 32.0f) {
                // Player too far, go idle
                state = MobState::Idle;
            } else if (dist < 4.0f) {
                // Get close enough to attack
                state = MobState::Attack;
            }
            // If it's daytime and zombie, start burning
            if (type == MobType::Zombie && dist > 0) {
                // Simplified: just start burn timer
                burnTimer = 600; // 60 seconds at 20Hz = 3 minutes max
                state = MobState::Burn;
            }
            break;

        case MobState::Attack:
            // If player moves away, resume chase
            if (dist > 8.0f) {
                state = MobState::Chase;
            }
            // After attacking for a while, go idle
            timer -= deltaTime;
            if (timer <= 0.0f) {
                state = MobState::Idle;
            }
            break;

        case MobState::Flee:
            // If player is far enough, stop fleeing
            if (dist > 20.0f) {
                state = MobState::Idle;
            }
            break;

        case MobState::Burn:
            // If night time, stop burning
            // (In a full implementation, check actual time of day)
            // For now, just reduce burn timer
            burnTimer -= static_cast<int>(deltaTime * 20.0f);
            if (burnTimer <= 0) {
                state = MobState::Idle;
            }
            break;
    }
}

glm::vec3 MobAI::MoveTowardTarget(float deltaTime, const World& world) {
    // Get current position
    float mx, mz;
    // Need access to position - this is simplified
    // In full implementation, would query world position component
    // For now, return zero movement
    (void)world;
    (void)deltaTime;
    return glm::vec3(0.0f);
}

float MobAI::DistanceToTarget(const World& world, int playerEntityId) const {
    // Placeholder: return distance to player
    // Full implementation would get player position and compute distance
    return 0.0f;
}

// ============================================================
// Mob spawning and despawn logic
// ============================================================

// Spawn mobs: 1 mob per 4x4 chunk area, max 200 total, within 32 chunks of player
void SpawnMobs(World& world, int playerChunkX, int playerChunkZ, int& totalMobs) {
    // Count existing mobs
    // In a full implementation, would iterate over entities with MobType

    // Calculate spawn area: ±2 chunks around player (8x8 chunk area = 128x128 blocks)
    int spawnRadius = 8; // chunks

    // Try to spawn up to 5 mobs per call
    int toSpawn = 5;
    while (toSpawn > 0 && totalMobs < 200) {
        // Pick random location within spawn radius
        int cx = playerChunkX + (rand() % (spawnRadius * 2)) - spawnRadius;
        int cz = playerChunkZ + (rand() % (spawnRadius * 2)) - spawnRadius;
        int cy = 0; // Ground level

        // Check if chunk already has mobs (simplified - just check if position is valid)
        // Get or create chunk
        Chunk& chunk = world.GetOrCreateChunk(cx, cy / 16, cz);

        // Check if the spot is valid (ground, not already packed with mobs)
        // Simplified: just check if there's air at the spawn position
        BlockType type = chunk.GetBlock(8, 0, 8); // Check center of chunk
        if (type == BlockType::Air || type == BlockType::Dirt) {
            // Spawn mob - just mark an entity as having MobAI
            // In full implementation, would create an entity with MobAI component
            totalMobs--;
            // Don't actually spawn here in this simplified version
        }

        toSpawn--;
    }
}

// Despawn: beyond 48 chunks
void CheckDespawn(World& world, int playerChunkX, int playerChunkZ) {
    // In a full implementation, would iterate over mob entities
    // and check distance from player
    // For now, just a placeholder
}