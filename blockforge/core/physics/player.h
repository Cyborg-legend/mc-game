#pragma once

#include "core/ecs/world.h"
#include "core/world/chunk.h"
#include "platform/common/iplatform.h"
#include <cstdint>

struct Player {
    // Position and velocity
    float x, y, z;
    float vx, vy, vz;

    // State
    bool onGround;
    uint16_t health;

    // Input
    struct Input {
        bool forward = false;
        bool backward = false;
        bool left = false;
        bool right = false;
        bool jump = false;
        bool sprint = false;
    } input;

    // Inventory
    uint8_t inventory[36]; // 36 slots
    uint8_t hotbar[9];     // 9 hotbar slots

    // Hotbar selection (1-9 -> index 0-8)
    int8_t selectedSlot = 0;

    // Constructor
    Player();

    // Update player physics with fixed timestep
    void Update(float dt, const InputState& input);

    // AABB collision bounds
    float GetWidth() const { return 0.6f; }
    float GetHeight() const { return 1.8f; }

    // Raycast for block interactions
    bool Raycast(const World& world, float maxDist, float& outX, float& outY, float& outZ, BlockType& outType);

    // Place/break block
    bool PlaceBlock(World& world, int mouseButton);
    bool BreakBlock(World& world);
};
