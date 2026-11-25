#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

// Block palette. All artwork for these blocks is generated procedurally at
// runtime (see OpenGLRenderer::generateTextureAtlas), so the game ships with its
// own original textures.
enum class BlockType : uint8_t {
    Air = 0,
    // Upperworld materials
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
    // Underworld materials
    Emberstone,
    Ashsoil,
    Lumenstone,
    Magma,
    Voidstone,
    Cinderbrick,
    PaleQuartz,
    CrimsonWood,
    Bloodleaves,
    Emberlode,
    Ironlode,
    Portal,
    Glowmoss,
    Cinderstone,
    Ashblock,
    Count
};

static_assert(static_cast<uint8_t>(BlockType::Count) <= 64,
              "BlockType count must fit inside the 16x16 tile atlas");

// The two playable realms.
enum class Dimension : uint8_t {
    Upperworld = 0,
    Underworld = 1,
    Count
};

inline const char* DimensionName(Dimension dimension) {
    switch (dimension) {
        case Dimension::Underworld: return "Underworld";
        case Dimension::Upperworld: return "Upperworld";
        default: return "Unknown";
    }
}

// Vertical generation limits (kept in the header so spawning code can use them).
static constexpr int UPPERWORLD_TOP = 96;      // upperworld is generated for y in [0, 96)
static constexpr int UNDERWORLD_CEILING = 120; // bedrock roof of the underworld starts here
static constexpr int UNDERWORLD_TOP = 128;     // underworld is generated for y in [0, 128)
static constexpr int SEA_LEVEL = 34;

// Collision / rendering classification shared by physics, meshing and the HUD.
inline bool IsLiquid(BlockType type) { return type == BlockType::Water; }
inline bool IsPortal(BlockType type) { return type == BlockType::Portal; }
inline bool IsCollidable(BlockType type) {
    return type != BlockType::Air && !IsLiquid(type) && !IsPortal(type);
}
// Opaque blocks hide the neighbouring face. Leaves stay fully opaque so the
// renderer never needs alpha sorting.
inline bool IsOpaque(BlockType type) { return IsCollidable(type); }
inline bool EmitsLight(BlockType type) {
    return type == BlockType::Portal || type == BlockType::Lumenstone ||
           type == BlockType::Magma || type == BlockType::Glowmoss;
}
inline bool IsBreakable(BlockType type) {
    return type != BlockType::Air && type != BlockType::Bedrock;
}

// Chunk: 16x16x16 blocks.
static constexpr int CHUNK_SIZE = 16;
static constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 4096

using BlockId = uint8_t;

struct Chunk {
    BlockId blocks[CHUNK_VOLUME];

    // Chunk grid coordinates.
    int cx, cy, cz;

    explicit Chunk(int cx = 0, int cy = 0, int cz = 0);

    BlockType GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, BlockType type);
    bool IsSolid(int x, int y, int z) const;
};

// World: sparse chunk storage, world-space block access and terrain generation
// for both the Upperworld and the Underworld.
class World {
public:
    World();

    Chunk& GetOrCreateChunk(int cx, int cy, int cz);
    BlockType GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, BlockType type);

    // Generate the terrain column (X/Z chunk coords) for the active dimension.
    void GenerateChunk(int cx, int cz);

    // Generate terrain in a square radius around the origin.
    void GenerateChunks(int radiusInChunks = 6);

    // Generate terrain in a square radius around a chunk column.
    void GenerateChunksAround(int centerCX, int centerCZ, int radiusInChunks = 6);

    // Drop every generated chunk (used when travelling between dimensions).
    void Clear();

    Dimension GetDimension() const { return m_dimension; }
    void SetDimension(Dimension dimension) { m_dimension = dimension; }

    uint64_t GetSeed() const { return m_seed; }
    void SetSeed(uint64_t seed) { m_seed = seed; }

    // First open standing position (feet) above the terrain at column x/z.
    int SurfaceY(int x, int z) const;

    size_t GetChunkCount() const;
    const std::unordered_map<uint64_t, Chunk>& GetChunks() const { return m_chunks; }

    static uint64_t PackKey(int cx, int cy, int cz);
    static void UnpackKey(uint64_t key, int& cx, int& cy, int& cz);

private:
    std::unordered_map<uint64_t, Chunk> m_chunks;
    Dimension m_dimension = Dimension::Upperworld;
    uint64_t m_seed = 1337;

    int ColumnTop() const;
    float ColumnHeight(int wx, int wz) const;
    bool CaveAt(int wx, int wy, int wz) const;

    void GenerateUpperworldColumn(int cx, int cz);
    void GenerateUnderworldColumn(int cx, int cz);
    void DecorateChunk(int cx, int cz);
    void DecorateUpperworldColumn(int cx, int cz);
};
