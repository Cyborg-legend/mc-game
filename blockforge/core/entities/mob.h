#pragma once

#include "core/ecs/entity.h"
#include "core/ecs/mobtype.h"
#include "core/ecs/position.h"
#include "core/ecs/velocity.h"
#include "core/world/world.h"
#include <glm/glm.hpp>
#include <cstdint>

// Mob state machine states
enum class MobState : uint8_t {
    Idle,
    Wander,
    Chase,
    Attack,
    Flee,
    Burn
};

// Mob AI component
struct MobAI {
    MobType type;
    MobState state;
    float timer; // For state transitions
    int targetEntityId; // Entity ID to chase/attack
    int burnTimer; // Timer for daylight burning

    // Pathfinding
    glm::vec3 targetPosition; // Target position for movement

    MobAI() : type(MobType::None), state(MobState::Idle), timer(0.0f),
              targetEntityId(-1), burnTimer(0) {}

    // Update AI state
    void Update(float deltaTime, World& world, int playerEntityId);

    // Decide next state
    void DecideState(float deltaTime, World& world, int playerEntityId);

    // Simple pathfinding move toward target
    glm::vec3 MoveTowardTarget(float deltaTime, const World& world);

    // Check if can reach target (simple obstacle check)
    bool CanReachTarget(const World& world, int cx, int cy, int cz);

    // Get distance to target
    float DistanceToTarget(const World& world, int playerEntityId) const;
};