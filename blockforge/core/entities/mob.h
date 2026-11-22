#pragma once

#include "core/ecs/world.h"
#include <cstdint>

enum class MobState : uint8_t {
    Idle,
    Wander,
    Chase,
    Attack,
    Flee,
    Burn
};

struct Mob {
    Entity entityId = 0;

    // Position and velocity
    float x, y, z;
    float vx, vy, vz;
    bool onGround;

    // State
    MobType type = MobType::None;
    MobState state = MobState::Idle;
    float timer = 0.0f;

    // AI parameters
    float wanderTimer = 0.0f;
    float wanderRadius = 10.0f;
    int2d homeChunk; // Chunk coordinates where mob was spawned

    // Constructor
    Mob() : onGround(true) {}

    // Update mob AI
    void Update(float dt, const World& world);

    // Move mob with simple collision
    void Move(float dt, World& world);

    // Check if mob should despawn
    bool ShouldDespawn(float playerDistSq) const;

    // Get position
    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetZ() const { return z; }
};