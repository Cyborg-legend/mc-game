#pragma once

#include "core/ecs/world.h"
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <glm/glm.hpp>

// BlockType enum: Air=0, Stone, Dirt, Grass, Wood, Leaves, Sand, Water, Bedrock, Planks, Sticks, CraftingTable, Brick (max 16 types)
enum class BlockType : uint8_t {
    Air = 0,
    Stone,
    Dirt,
    Grass,
    Wood,
    Leaves,
    Sand,
    Water,
    Bedrock,
    Planks,
    Sticks,
    CraftingTable,
    Brick,
    // Keep last for array sizing - max 16 types
    Count = 16
};

// Static assertion to ensure we don't exceed 16 block types
static_assert(static_cast<uint8_t>(BlockType::Count) <= 16, "BlockType count must not exceed 16");

// Chunk: 16×16×16 blocks
static constexpr int CHUNK_SIZE = 16;
static constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 4096

using BlockId = uint8_t;

struct Chunk {
    BlockId blocks[CHUNK_VOLUME]; // uint8_t blocks[16*16*16]

    // Chunk coordinates
    int cx, cy, cz; // chunk grid coordinates

    // Dirty flag: true if mesh needs regeneration
    bool dirty = true;

    // Timestamp for dirty tracking
    uint64_t dirtyTime = 0;

    // Construct chunk at given coordinates
    Chunk(int cx, int cy, int cz) : cx(cx), cy(cy), cz(cz) {
        // Initialize all blocks to Air
        std::fill_n(blocks, CHUNK_VOLUME, static_cast<BlockId>(BlockType::Air));
    }

    // Get block type at local position (x,y,z within chunk)
    BlockType GetBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
            return BlockType::Bedrock; // Out of bounds
        }
        return static_cast<BlockType>(blocks[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x]);
    }

    // Set block type at local position (marks chunk dirty)
    void SetBlock(int x, int y, int z, BlockType type) {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
            blocks[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x] = static_cast<BlockId>(type);
            dirty = true;
        }
    }

    // Get packed chunk key for map lookup
    uint64_t GetKey() const {
        return static_cast<uint64_t>(cx) | (static_cast<uint64_t>(cy) << 24) | (static_cast<uint64_t>(cz) << 40);
    }
};

// World class for chunk management
class World {
public:
    World() = default;

    // Get chunk by packed coordinates, returns nullptr if not exists
    const Chunk* GetChunk(uint64_t key) const {
        auto it = chunks_.find(key);
        if (it != chunks_.end()) return &it->second;
        return nullptr;
    }

    Chunk& GetOrCreateChunk(int cx, int cy, int cz) {
        uint64_t key = PackCoords(cx, cy, cz);
        auto it = chunks_.find(key);
        if (it != chunks_.end()) return it->second;

        // Create new chunk if under limit
        if (chunks_.size() >= MAX_CHUNKS) {
            // This shouldn't happen in normal gameplay, but handle gracefully
            static Chunk dummy(-1, -1, -1);
            return dummy;
        }

        Chunk newChunk(cx, cy, cz);
        chunks_[key] = std::move(newChunk);
        return chunks_[key];
    }

    // Get block at world position
    BlockType GetBlock(int x, int y, int z) {
        // Calculate chunk coordinates
        int cx = x / CHUNK_SIZE;
        int cy = y / CHUNK_SIZE;
        int cz = z / CHUNK_SIZE;

        // Calculate local position within chunk
        int lx = x % CHUNK_SIZE;
        if (lx < 0) lx += CHUNK_SIZE;
        int ly = y % CHUNK_SIZE;
        if (ly < 0) ly += CHUNK_SIZE;
        int lz = z % CHUNK_SIZE;
        if (lz < 0) lz += CHUNK_SIZE;

        Chunk& chunk = GetOrCreateChunk(cx, cy, cz);
        return chunk.GetBlock(lx, ly, lz);
    }

