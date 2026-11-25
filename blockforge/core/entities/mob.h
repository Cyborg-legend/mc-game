#pragma once

#include "core/ecs/world.h"
#include "core/world/chunk.h"
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
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    bool onGround = true;

    // State
    MobType type = MobType::None;
    MobState state = MobState::Idle;
    float timer = 0.0f;

    // AI parameters
    float wanderTimer = 0.0f;
    float wanderRadius = 10.0f;

    Mob();

    void Update(float dt, const World& world);
    void Move(float dt, World& world);
    bool ShouldDespawn(float playerDistSq) const;

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetZ() const { return z; }
};
