#include "world.h"
#include "world/chunk.h"
#include "physics/player.h"
#include "entities/mob.h"
#include <cmath>
#include <algorithm>

ECS::ECS() {
    // Initialize all entities as inactive
    for (Entity i = 0; i < MAX_ENTITIES; ++i) {
        m_activeEntities[i] = i;
        m_positions[i] = { 0.0f, 0.0f, 0.0f };
        m_velocities[i] = { 0.0f, 0.0f, 0.0f };
        m_healths[i] = { 20, 20 };
        m_mobTypes[i] = { MobType::None };
        m_playerInputs[i] = { false, false, false, false, false, false };
    }
    m_numActive = MAX_ENTITIES;
    
    // Initialize chunks as empty (all Air)
    for (int i = 0; i < 4096; ++i) {
        m_chunks[i].blocks[0] = (uint8_t)0; // Air
    }
    
    // Reserve space for mobs and spawn some
    m_mobs.reserve(MAX_ENTITIES);
    
    // Spawn some mobs
    SpawnMobs();
}

Entity ECS::AddEntity(MobType type) {
    if (m_numActive == 0) return MAX_ENTITIES; // No slots
    
    Entity id = m_activeEntities[--m_numActive];
    m_mobTypes[id] = { type };
    
    // Add to mob vector if this is a mob
    if (type != MobType::None) {
        m_mobs.push_back(id);
    }
    
    return id;
}

void ECS::RemoveEntity(Entity id) {
    if (id >= MAX_ENTITIES) return;
    m_activeEntities[m_numActive++] = id;
    
    // Remove from mob vector if present
    for (auto it = m_mobs.begin(); it != m_mobs.end(); ++it) {
        if (*it == id) {
            *it = m_mobs.back();
            m_mobs.pop_back();
            break;
        }
    }
    
    // Mark as inactive
    m_mobTypes[id] = { MobType::None };
}

Position& ECS::GetPosition(Entity id) {
    return m_positions[id];
}

const Position& ECS::GetPosition(Entity id) const {
    return m_positions[id];
}

Velocity& ECS::GetVelocity(Entity id) {
    return m_velocities[id];
}

const Velocity& ECS::GetVelocity(Entity id) const {
    return m_velocities[id];
}

Health& ECS::GetHealth(Entity id) {
    return m_healths[id];
}

const Health& ECS::GetHealth(Entity id) const {
    return m_healths[id];
}

MobType ECS::GetMobType(Entity id) const {
    return m_mobTypes[id].type;
}

PlayerInput& ECS::GetPlayerInput(Entity id) {
    return m_playerInputs[id];
}

const PlayerInput& ECS::GetPlayerInput(Entity id) const {
    return m_playerInputs[id];
}

void ECS::SpawnMobs() {
    // Spawn 1 mob per 4x4 chunk area, max 200, within 32 chunks
    const int maxChunks = 32;
    const int maxMobs = 200;
    
    for (int i = 0; i < maxMobs && i < maxChunks * maxChunks; ++i) {
        // Distribute mobs across chunks
        int chunkX = (i % maxChunks) * 4 + 2;
        int chunkZ = (i / maxChunks) * 4 + 2;
        
        Entity mob = AddEntity(MobType::Wanderer);
        m_positions[mob] = { (float)(chunkX * 16), 20.0f, (float)(chunkZ * 16) };
    }
}

Chunk& ECS::GetOrCreateChunk(int x, int y, int z) {
    // Simple chunk indexing: chunk coordinates * 16 to get block index
    int cx = ((x % 16) + 16) % 16; // Wrap to 0-15
    int cz = ((z % 16) + 16) % 16;
    int chunkIdx = (cx * 16) + cz; // Simple index
    
    // Clamp to valid range
    if (chunkIdx < 0) chunkIdx = 0;
    if (chunkIdx >= 4096) chunkIdx = 4095;
    
    return m_chunks[chunkIdx];
}

uint8_t ECS::GetBlock(int x, int y, int z) const {
    if (y < 0 || y > 255) return (uint8_t)0; // Air outside world
    
    int cx = ((x % 16) + 16) % 16;
    int cy = y;
    int cz = ((z % 16) + 16) % 16;
    int chunkIdx = (cx * 16) + cz;
    
    if (chunkIdx < 0 || chunkIdx >= 4096) return (uint8_t)0;
    
    // Simple block index within chunk: y * 256 + x*16 + z
    int blockIdx = y * 256 + cx * 16 + cz;
    if (blockIdx < 0 || blockIdx >= 4096) return (uint8_t)0;
    
    return m_chunks[chunkIdx].blocks[blockIdx];
}

void ECS::SetBlock(int x, int y, int z, uint8_t type) {
    if (y < 0 || y > 255) return;
    
    int cx = ((x % 16) + 16) % 16;
    int cy = y;
    int cz = ((z % 16) + 16) % 16;
    int chunkIdx = (cx * 16) + cz;
    
    if (chunkIdx < 0 || chunkIdx >= 4096) return;
    
    int blockIdx = y * 256 + cx * 16 + cz;
    if (blockIdx < 0 || blockIdx >= 4096) return;
    
    m_chunks[chunkIdx].blocks[blockIdx] = type;
}

void ECS::GenerateChunks() {
    // 2D value noise terrain
    // y=0 bedrock to y=64 sky
    
    for (int cx = 0; cx < 16; ++cx) {
        for (int cz = 0; cz < 16; ++cz) {
            int chunkIdx = (cx * 16) + cz;
            
            // Generate terrain height using simple noise
            float n = (float)(cx * 31 + cz * 71) / 256.0f;
            int height = (int)(64.0f * n) + 16;
            
            // Clamp height
            if (height < 0) height = 0;
            if (height > 64) height = 64;
            
            // Set blocks
            for (int y = 0; y <= height; ++y) {
                if (y == 0) {
                    // Bedrock
                    m_chunks[chunkIdx].blocks[y * 256 + cx * 16 + cz] = (uint8_t)9; // Bedrock
                } else if (y == 1) {
                    // Dirt
                    m_chunks[chunkIdx].blocks[y * 256 + cx * 16 + cz] = (uint8_t)3; // Dirt
                } else if (y == height) {
                    // Grass
                    m_chunks[chunkIdx].blocks[y * 256 + cx * 16 + cz] = (uint8_t)2; // Grass
                } else {
                    // Stone
                    m_chunks[chunkIdx].blocks[y * 256 + cx * 16 + cz] = (uint8_t)1; // Stone
                }
            }
        }
    }
}