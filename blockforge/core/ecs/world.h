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

class World {
public:
    World();
    ~World() = default;

    Entity AddEntity(MobType type = MobType::None);
    void RemoveEntity(Entity id);

    // SoA accessors
    Position& GetPosition(Entity id);
    const Position& GetPosition(Entity id) const;
    Velocity& GetVelocity(Entity id);
    const Velocity& GetVelocity(Entity id) const;
    Health& GetHealth(Entity id);
    const Health& GetHealth(Entity id) const;
    MobType GetMobType(Entity id) const;
    PlayerInput& GetPlayerInput(Entity id);
    const PlayerInput& GetPlayerInput(Entity id) const;

    // ForEach with perfect forwarding, no dynamic allocation
    template <typename F, typename... Components>
    void ForEach(F&& func) {
        for (Entity id : m_activeEntities) {
            // Safe access - only entities with all components will work
            // Caller must ensure entity has all required components
            if constexpr (sizeof...(Components) == 0) {
                func(id);
            } else if constexpr (std::is_same_v<Components, Position>) {
                func(id, GetPosition(id));
            } else if constexpr (std::is_same_v<Components, Velocity>) {
                func(id, GetVelocity(id));
            } else if constexpr (std::is_same_v<Components, Health>) {
                func(id, GetHealth(id));
            } else if constexpr (std::is_same_v<Components, MobTypeComponent>) {
                func(id, GetMobType(id));
            } else if constexpr (std::is_same_v<Components, PlayerInput>) {
                func(id, GetPlayerInput(id));
            }
        }
    }

    // Chunk access
    struct Chunk {
        uint8_t blocks[4096]; // 16x16x16
    };
    Chunk& GetOrCreateChunk(int x, int z);
    uint8_t GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, uint8_t type);
    void GenerateChunks();

    // Mob management
    std::vector<Entity>& GetMobs() { return m_mobs; }
    const std::vector<Entity>& GetMobs() const { return m_mobs; }

    Entity GetPlayerEntity() const { return m_playerEntity; }
    void SetPlayerEntity(Entity id) { m_playerEntity = id; }

private:
    std::array<Position, MAX_ENTITIES> m_positions{};
    std::array<Velocity, MAX_ENTITIES> m_velocities{};
    std::array<Health, MAX_ENTITIES> m_healths{};
    std::array<MobTypeComponent, MAX_ENTITIES> m_mobTypes{};
    std::array<PlayerInput, MAX_ENTITIES> m_playerInputs{};
    
    std::array<Chunk, 4096> m_chunks{};
    Entity m_activeEntities[MAX_ENTITIES] = {0};
    uint32_t m_numActive = 0;
    
    Entity m_mobs[MAX_ENTITIES] = {0};
    uint32_t m_numMobs = 0;
    
    Entity m_playerEntity = 1;
};