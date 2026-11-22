#pragma once

#include <cstdint>
#include <vector>
#include <functional>

using Entity = uint32_t;
static constexpr Entity MAX_ENTITIES = 65535;
static constexpr Entity INVALID_ENTITY = MAX_ENTITIES - 1;

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Health {
    int value;
    int max;
};

enum class MobType : uint8_t {
    None = 0,
    Player,
    Passive,
    Neutral,
    Hostile,
    Count
};

class ECS {
public:
    ECS();
    ~ECS();

    Entity AddEntity();
    void RemoveEntity(Entity e);
    bool IsAlive(Entity e) const;

    void SetPosition(Entity e, float x, float y, float z);
    void SetVelocity(Entity e, float x, float y, float z);
    void SetHealth(Entity e, int val, int max);
    void SetMobType(Entity e, MobType type);

    const Position& GetPosition(Entity e) const;
    const Velocity& GetVelocity(Entity e) const;
    const Health& GetHealth(Entity e) const;
    MobType GetMobType(Entity e) const;

    template<typename F>
    void ForEach(MobType filter, F&& fn) {
        for (Entity i = 0; i < MAX_ENTITIES; ++i) {
            if (alive_[i] && mobType_[i] == filter) {
                fn(i);
            }
        }
    }

    template<typename F>
    void ForEachAlive(F&& fn) {
        for (Entity i = 0; i < MAX_ENTITIES; ++i) {
            if (alive_[i]) {
                fn(i);
            }
        }
    }

    int GetEntityCount() const { return count_; }

private:
    bool alive_[MAX_ENTITIES];
    Position positions_[MAX_ENTITIES];
    Velocity velocities_[MAX_ENTITIES];
    Health healths_[MAX_ENTITIES];
    MobType mobType_[MAX_ENTITIES];
    int count_;
    Entity nextId_;
};   