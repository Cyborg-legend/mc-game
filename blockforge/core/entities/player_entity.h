#pragma once

#include "core/ecs/entity.h"
#include "core/physics/player.h"
#include "core/ecs/world.h"
#include "core/world/chunk.h"
#include <glm/glm.hpp>

// Player entity that combines ECS components with physics
class PlayerEntity {
public:
    PlayerEntity() : entityId_(ecsWorld_->AddEntity()) {
        // Initialize player components
        ecsWorld_->SetPosition(entityId_, 0.0f, 64.0f, 0.0f); // Start at y=64
        ecsWorld_->SetHealth(entityId_, 20, 20);
        ecsWorld_->SetMobType(entityId_, MobType::Player);
        ecsWorld_->SetPlayerInput(entityId_, PlayerInput::None);

        // Initialize player physics
        player_.position = glm::vec3(0.0f, 64.0f, 0.0f);
        player_.hotbarIndex = 0;
        // Initialize inventory
        for (int i = 0; i < 36; ++i) {
            player_.inventory[i].type = BlockType::Air;
            player_.inventory[i].count = 0;
        }
    }

    ~PlayerEntity() {
        if (ecsWorld_) {
            ecsWorld_->RemoveEntity(entityId_);
        }
    }

    // Initialize with world reference
    void Init(World* world) { ecsWorld_ = world; }

    // Get ECS entity ID
    Entity GetEntityId() const { return entityId_; }

    // Get player physics
    const Player& GetPlayer() const { return player_; }
    Player& GetPlayer() { return player_; }

    // Update player input and physics
    void Update(float deltaTime, const PlayerInput& input, World& world);

    // Move player with collision detection
    bool Move(glm::vec3 moveVec, World& world);

    // Look around (yaw/pitch)
    void Look(float dx, float dy);

    // Jump
    void Jump(World& world);

    // Place block
    bool PlaceBlock(int maxDist, World& world);

    // Break block
    bool BreakBlock(int maxDist, World& world);

    // Inventory operations
    void PickupBlock(BlockType type);
    void UseHotbar(int direction);
    void ScrollHotbar(int delta);

private:
    Entity entityId_;
    World* ecsWorld_ = nullptr;

    Player player_;

    // Mouse sensitivity
    float mouseSensitivity = 0.1f;
};