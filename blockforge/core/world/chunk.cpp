#include "world/chunk.h"
#include <cmath>

// Chunk coordinates modulo the fixed pool size
// Pool is 4096 chunks = 64x64 grid in XZ, unlimited Y (but we use 64 for simplicity)

Chunk& World::GetOrCreateChunk(int x, int y, int z) {
    // Map world coordinates to chunk grid indices within the 64x64x64 pool
    // Chunk size is 16 blocks
    int cx = (x / CHUNK_SIZE) % 64;
    if (cx < 0) cx += 64;
    int cy = (y / CHUNK_SIZE); // Can be negative or positive
    int cz = (z / CHUNK_SIZE) % 64;
    if (cz < 0) cz += 64;

    // Flat index into the 4096 chunk pool (64*64 layout in XZ)
    int chunkIdx = (cx * 64) + cz;

    // If y is negative, we still use the same XZ chunk but different "layer"
    // For simplicity, use cy modulo 64 for the Y direction too
    // But we have only 4096 chunks total, so we pack XZ into 64*64 and ignore Y pooling
    // Actually, let's just use cx and cz for the 4096 pool, y is ignored for indexing
    // and we always modify the same Y layer. For a proper game, Y would be another dimension.
    // For now, just use the XZ plane:
    chunkIdx = (cx * 64) + cz;

    return m_chunks[chunkIdx];
}

BlockType World::GetBlock(int x, int y, int z) const {
    if (y < 0 || y > 255) return BlockType::Air;

    int cx = (x / CHUNK_SIZE) % 64;
    if (cx < 0) cx += 64;
    int cz = (z / CHUNK_SIZE) % 64;
    if (cz < 0) cz += 64;

    int chunkIdx = (cx * 64) + cz;
    if (chunkIdx < 0 || chunkIdx >= 4096) return BlockType::Air;

    return m_chunks[chunkIdx].GetBlock(x % CHUNK_SIZE, y, z % CHUNK_SIZE);
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    if (y < 0 || y > 255) return;

    int cx = (x / CHUNK_SIZE) % 64;
    if (cx < 0) cx += 64;
    int cz = (z / CHUNK_SIZE) % 64;
    if (cz < 0) cz += 64;

    int chunkIdx = (cx * 64) + cz;
    if (chunkIdx < 0 || chunkIdx >= 4096) return;

    m_chunks[chunkIdx].SetBlock(x % CHUNK_SIZE, y, z % CHUNK_SIZE, type);
}

void World::GenerateChunk(int cx, int cz) {
    // 2D value noise terrain
    // y=0 bedrock to y=64 sky
    // cx,cz are chunk coordinates in the XZ plane

    // Compute chunk index from cx,cz within the 4096 chunk pool (64x64 grid)
    int chunkIdx = ((cx % 64) + 64) % 64 * 64 + ((cz % 64) + 64) % 64;
    if (chunkIdx < 0) chunkIdx = 0;
    if (chunkIdx >= 4096) chunkIdx = 4095;

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
                    m_chunks[chunkIdx].SetBlock(x, y, z, BlockType::Bedrock);
                } else if (y == 1) {
                    // Dirt
                    m_chunks[chunkIdx].SetBlock(x, y, z, BlockType::Dirt);
                } else if (y == height) {
                    // Grass top
                    m_chunks[chunkIdx].SetBlock(x, y, z, BlockType::Grass);
                } else {
                    // Stone
                    m_chunks[chunkIdx].SetBlock(x, y, z, BlockType::Stone);
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