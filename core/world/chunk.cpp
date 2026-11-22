#include "world/chunk.h"
#include <cmath>
#include <unordered_map>

// Chunk coordinates modulo the unordered_map lookup
// The map uses packed chunk coordinates as keys

Chunk& World::GetOrCreateChunk(int x, int y, int z) {
    // Pack chunk coordinates into a key
    uint64_t key = PackKey(x, y, z);

    // Try to find the chunk, create if not exists
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) {
        return it->second;
    }

    // Create new chunk if under limit (unordered_map grows dynamically)
    Chunk newChunk(x, y, z);
    m_chunks[key] = std::move(newChunk);

    // Return reference to the newly created chunk
    return m_chunks[key];
}

BlockType World::GetBlock(int x, int y, int z) const {
    if (y < 0 || y > 255) return BlockType::Air;

    // Pack chunk coordinates into a key
    uint64_t key = PackKey(x, y, z);

    // Look up the chunk
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) return BlockType::Air;

    return it->second.GetBlock(x % CHUNK_SIZE, y, z % CHUNK_SIZE);
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    if (y < 0 || y > 255) return;

    // Pack chunk coordinates into a key
    uint64_t key = PackKey(x, y, z);

    // Look up the chunk, create if not exists
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        // Create new chunk
        Chunk newChunk;
        // Set the block in the new chunk
        // We need to set the local position within the chunk
        int localX = x % CHUNK_SIZE;
        if (localX < 0) localX += CHUNK_SIZE;
        int localY = y;
        int localZ = z % CHUNK_SIZE;
        if (localZ < 0) localZ += CHUNK_SIZE;
        newChunk.SetBlock(localX, localY, localZ, type);
        m_chunks[key] = std::move(newChunk);
    } else {
        // Chunk exists, set the block
        it->second.SetBlock(x % CHUNK_SIZE, y, z % CHUNK_SIZE, type);
    }
}

void World::GenerateChunk(int cx, int cz) {
    // 2D value noise terrain
    // y=0 bedrock to y=64 sky
    // cx,cz are chunk coordinates in the XZ plane

    // Generate chunk key
    uint64_t key = PackKey(cx, 0, cz);

    // Look up or create the chunk
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        // Create new chunk
        Chunk newChunk;
        m_chunks[key] = std::move(newChunk);
        it = m_chunks.find(key);
    }

    // Generate terrain
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            // World coordinates relative to chunk origin
            float wx = (cx * CHUNK_SIZE) + x + 0.5f;
            float vz = (cz * CHUNK_SIZE) + z + 0.5f;

            // Simple 2D value noise
            int xi = (int)floor(wx);
            int zi = (int)floor(vz);
            float xf = wx - xi;
            float zf = vz - zi;

            // Wrap using large primes for noise
            xi = (xi & 0x7fffffff) ^ (xi >> 13);
            zi = (zi & 0x7fffffff) ^ (zi >> 13);

            // Hash
            uint32_t h1 = xi * 61 + zi * 17;
            uint32_t h2 = xi * 31 + zi * 67;

            // Gradient noise approximation
            float n1 = (h1 & 0x3fffffff) / 1073741823.0f;
            float n2 = (h2 & 0x3fffffff) / 1073741823.0f;

            // Cubic fade
            float fx = 3.0f * xf * xf - 2.0f * xf * xf * xf;
            float fz = 3.0f * zf * zf - 2.0f * zf * zf * zf;
            float noiseVal = n1 * (1.0f - fx) + n2 * fx;

            // Height: 0 (bedrock) to 64 (sky)
            int height = (int)(64.0f * noiseVal);
            if (height < 0) height = 0;
            if (height > 64) height = 64;

            // Set blocks from bottom to top
            for (int y = 0; y <= height; ++y) {
                if (y == 0) {
                    // Bedrock at y=0
                    it->second.SetBlock(x, y, z, BlockType::Bedrock);
                } else if (y == 1) {
                    // Dirt
                    it->second.SetBlock(x, y, z, BlockType::Dirt);
                } else if (y == height) {
                    // Grass top
                    it->second.SetBlock(x, y, z, BlockType::Grass);
                } else {
                    // Stone
                    it->second.SetBlock(x, y, z, BlockType::Stone);
                }
            }
        }
    }
}

float World::ValueNoise(float x, float z) const {
    int xi = (int)floor(x);
    int zi = (int)floor(z);
    float xf = x - xi;
    float zf = z - zi;

    xi = (xi & 0x7fffffff);
    zi = (zi & 0x7fffffff);

    uint32_t h1 = xi * 61 + zi * 17;
    uint32_t h2 = xi * 31 + zi * 67;

    float n1 = (h1 & 0x3fffffff) / 1073741823.0f;
    float n2 = (h2 & 0x3fffffff) / 1073741823.0f;

    float fx = 3.0f * xf * xf - 2.0f * xf * xf * xf;
    float fz = 3.0f * zf * zf - 2.0f * zf * zf * zf;
    return n1 * (1.0f - fx) + n2 * fx;
}