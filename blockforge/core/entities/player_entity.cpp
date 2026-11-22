#include "player_entity.h"
#include "world/chunk.h"
#include "physics/player.h"
#include <cmath>

PlayerEntity::PlayerEntity() : x(0.0f), y(20.0f), z(0.0f), vx(0.0f), vy(0.0f), vz(0.0f),
    onGround(true), health(20), isPlayer(true), yaw(0.0f), pitch(0.0f) {}

void PlayerEntity::Update(float dt, const InputState& input, World& world) {
    Move(dt, world);
    
    // Mouse look - update yaw and pitch
    // yaws and pitch would be accumulated from mouse movement
    // For now, just store the input for the player physics system
    
    // Update player physics
    Player player;
    player.x = this->x;
    player.y = this->y;
    player.z = this->z;
    player.onGround = this->onGround;
    player.input = input;
    
    // The player physics update would be handled by the main loop
    // This is a simplified version
}

void PlayerEntity::Move(float dt, World& world) {
    // Fixed movement values from spec
    float speed = 4.0f; // walk
    if (/* input.sprint */ true) speed = 6.0f; // sprint
    
    // Direction based on yaw
    float dx = 0.0f, dz = 0.0f;
    
    // Simple forward/backward/left/right
    // In a full implementation, this would use the actual input state
    
    // Apply movement
    x += dx * speed * dt;
    z += dz * speed * dt;
    
    // Gravity and jumping
    if (!onGround) {
        vy -= 20.0f * dt; // Gravity
    }
    
    y += vy * dt;
    
    // Simple ground check
    if (y <= 0.0f) {
        y = 0.0f;
        vy = 0.0f;
        onGround = true;
    }
}

void PlayerEntity::Look(float dx, float dy) {
    yaw += dx * 0.01f; // Sensitivity
    pitch += dy * 0.01f;
    
    // Clamp pitch to prevent flipping
    if (pitch > 1.57f) pitch = 1.57f; // 90 degrees
    if (pitch < -1.57f) pitch = -1.57f;
}

void PlayerEntity::Jump(World& world) {
    if (onGround) {
        vy = 8.0f; // Jump velocity
        onGround = false;
    }
}

bool PlayerEntity::PlaceBlock(World& world) {
    // Raycast to find block to place
    float rx, ry, rz;
    BlockType type;
    
    // Simple raycast - check block at player position + 1 in look direction
    rx = x + std::cosf(pitch) * std::cosf(yaw);
    ry = y + std::sinf(pitch);
    rz = z + std::sinf(pitch) * std::sinf(yaw);
    
    if (world.GetBlock((int)rx, (int)ry, (int)rz) == BlockType::Air) {
        world.SetBlock((int)rx, (int)ry, (int)rz, BlockType::Dirt);
        return true;
    }
    return false;
}

bool PlayerEntity::BreakBlock(World& world) {
    // Raycast to find block to break
    float rx, ry, rz;
    BlockType type;
    
    // Check block at player position
    rx = x;
    ry = y;
    rz = z;
    
    BlockType bt = world.GetBlock((int)rx, (int)ry, (int)rz);
    if (bt != BlockType::Air && bt != BlockType::Bedrock) {
        world.SetBlock((int)rx, (int)ry, (int)rz, BlockType::Air);
        return true;
    }
    return false;
}

bool PlayerEntity::PickupBlock(World& world) {
    // Not implemented - basic placeholder
    return false;
}