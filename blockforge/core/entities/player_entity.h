#pragma once

#include "core/ecs/world.h"
#include "core/world/chunk.h"
#include "platform/common/iplatform.h"

#include <cstdint>

// Player representation used by the entity/physics layer.
class PlayerEntity {
public:
    Entity entityId = 0;
    float x = 0.0f, y = 80.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    bool onGround = false;
    uint16_t health = 20;
    bool isPlayer = true;
    float yaw = -1.5707963f;
    float pitch = 0.0f;

    PlayerEntity();

    void Update(float dt, const InputState& input, World& world);
    void Move(float dt, World& world);
    void Look(float dx, float dy);
    void Jump(World& world);

    bool PlaceBlock(World& world);
    bool BreakBlock(World& world);
    bool PickupBlock(World& world);
};
