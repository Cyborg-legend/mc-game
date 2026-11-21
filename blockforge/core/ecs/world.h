#pragma once

#include "entity.h"
#include "position.h"
#include "velocity.h"
#include "health.h"
#include "mobtype.h"
#include "playerinput.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>

namespace core::ecs {

// Component bit positions
enum class ComponentType : uint32_t {
    Position = 1 << 0,
    Velocity = 1 << 1,
    Health = 1 << 2,
    MobType = 1 << 3,
    PlayerInput = 1 << 4,
    Count
};

class World {
public:
    World() : nextEntityId_(1) {}
    ~World() = default;

    // Add an entity, return its ID
    Entity AddEntity() {
        Entity id = nextEntityId_++;
        if (id >= MAX_ENTITIES) {
            id = MAX_ENTITIES - 1;
        }
        // Ensure dense array is large enough
        if (id >= static_cast<Entity>(entityIds_.size())) {
            entityIds_.resize(id + 1, Entity{0});
        }
        entityIds_[id] = id;
        // Initialize component mask to none
        componentMask_[id] = 0;
        return id;
    }

    void RemoveEntity(Entity id) {
        if (id < entityIds_.size() && entityIds_[id] != Entity{0}) {
            // Swap with last active entity
            Entity lastId = entityIds_.back();
            entityIds_[id] = lastId;
            componentMask_[id] = componentMask_[lastId];
            // Update the sparse index for the last entity
            if (lastId < entityIds_.size()) {
                entityIds_[lastId] = id;
            }
            componentMask_[lastId] = 0;
            entityIds_.pop_back();
        }
    }

    // ---- Component access (SoA layout) ----

    // Position: set/get for entity id
    void SetPosition(Entity id, float x, float y, float z) {
        if (id < MAX_ENTITIES) {
            positions_[id][0] = x;
            positions_[id][1] = y;
            positions_[id][2] = z;
            componentMask_[id] |= static_cast<uint32_t>(ComponentType::Position);
        }
    }

    void GetPosition(Entity id, float& x, float& y, float& z) const {
        if (id < MAX_ENTITIES) {
            x = positions_[id][0];
            y = positions_[id][1];
            z = positions_[id][2];
        }
    }

    // Velocity: set/get for entity id
    void SetVelocity(Entity id, float x, float y, float z) {
        if (id < MAX_ENTITIES) {
            velocities_[id][0] = x;
            velocities_[id][1] = y;
            velocities_[id][2] = z;
            componentMask_[id] |= static_cast<uint32_t>(ComponentType::Velocity);
        }
    }

    void GetVelocity(Entity id, float& x, float& y, float& z) const {
        if (id < MAX_ENTITIES) {
            x = velocities_[id][0];
            y = velocities_[id][1];
            z = velocities_[id][2];
        }
    }

    // Health: set/get for entity id
    void SetHealth(Entity id, int value, int max) {
        if (id < MAX_ENTITIES) {
            healths_[id].value = value;
            healths_[id].max = max;
            componentMask_[id] |= static_cast<uint32_t>(ComponentType::Health);
        }
    }

    void GetHealth(Entity id, int& value, int& max) const {
        if (id < MAX_ENTITIES) {
            value = healths_[id].value;
            max = healths_[id].max;
        }
    }

    // MobType: set/get for entity id
    void SetMobType(Entity id, MobType type) {
        if (id < MAX_ENTITIES) {
            mobTypes_[id] = static_cast<uint8_t>(type);
            componentMask_[id] |= static_cast<uint32_t>(ComponentType::MobType);
        }
    }

    MobType GetMobType(Entity id) const {
        if (id < MAX_ENTITIES) {
            return static_cast<MobType>(mobTypes_[id]);
        }
        return MobType::None;
    }

    // PlayerInput: set/get for entity id
    void SetPlayerInput(Entity id, PlayerInput input) {
        if (id < MAX_ENTITIES) {
            playerInputs_[id] = static_cast<uint8_t>(input);
            componentMask_[id] |= static_cast<uint32_t>(ComponentType::PlayerInput);
        }
    }