    // Set block at world position
    void SetBlock(int x, int y, int z, BlockType type) {
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

    // Generate terrain using 2D value noise inline
    void GenerateChunk(int cx, int cy, int cz, uint64_t seed) {
        Chunk& chunk = GetOrCreateChunk(cx, cy, cz);

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                // Calculate world coordinates relative to chunk
                float wx = (cx * CHUNK_SIZE) + x;
                float vz = (cz * CHUNK_SIZE) + z;

                // Simple 2D value noise
                float n = ValueNoise(wx, vz, seed);

                // Determine height based on noise
                // Height range: y=0 (bedrock) to y=64 (sky)
                int height = static_cast<int>(64.0f * n); // 0-64

                // Set bedrock at bottom
                if (height > 0) {
                    chunk.SetBlock(x, 0, z, BlockType::Bedrock);
                    // Set stone up to height-2
                    for (int y = 1; y < height - 1; ++y) {
                        chunk.SetBlock(x, y, z, BlockType::Stone);
                    }
                    // Dirt layer
                    if (height > 1) {
                        chunk.SetBlock(x, height - 1, z, BlockType::Dirt);
                    }
                    // Grass top
                    if (height > 0) {
                        chunk.SetBlock(x, height, z, BlockType::Grass);
                    }
                }
            }
        }
        chunk.dirty = false;
    }

    // Get packed chunk coordinates
    static uint64_t PackCoords(int cx, int cy, int cz) {
        return static_cast<uint64_t>(cx) | (static_cast<uint64_t>(cy) << 24) | (static_cast<uint64_t>(cz) << 40);
    }

    static void UnpackCoords(uint64_t key, int& cx, int& cy, int& cz) {
        cx = static_cast<int>(key & 0xFFFF);
        cy = static_cast<int>((key >> 24) & 0xFF);
        cz = static_cast<int>((key >> 40) & 0xFFFF);
    }

    // Access chunk by key
    Chunk& operator[](uint64_t key) {
        return chunks_.at(key);
    }

    // Constant access
    const Chunk& operator[](uint64_t key) const {
        return chunks_.at(key);
    }

    // Get all dirty chunks (copy)
    std::vector<uint64_t> GetDirtyChunks() const {
        std::vector<uint64_t> result;
        for (const auto& [key, chunk] : chunks_) {
            if (chunk.dirty) {
                result.push_back(key);
            }
        }
        return result;
    }

    // Ring buffer for dirty chunks (max 256)
    void AddDirtyChunk(uint64_t key) {
        if (dirtyChunks_.size() >= 256) {
            // Ring buffer: overwrite oldest (first element)
            dirtyChunks_.erase(dirtyChunks_.begin());
        }
        dirtyChunks_.push_back(key);
    }

    // Get dirty chunks ring buffer
    const std::vector<uint64_t>& GetDirtyChunksRing() const { return dirtyChunks_; }

    // Clear all dirty flags
    void ClearDirtyFlags() {
        for (auto& [key, chunk] : chunks_) {
            chunk.dirty = false;
        }
        dirtyChunks_.clear();
    }

    // Get chunk count
    size_t GetChunkCount() const { return chunks_.size(); }

    // Maximum pre-allocated chunks
    static constexpr size_t MAX_CHUNKS = 4096;

private:
    // Simple 2D value noise inline
    static float ValueNoise(float x, float z, uint64_t seed) {
        // Convert to integer coordinates
        int xi = static_cast<int>(floor(x));
        int zi = static_cast<int>(floor(z));
        float xf = x - xi;
        float zf = z - zi;

        // Wrap around using seed
        xi = (xi & 0x7fffffff) ^ (seed & 0x7fffffff);
        zi = (zi & 0x7fffffff) ^ (seed & 0x7fffffff);

        // Simple hash
        uint32_t h1 = xi + zi * 61 + seed * 17;
        uint32_t h2 = xi * 31 + zi + seed * 67;

        // Simple gradient noise approximation
        float n1 = (h1 & 0x3fffffff) / 1073741823.0f; // 0 to ~1
        float n2 = (h2 & 0x3fffffff) / 1073741823.0f; // 0 to ~1

        // Interpolate
        float fx = 3.0f * xf * xf - 2.0f * xf * xf * xf; // cubic fade
        float fz = 3.0f * zf * zf - 2.0f * zf * zf * zf;
        float result = n1 * (1.0f - fx) + n2 * fx;

        return result;
    }

    // D1 database chunks map: key -> chunk
    std::unordered_map<uint64_t, Chunk> chunks_;

    // Dirty chunk tracking ring buffer
    std::vector<uint64_t> dirtyChunks_;
};