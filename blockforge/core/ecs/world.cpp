#include "world.h"

ECS::ECS() {
    for (Entity i = 0; i < MAX_ENTITIES; ++i) {
        m_activeEntities[i] = i;
        m_positions[i] = Position{ 0.0f, 0.0f, 0.0f };
        m_velocities[i] = Velocity{ 0.0f, 0.0f, 0.0f };
        m_healths[i] = Health{ 20, 20 };
        m_mobTypes[i] = MobTypeComponent{ MobType::None };
        m_playerInputs[i] = PlayerInput{};
    }
    m_numActive = MAX_ENTITIES;
}

Entity ECS::AddEntity(MobType type) {
    if (m_numActive == 0) {
        return MAX_ENTITIES;
    }
    const Entity id = m_activeEntities[--m_numActive];
    m_mobTypes[id].type = type;
    if (type != MobType::None) {
        m_mobs.push_back(id);
    }
    return id;
}

void ECS::RemoveEntity(Entity id) {
    if (id >= MAX_ENTITIES) {
        return;
    }
    m_activeEntities[m_numActive++] = id;
    for (size_t i = 0; i < m_mobs.size(); ++i) {
        if (m_mobs[i] == id) {
            m_mobs[i] = m_mobs.back();
            m_mobs.pop_back();
            break;
        }
    }
    m_mobTypes[id].type = MobType::None;
}

Position& ECS::GetPosition(Entity id) { return m_positions[id]; }
const Position& ECS::GetPosition(Entity id) const { return m_positions[id]; }
Velocity& ECS::GetVelocity(Entity id) { return m_velocities[id]; }
const Velocity& ECS::GetVelocity(Entity id) const { return m_velocities[id]; }
Health& ECS::GetHealth(Entity id) { return m_healths[id]; }
const Health& ECS::GetHealth(Entity id) const { return m_healths[id]; }
MobType ECS::GetMobType(Entity id) const { return m_mobTypes[id].type; }
PlayerInput& ECS::GetPlayerInput(Entity id) { return m_playerInputs[id]; }
const PlayerInput& ECS::GetPlayerInput(Entity id) const { return m_playerInputs[id]; }