    PlayerInput GetPlayerInput(Entity id) const {
        if (id < MAX_ENTITIES) {
            return static_cast<PlayerInput>(playerInputs_[id]);
        }
        return PlayerInput::None;
    }

    // Check if entity has specific component
    bool HasComponent(Entity id, ComponentType type) const {
        if (id >= MAX_ENTITIES) return false;
        return (componentMask_[id] & static_cast<uint32_t>(type)) != 0;
    }

    // Get component mask for entity
    uint32_t GetComponentMask(Entity id) const {
        if (id >= MAX_ENTITIES) return 0;
        return componentMask_[id];
    }

    // Set component mask for entity
    void SetComponentMask(Entity id, uint32_t mask) {
        if (id < MAX_ENTITIES) {
            componentMask_[id] = mask;
        }
    }

    // ForEach - iterate over entities that have ALL of the specified components
    // No dynamic allocation - iterates over contiguous arrays indexed by entity id
    // Template variadic with component type pack
    template <ComponentType... Required>
    void ForEach(std::function<void(Entity id, const float* pos, const float* vel, int healthVal, int healthMax, MobType mob, PlayerInput input)> lambda) {
        // Iterate over all possible entity ids (contiguous array iteration)
        for (Entity id = 1; id < nextEntityId_; ++id) {
            // Check if entity has all required components (bitmask check)
            uint32_t mask = componentMask_[id];
            bool hasAll = true;
            // Fold expression: check each required component
            ((hasAll = hasAll && ((mask & static_cast<uint32_t>(Required)) != 0)), ...);
            if (!hasAll) continue;

            // Safe contiguous array access - indexed by entity id
            const float* pos = nullptr;
            const float* vel = nullptr;
            int healthVal = 0;
            int healthMax = 0;
            MobType mob = MobType::None;
            PlayerInput input = PlayerInput::None;

            if (mask & static_cast<uint32_t>(ComponentType::Position)) {
                pos = positions_[id];
            }
            if (mask & static_cast<uint32_t>(ComponentType::Velocity)) {
                vel = velocities_[id];
            }
            if (mask & static_cast<uint32_t>(ComponentType::Health)) {
                healthVal = healths_[id].value;
                healthMax = healths_[id].max;
            }
            if (mask & static_cast<uint32_t>(ComponentType::MobType)) {
                mob = static_cast<MobType>(mobTypes_[id]);
            }
            if (mask & static_cast<uint32_t>(ComponentType::PlayerInput)) {
                input = static_cast<PlayerInput>(playerInputs_[id]);
            }

            lambda(id, pos, vel, healthVal, healthMax, mob, input);
        }
    }

    // Fixed 20Hz timestep structure
    struct FixedTimestep {
        float deltaTime = 1.0f / 20.0f;  // 50ms = 20Hz
        float accumulator = 0.0f;
    };

    FixedTimestep timestep_;

    // Entity management
    Entity nextEntityId_;
    // Dense array: maps entity id -> entity slot (for iteration)
    std::vector<Entity> entityIds_;
    // Component presence mask per entity id
    std::array<uint32_t, MAX_ENTITIES> componentMask_;

    // SoA component storage - contiguous per entity
    // positions_[id] = {x, y, z} for entity id
    std::array<std::array<float, 3>, MAX_ENTITIES> positions_;
    // velocities_[id] = {x, y, z} for entity id
    std::array<std::array<float, 3>, MAX_ENTITIES> velocities_;
    // healths_[id] = {value, max} for entity id
    std::array<std::array<int, 2>, MAX_ENTITIES> healths_;
    // mobTypes_[id] = enum value for entity id
    std::array<uint8_t, MAX_ENTITIES> mobTypes_;
    // playerInputs_[id] = enum value for entity id
    std::array<uint8_t, MAX_ENTITIES> playerInputs_;
};

} // namespace core::ecs