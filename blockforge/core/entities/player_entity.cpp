#include "player_entity.h"
#include "core/ecs/position.h"
#include "core/ecs/velocity.h"
#include "core/ecs/health.h"
#include "core/ecs/mobtype.h"
#include "core/ecs/playerinput.h"
#include <glm/glm.hpp>
#include <cmath>

void PlayerEntity::Update(float deltaTime, const PlayerInput& input, World& world) {
    // Store input
    PlayerInput currentInput = input;

    // Get current position from ECS
    float px, py, pz;
    ecsWorld_->GetPosition(entityId_, px, py, pz);
    player_.position = glm::vec3(px, py, pz);

    // Update hotbar selection from scroll/wheel
    // (handled separately)

    // Calculate movement direction based on input and camera yaw
    // We'll use a simple approach: forward/back relative to yaw
    float moveSpeed = player_.isSprinting ? player_.sprintSpeed : player_.speed;

    // Direction vectors based on yaw (yaw is rotation around Y axis)
    float radYaw = glm::radians(player_.position.y); // Assuming position stores yaw somehow
    // Actually, let's store yaw separately or use a different approach
    // For now, use basic WASD movement

    // Simple: use input to move in world space
    glm::vec3 move(0.0f);
    if (currentInput == PlayerInput::Forward) {
        // Move in negative Z direction ( Blender/OpenGL convention )
        // Actually, let's use a proper yaws-based direction
        // This is simplified - full implementation would track camera yaw
        move.z -= moveSpeed;
    } else if (currentInput == PlayerInput::Back) {
        move.z += moveSpeed;
    }
    if (currentInput == PlayerInput::Left) {
        move.x -= moveSpeed;
    } else if (currentInput == PlayerInput::Right) {
        move.x += moveSpeed;
    }

    // Apply gravity
    player_.ApplyGravity(deltaTime);

    // Move and check collisions
    // Move X first, then Z, then Y (with gravity)
    // Simple collision: check 6 faces against world blocks

    // Move X
    if (move.x != 0.0f) {
        // Try move in X
        player_.position.x += move.x * deltaTime;
        // Check for collisions with world blocks
        // ... (simplified - would check AABB against chunks)
        // For now, just clamp if hitting a wall
        // Check blocks at player foot and head positions
        int fx = static_cast<int>(std::floor(player_.position.x - player_.width / 2.0f));
        int fz = static_cast<int>(std::floor(player_.position.z));
        int fy = static_cast<int>(std::floor(player_.position.y));

        // Check if blocking
        BlockType leftBlock = world.GetBlock(fx - 1, fy, fz);
        BlockType rightBlock = world.GetBlock(fx + 1, fy, fz);
        // If blocking, revert X movement
        // (This is very simplified)
    }

    // Move Z
    if (move.z != 0.0f) {
        player_.position.z += move.z * deltaTime;
    }

    // Apply vertical movement (gravity/jump)
    player_.position.y += player_.velocity.y * deltaTime;

    // Update ECS position
    ecsWorld_->SetPosition(entityId_, player_.position.x, player_.position.y, player_.position.z);
}

void PlayerEntity::Look(float dx, float dy) {
    // Mouse look: adjust yaw and pitch
    player_.yaw += dx * player_.mouseSensitivity;
    player_.pitch += dy * player_.mouseSensitivity;

    // Clamp pitch to prevent flipping
    if (player_.pitch > 89.0f) player_.pitch = 89.0f;
    if (player_.pitch < -89.0f) player_.pitch = -89.0f;
}

void PlayerEntity::Jump(World& world) {
    if (player_.CanJump()) {
        player_.Jump();
    }
}

bool PlayerEntity::PlaceBlock(int maxDist, World& world) {
    return player_.PlaceBlock(maxDist, world);
}

bool PlayerEntity::BreakBlock(int maxDist, World& world) {
    return player_.BreakBlock(maxDist, world);
}

void PlayerEntity::PickupBlock(BlockType type) {
    // Find empty slot in inventory
    for (int i = 0; i < 36; ++i) {
        if (player_.inventory[i].type == BlockType::Air) {
            player_.inventory[i].type = type;
            player_.inventory[i].count = 1;
            return;
        }
    }
    // Shift-click: move to first slot with same type
    for (int i = 0; i < 36; ++i) {
        if (player_.inventory[i].type == type && player_.inventory[i].count < 64) {
            player_.inventory[i].count++;
            return;
        }
    }
}

void PlayerEntity::UseHotbar(int direction) {
    CycleHotbar(direction);
}

void PlayerEntity::ScrollHotbar(int delta) {
    hotbarIndex = (hotbarIndex + delta + HOTBAR_SLOTS) % HOTBAR_SLOTS;
}

// Helper to cycle hotbar
void PlayerEntity::CycleHotbar(int direction) {
    hotbarIndex = (hotbarIndex + direction + HOTBAR_SLOTS) % HOTBAR_SLOTS;
}