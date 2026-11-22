#include "world.h"
#include <cstring>

ECS::ECS() : count_(0), nextId_(0) {
    std::memset(alive_, 0, sizeof(alive_));
    std::memset(mobType_, 0, sizeof(mobType_));
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        positions_[i] = {0, 0, 0};
        velocities_[i] = {0, 0, 0};
        healths_[i] = {0, 0};
    }
}

ECS::~ECS() {}

Entity ECS::AddEntity() {
    if (count_ >= MAX_ENTITIES - 1) return INVALID_ENTITY;
    Entity e = nextId_++;
    if (nextId_ >= MAX_ENTITIES) nextId_ = 0;
    alive_[e] = true;
    count_++;
    return e;
}

void ECS::RemoveEntity(Entity e) {
    if (e >= MAX_ENTITIES || !alive_[e]) return;
    alive_[e] = false;
    count_--;
}

bool ECS::IsAlive(Entity e) const {
    return e < MAX_ENTITIES && alive_[e];
}

void ECS::SetPosition(Entity e, float x, float y, float z) {
    if (e < MAX_ENTITIES) positions_[e] = {x, y, z};
}

void ECS::SetVelocity(Entity e, float x, float y, float z) {
    if (e < MAX_ENTITIES) velocities_[e] = {x, y, z};
}

void ECS::SetHealth(Entity e, int val, int max) {
    if (e < MAX_ENTITIES) healths_[e] = {val, max};
}

void ECS::SetMobType(Entity e, MobType type) {
    if (e < MAX_ENTITIES) mobType_[e] = type;
}

const Position& ECS::GetPosition(Entity e) const { return positions_[e]; }
const Velocity& ECS::GetVelocity(Entity e) const { return velocities_[e]; }
const Health& ECS::GetHealth(Entity e) const { return healths_[e]; }
MobType ECS::GetMobType(Entity e) const { return mobType_[e]; }   