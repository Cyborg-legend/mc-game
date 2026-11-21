#include "world.h"

namespace core::ecs {

World::World() : nextEntityId_(1) {
    // All arrays are value-initialized by std::array
    // componentMask_ is zero-initialized
}

World::~World() = default;

Entity World::AddEntity() {
    Entity id = nextEntityId_++;
    if (id >= MAX_ENTITIES) {
        id = MAX_ENTITIES - 1;
    }
    if (id >= static_cast<Entity>(entityIds_.size())) {
        entityIds_.resize(id + 1, Entity{0});
    }
    entityIds_[id] = id;
    componentMask_[id] = 0;
    return id;
}

void World::RemoveEntity(Entity id) {
    if (id < entityIds_.size() && entityIds_[id] != Entity{0}) {
        Entity lastId = entityIds_.back();
        entityIds_[id] = lastId;
        componentMask_[id] = componentMask_[lastId];
        if (lastId < entityIds_.size()) {
            entityIds_[lastId] = id;
        }
        componentMask_[lastId] = 0;
        entityIds_.pop_back();
    }
}

// Position accessors
void World::SetPosition(Entity id, float x, float y, float z) {
    if (id < MAX_ENTITIES) {
        positions_[id][0] = x;
        positions_[id][1] = y;
        positions_[id][2] = z;
        componentMask_[id] |= static_cast<uint32_t>(ComponentType::Position);
    }
}

void World::GetPosition(Entity id, float& x, float& y, float& z) const {
    if (id < MAX_ENTITIES) {
        x = positions_[id][0];
        y = positions_[id][1];
        z = positions_[id][2];
    }
}

// Velocity accessors
void World::SetVelocity(Entity id, float x, float y, float z) {
    if (id < MAX_ENTITIES) {
        velocities_[id][0] = x;
        velocities_[id][1] = y;
        velocities_[id][2] = z;
        componentMask_[id] |= static_cast<uint32_t>(ComponentType::Velocity);
    }
}

void World::GetVelocity(Entity id, float& x, float& y, float& z) const {
    if (id < MAX_ENTITIES) {
        x = velocities_[id][0];
        y = velocities_[id][1];
        z = velocities_[id][2];
    }
}

// Health accessors
void World::SetHealth(Entity id, int value, int max) {
    if (id < MAX_ENTITIES) {
        healths_[id][0] = value;
        healths_[id][1] = max;
        componentMask_[id] |= static_cast<uint32_t>(ComponentType::Health);
    }
}

void World::GetHealth(Entity id, int& value, int& max) const {
    if (id < MAX_ENTITIES) {
        value = healths_[id][0];
        max = healths_[id][1];
    }
}

// MobType accessors
void World::SetMobType(Entity id, MobType type) {
    if (id < MAX_ENTITIES) {
        mobTypes_[id] = static_cast<uint8_t>(type);
        componentMask_[id] |= static_cast<uint32_t>(ComponentType::MobType);
    }
}

MobType World::GetMobType(Entity id) const {
    if (id < MAX_ENTITIES) {
        return static_cast<MobType>(mobTypes_[id]);
    }
    return MobType::None;
}

// PlayerInput accessors
void World::SetPlayerInput(Entity id, PlayerInput input) {
    if (id < MAX_ENTITIES) {
        playerInputs_[id] = static_cast<uint8_t>(input);
        componentMask_[id] |= static_cast<uint32_t>(ComponentType::PlayerInput);
    }
}

PlayerInput World::GetPlayerInput(Entity id) const {
    if (id < MAX_ENTITIES) {
        return static_cast<PlayerInput>(playerInputs_[id]);
    }
    return PlayerInput::None;
}

// ForEach implementation - iterate over entities with all required components
template <ComponentType... Required>
void World::ForEach(std::function<void(Entity id, const float* pos, const float* vel, int healthVal, int healthMax, MobType mob, PlayerInput input)> lambda) {
    (void)Required...; // suppress unused warning in some cases
    for (Entity id = 1; id < nextEntityId_; ++id) {
        uint32_t mask = componentMask_[id];
        bool hasAll = true;
        ((hasAll = hasAll && ((mask & static_cast<uint32_t>(Required)) != 0)), ...);
        if (!hasAll) continue;

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
            healthVal = healths_[id][0];
            healthMax = healths_[id][1];
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

// Explicit template instantiation for common component combinations
template void World::ForEach<ComponentType::Position, ComponentType::Velocity>(std::function<void(Entity id, const float* pos, const float* vel, int healthVal, int healthMax, MobType mob, PlayerInput input)> lambda);
template void World::ForEach<ComponentType::Position, ComponentType::Velocity, ComponentType::Health>(std::function<void(Entity id, const float* pos, const float* vel, int healthVal, int healthMax, MobType mob, PlayerInput input)> lambda);

} // namespace core::ecs