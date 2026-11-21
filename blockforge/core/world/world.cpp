#include "chunk.h"
#include <unordered_map>
#include <algorithm>

// ============================================================
// World implementation
// ============================================================

World::World() = default;

const Chunk* World::GetChunk(uint64_t key) const {
    auto it = chunks_.find(key);
    if (it != chunks_.end()) return &it->second;
    return nullptr;
}

Chunk& World::GetOrCreateChunk(int cx, int cy, int cz) {
    uint64_t key = PackCoords(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it != chunks_.end()) return it->second;

    // Create new chunk if under limit
    if (chunks_.size() >= MAX_CHUNKS) {
        static Chunk dummy(-1, -1, -1);
        return dummy;
    }

    Chunk newChunk(cx, cy, cz);
    chunks_[key] = std::move(newChunk);
    return chunks_[key];
}

BlockType World::GetBlock(int x, int y, int z) {
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;

    int lx = x % CHUNK_SIZE;
    if (lx < 0) lx += CHUNK_SIZE;
    int ly = y % CHUNK_SIZE;
    if (ly < 0) ly += CHUNK_SIZE;
    int lz = z % CHUNK_SIZE;
    if (lz < 0) lz += CHUNK_SIZE;

    Chunk& chunk = GetOrCreateChunk(cx, cy, cz);
    return chunk.GetBlock(lx, ly, lz);
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;

    int lx = x % CHUNK_SIZE;
    if (lx < 0) lx += CHUNK_SIZE;
    int ly = y % CHUNK_SIZE;
    if (ly < 0) ly += CHUNK_SIZE;
    int lz = z % CHUNK_SIZE;
    if (lz < 0) lz += CHUNK_SIZE;

    Chunk& chunk = GetOrCreateChunk(cx, cy, cz);
    chunk.SetBlock(lx, ly, lz, type);
}

void World::GenerateChunk(int cx, int cy, int cz, uint64_t seed) {
    Chunk& chunk = GetOrCreateChunk(cx, cy, cz);

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            float wx = (cx * CHUNK_SIZE) + x;
            float vz = (cz * CHUNK_SIZE) + z;

            float n = ValueNoise(wx, vz, seed);

            int height = static_cast<int>(64.0f * n);

            if (height > 0) {
                chunk.SetBlock(x, 0, z, BlockType::Bedrock);
                for (int y = 1; y < height - 1; ++y) {
                    chunk.SetBlock(x, y, z, BlockType::Stone);
                }
                if (height > 1) {
                    chunk.SetBlock(x, height - 1, z, BlockType::Dirt);
                }
                if (height > 0) {
                    chunk.SetBlock(x, height, z, BlockType::Grass);
                }
            }
        }
    }
    chunk.dirty = false;
}

uint64_t World::PackCoords(int cx, int cy, int cz) {
    return static_cast<uint64_t>(cx) | (static_cast<uint64_t>(cy) << 24) | (static_cast<uint64_t>(cz) << 40);
}

void World::UnpackCoords(uint64_t key, int& cx, int& cy, int& cz) {
    cx = static_cast<int>(key & 0xFFFF);
    cy = static_cast<int>((key >> 24) & 0xFF);
    cz = static_cast<int>((key >> 40) & 0xFFFF);
}

Chunk& World::operator[](uint64_t key) {
    return chunks_.at(key);
}

const Chunk& World::operator[](uint64_t key) const {
    return chunks_.at(key);
}

std::vector<uint64_t> World::GetDirtyChunks() const {
    std::vector<uint64_t> result;
    for (const auto& [key, chunk] : chunks_) {
        if (chunk.dirty) {
            result.push_back(key);
        }
    }
    return result;
}

void World::AddDirtyChunk(uint64_t key) {
    if (dirtyChunks_.size() >= 256) {
        dirtyChunks_.erase(dirtyChunks_.begin());
    }
    dirtyChunks_.push_back(key);
}

const std::vector<uint64_t>& World::GetDirtyChunksRing() const { return dirtyChunks_; }

void World::ClearDirtyFlags() {
    for (auto& [key, chunk] : chunks_) {
        chunk.dirty = false;
    }
    dirtyChunks_.clear();
}

size_t World::GetChunkCount() const { return chunks_.size(); }