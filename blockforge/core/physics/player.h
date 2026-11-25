#pragma once

#include "core/world/chunk.h"
#include "platform/common/iplatform.h"
#include <cstdint>

struct Player {
    // Position and velocity (feet position).
    float x = 0.0f, y = 80.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;

    // View
    float yaw = -1.5707963f; // radians, -90deg looks toward -Z
    float pitch = 0.0f;

    // State
    bool onGround = false;
    uint16_t health = 20;

    // Inventory
    uint8_t inventory[36];
    uint8_t hotbar[9];
    int8_t selectedSlot = 0;

    Player();

    // physics_dt is a fixed timestep (1/20s).
    void Update(float dt, const InputState& input, const World& world);

    float GetWidth() const { return 0.6f; }
    float GetHeight() const { return 1.8f; }

    // Grid-march raycast from the eye position along yaw/pitch.
    bool Raycast(const World& world, float maxDist, int& outX, int& outY, int& outZ, BlockType& outType) const;

    // outX/outY/outZ receive the placed block position when non-null.
    bool PlaceBlock(World& world, BlockType type, int* outX = nullptr, int* outY = nullptr, int* outZ = nullptr);
    bool BreakBlock(World& world);
};
