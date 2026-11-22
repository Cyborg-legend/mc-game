#pragma once

#include "core/ecs/world.h"
#include "core/physics/player.h"
#include <cstdint>

class PlayerEntity {
public:
    Entity entityId = 0;
    
    // Position and velocity
    float x, y, z;
    float vx, vy, vz;
    
    // State
    bool onGround;
    uint16_t health;
    bool isPlayer; // Distinguish from mobs
    
    // Input and look
    float yaw; // Horizontal rotation in radians
    float pitch; // Vertical rotation in radians
    
    // Constructor
    PlayerEntity();
    
    // Update player entity with input and physics
    void Update(float dt, const InputState& input, World& world);
    
    // Move with collision
    void Move(float dt, World& world);
    
    // Look around (mouse movement)
    void Look(float dx, float dy);
    
    // Jump
    void Jump(World& world);
    
    // Place block
    bool PlaceBlock(World& world);
    
    // Break block
    bool BreakBlock(World& world);
    
    // Pick up block
    bool PickupBlock(World& world);
    
    // Get position
    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetZ() const { return z; }
};