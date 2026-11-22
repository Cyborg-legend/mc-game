#pragma once

#include <cstdint>
#include <array>
#include <unordered_map>

// BlockType enum matching spec: Air, Stone, Dirt, Grass, Wood, Leaves, Sand, Water, Bedrock, Planks, Sticks, CraftingTable, Brick (14 types)
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

    // Construct chunk at given coordinates
    Chunk(int cx = 0, int cy = 0, int cz = 0) : cx(cx), cy(cy), cz(cz) {
        // Initialize all blocks to Air
        for (int i = 0; i < CHUNK_VOLUME; ++i) {
            blocks[i] = static_cast<BlockId>(BlockType::Air);
        }
    }

    // Get block type at local position (x,y,z within chunk)
    BlockType GetBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
            return BlockType::Air; // Out of bounds
        }
        return static_cast<BlockType>(blocks[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x]);
    }

    // Set block type (marks chunk modified)
    void SetBlock(int x, int y, int z, BlockType type) {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
            blocks[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x] = static_cast<BlockId>(type);
        }
    }
};

// World class for chunk management
class World {
public:
    World();
    ~World() = default;

    // Get chunk by coordinates, creates if not exists
    Chunk& GetOrCreateChunk(int x, int y, int z);

    // Get block at world position
    BlockType GetBlock(int x, int y, int z) const;

    // Set block at world position
    void SetBlock(int x, int y, int z, BlockType type);

    // Generate terrain chunk using 2D value noise
    // x,z = chunk coordinates, y ranges from bedrock (y=0) to sky (y=64)
    void GenerateChunk(int cx, int cz);

    // Get all chunks (for iteration)
    size_t GetChunkCount() const;

private:
    // Use unordered_map instead of std::array to avoid 16MB stack allocation
    // Only stores chunks that are actually used
    std::unordered_map<uint64_t, Chunk> m_chunks;

    // Simple 2D value noise
    float ValueNoise(float x, float z) const;

    // Pack chunk coordinates into key
    uint32_t PackKey(int cx, int cy, int cz) const;
};