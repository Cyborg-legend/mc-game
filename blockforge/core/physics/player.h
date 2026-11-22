#pragma once

#include <cstdint>
#include "core/world/chunk.h"

static constexpr int MAX_INVENTORY = 36;
static constexpr int HOTBAR_SLOTS = 9;

struct InventorySlot {
    BlockType type = BlockType::Air;
    int count = 0;
};

struct Player {
    float x, y, z;
    float vx, vy, vz;
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool onGround = false;
    bool isSprinting = false;
    bool isFlying = false;

    float gravity = -20.0f;
    float jumpVelocity = 8.0f;
    float walkSpeed = 4.0f;
    float sprintSpeed = 6.0f;

    int health = 20;
    int maxHealth = 20;

    InventorySlot inventory[MAX_INVENTORY];
    int hotbarIndex = 0;

    float width = 0.6f;
    float height = 1.8f;

    Player() : x(0), y(64), z(0), vx(0), vy(0), vz(0) {
        for (int i = 0; i < MAX_INVENTORY; ++i) {
            inventory[i] = {};
        }
    }

    void Update(float dt, const bool keys[], int mouseButtons[]);
    void Move(float dt, class World& world);
    void Jump();
    void Look(float dx, float dy);
    bool PlaceBlock(class World& world, const bool mouseButtons[]);
    bool BreakBlock(class World& world, const bool mouseButtons[]);
    void CycleHotbar(int direction);
    void SetHotbar(int index);
};   