#include "chunk.h"
#include <cmath>

uint64_t World::PackCoords(int cx, int cy, int cz) {
    return ((uint64_t)(uint32_t)cx << 32) | ((uint64_t)(uint32_t)cy << 16) | (uint32_t)cz;
}

void World::UnpackCoords(uint64_t key, int& cx, int& cy, int& cz) {
    cx = (int)((key >> 32) & 0xFFFFFFFF);
    cy = (int)((key >> 16) & 0xFFFF);
    cz = (int)(key & 0xFFFF);
}

Chunk& World::GetOrCreateChunk(int cx, int cy, int cz) {
    uint64_t key = PackCoords(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it != chunks_.end()) return it->second;
    auto result = chunks_.emplace(key, Chunk(cx, cy, cz));
    return result.first->second;
}

const Chunk* World::GetChunk(int cx, int cy, int cz) const {
    uint64_t key = PackCoords(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it != chunks_.end()) return &it->second;
    return nullptr;
}

BlockType World::GetBlock(int x, int y, int z) {
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;

    const Chunk* chunk = GetChunk(cx, cy, cz);
    if (!chunk) return BlockType::Air;
    return chunk->GetBlock(lx, ly, lz);
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;

    Chunk& chunk = GetOrCreateChunk(cx, cy, cz);
    chunk.SetBlock(lx, ly, lz, type);
}

float World::ValueNoise(float x, float z) const {
    int xi = (int)floorf(x);
    int zi = (int)floorf(z);
    float xf = x - xi;
    float zf = z - zi;

    uint32_t h1 = (uint32_t)(xi * 374761393 + zi * 668265263 + seed_);
    uint32_t h2 = (uint32_t)((xi+1) * 374761393 + zi * 668265263 + seed_);
    uint32_t h3 = (uint32_t)(xi * 374761393 + (zi+1) * 668265263 + seed_);
    uint32_t h4 = (uint32_t)((xi+1) * 374761393 + (zi+1) * 668265263 + seed_);

    float n1 = (float)(h1 & 0xFFFF) / 65535.0f;
    float n2 = (float)(h2 & 0xFFFF) / 65535.0f;
    float n3 = (float)(h3 & 0xFFFF) / 65535.0f;
    float n4 = (float)(h4 & 0xFFFF) / 65535.0f;

    float fx = xf * xf * (3.0f - 2.0f * xf);
    float fz = zf * zf * (3.0f - 2.0f * zf);

    float a = n1 + fx * (n2 - n1);
    float b = n3 + fx * (n4 - n3);
    return a + fz * (b - a);
}

void World::GenerateChunk(int cx, int cy, int cz) {
    if (cy != 0) return;

    Chunk& chunk = GetOrCreateChunk(cx, cy, cz);

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            float wx = (float)(cx * CHUNK_SIZE + x) * 0.05f;
            float wz = (float)(cz * CHUNK_SIZE + z) * 0.05f;
            float n = ValueNoise(wx, wz);
            int height = (int)(n * 40.0f) + 10;

            if (height > CHUNK_SIZE) height = CHUNK_SIZE;
            if (height < 1) height = 1;

            chunk.SetBlock(x, 0, z, BlockType::Bedrock);
            for (int y = 1; y < height - 1; ++y)
                chunk.SetBlock(x, y, z, BlockType::Stone);
            if (height > 1)
                chunk.SetBlock(x, height - 1, z, BlockType::Dirt);
            chunk.SetBlock(x, height, z, BlockType::Grass);
        }
    }
    chunk.dirty = false;
}

void World::GenerateArea(int cx, int cy, int cz, int radius) {
    for (int dx = -radius; dx <= radius; ++dx)
        for (int dz = -radius; dz <= radius; ++dz)
            GenerateChunk(cx + dx, cy, cz + dz);
}

std::vector<uint64_t> World::GetDirtyChunks() const {
    std::vector<uint64_t> result;
    for (auto& pair : chunks_) {
        if (pair.second.dirty) result.push_back(pair.first);
    }
    return result;
}

void World::ClearDirtyFlags() {
    for (auto& pair : chunks_) pair.second.dirty = false;
}   