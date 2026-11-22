#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <functional>

constexpr uint32_t MAX_ENTITIES = 65535;
using Entity = uint32_t;

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Health {
    uint16_t value;
    uint16_t max;
};

enum class MobType : uint8_t {
    None,
    Wanderer,
    Chaser,
    Fleer,
    Burner
};

struct MobTypeComponent {
    MobType type = MobType::None;
};

struct PlayerInput {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool jump = false;
    bool sprint = false;
};

class ECS {
public:
    ECS();
    ~ECS() = default;

    Entity AddEntity(MobType type = MobType::None);
    void RemoveEntity(Entity id);

    Position& GetPosition(Entity id);
    const Position& GetPosition(Entity id) const;
    Velocity& GetVelocity(Entity id);
    const Velocity& GetVelocity(Entity id) const;
    Health& GetHealth(Entity id);
    const Health& GetHealth(Entity id) const;
    MobType GetMobType(Entity id) const;
    PlayerInput& GetPlayerInput(Entity id);
    const PlayerInput& GetPlayerInput(Entity id) const;

private:
    // Entity management
    Entity m_activeEntities[MAX_ENTITIES];
    Entity m_numActive;

    // Component storage
    Position m_positions[MAX_ENTITIES];
    Velocity m_velocities[MAX_ENTITIES];
    Health m_healths[MAX_ENTITIES];
    MobTypeComponent m_mobTypes[MAX_ENTITIES];
    PlayerInput m_playerInputs[MAX_ENTITIES];

    // Mobs storage
    std::vector<Entity> m_mobs;
};