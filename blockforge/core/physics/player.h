#pragma once

#include "core/ecs/position.h"
#include "core/ecs/velocity.h"
#include "core/ecs/health.h"
#include "core/ecs/mobtype.h"
#include "core/ecs/playerinput.h"
#include <array>
#include <cmath>

// Player component: position, velocity, onGround, health, inventory (36 slots)
static constexpr int MAX_INVENTORY = 36;
static constexpr int HOTBAR_SLOTS = 9;

enum class HotbarSelection : int {
    Slot1 = 0,
    Slot2,
    Slot3,
    Slot4,
    Slot5,
    Slot6,
    Slot7,
    Slot8,
    Slot9
};

struct InventorySlot {
    BlockType type = BlockType::Air;
    int count = 0; // stack size (max 64)
};

struct Player {
    // Movement
    glm::vec3 position;      // World position
    glm::vec3 velocity;      // Current velocity
    float onGround : 1;    // Whether player is on ground
    float inWater : 1;     // Whether player is in water
    float isSprinting : 1; // Sprinting flag
    float isFlying : 1;    // Creative fly flag

    // Physics properties
    float gravity = -20.0f;      // Gravity acceleration (blocks/s²)
    float jumpVelocity = 8.0f;   // Initial jump velocity (blocks/s)
    float speed = 4.0f;          // Normal walking speed (blocks/s)
    float sprintSpeed = 6.0f;    // Sprinting speed (blocks/s)

    // Health
    int health = 20;
    int maxHealth = 20;

    // Inventory
    InventorySlot inventory[MAX_INVENTORY];
    int hotbarIndex; // Selected hotbar slot (0-8)

    // Player dimensions
    float width = 0.6f;
    float height = 1.8f;

    // State
    Player() : onGround(false), inWater(false), isSprinting(false), isFlying(false),
               hotbarIndex(0) {
        // Initialize inventory to Air
        for (int i = 0; i < MAX_INVENTORY; ++i) {
            inventory[i].type = BlockType::Air;
            inventory[i].count = 0;
        }
    }

    // Reset vertical velocity
    void ResetVerticalVelocity() { velocity.y = 0.0f; }

    // Apply gravity for one timestep (20Hz = 0.05s)
    void ApplyGravity(float deltaTime) {
        velocity.y += gravity * deltaTime;
    }

    // Jump - only if on ground
    void Jump() {
        if (onGround) {
            velocity.y = jumpVelocity;
            onGround = false;
        }
    }

    // Check if can jump
    bool CanJump() const { return onGround; }

    // Set on ground state
    void SetOnGround(bool ground) { onGround = ground; }

    // Set sprinting state
    void SetSprinting(bool sprinting) { isSprinting = sprinting; }

    // Toggle flying (creative)
    void ToggleFly() { isFlying = !isFlying; }

    // Get selected inventory slot
    InventorySlot& GetSelectedSlot() { return inventory[hotbarIndex]; }

    // Get hotbar slot
    InventorySlot& GetHotbarSlot(int index) { return inventory[HOTBAR_SLOTS + index]; }

    // Cycle hotbar selection
    void CycleHotbar(int direction) {
        hotbarIndex = (hotbarIndex + direction + HOTBAR_SLOTS) % HOTBAR_SLOTS;
    }

    // Place block from selected slot
    bool PlaceBlock(int x, int y, int z, World& world);

    // Break block via raycast
    bool BreakBlock(int maxDistance, World& world);
};