#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

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
    Count = 14
};

static constexpr int CHUNK_SIZE = 16;
static constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct Chunk {
    uint8_t blocks[CHUNK_VOLUME];
    int cx, cy, cz;
    bool dirty = true;

    Chunk() : cx(0), cy(0), cz(0) {
        for (int i = 0; i < CHUNK_VOLUME; ++i) blocks[i] = 0;
    }

    Chunk(int cx, int cy, int cz) : cx(cx), cy(cy), cz(cz) {
        for (int i = 0; i < CHUNK_VOLUME; ++i) blocks[i] = 0;
    }

    BlockType GetBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
            return BlockType::Bedrock;
        return (BlockType)blocks[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x];
    }

    void SetBlock(int x, int y, int z, BlockType type) {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
            blocks[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x] = (uint8_t)type;
            dirty = true;
        }
    }
};

class World {
public:
    World() : seed_(12345) {}

    void SetSeed(uint64_t seed) { seed_ = seed; }
    uint64_t GetSeed() const { return seed_; }

    Chunk& GetOrCreateChunk(int cx, int cy, int cz);
    const Chunk* GetChunk(int cx, int cy, int cz) const;

    BlockType GetBlock(int x, int y, int z);
    void SetBlock(int x, int y, int z, BlockType type);

    void GenerateChunk(int cx, int cy, int cz);
    void GenerateArea(int cx, int cy, int cz, int radius);

    size_t GetChunkCount() const { return chunks_.size(); }
    std::vector<uint64_t> GetDirtyChunks() const;
    void ClearDirtyFlags();

    static uint64_t PackCoords(int cx, int cy, int cz);
    static void UnpackCoords(uint64_t key, int& cx, int& cy, int& cz);

private:
    float ValueNoise(float x, float z) const;

    std::unordered_map<uint64_t, Chunk> chunks_;
    uint64_t seed_;
};   