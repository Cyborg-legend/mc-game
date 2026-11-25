#include "world/chunk.h"

#include <cmath>

namespace {

uint32_t Mix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

uint32_t Hash2(int x, int z) {
    return Mix32(static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(z) * 668265263u + 0x9e3779b9u);
}

uint32_t Hash3(int x, int y, int z) {
    return Mix32(Hash2(x, z) ^ (static_cast<uint32_t>(y) * 2246822519u));
}

float Hash01(int x, int z) {
    return static_cast<float>(Hash2(x, z) & 0xFFFFFFu) / 16777215.0f;
}

float Hash01_3(int x, int y, int z) {
    return static_cast<float>(Hash3(x, y, z) & 0xFFFFFFu) / 16777215.0f;
}

float Fade(float t) { return t * t * (3.0f - 2.0f * t); }
float Lerp(float a, float b, float t) { return a + (b - a) * t; }

float ValueNoise2(float x, float z) {
    const int xi = static_cast<int>(std::floor(x));
    const int zi = static_cast<int>(std::floor(z));
    const float xf = x - static_cast<float>(xi);
    const float zf = z - static_cast<float>(zi);

    const float n00 = Hash01(xi, zi);
    const float n10 = Hash01(xi + 1, zi);
    const float n01 = Hash01(xi, zi + 1);
    const float n11 = Hash01(xi + 1, zi + 1);

    const float u = Fade(xf);
    const float v = Fade(zf);
    return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);
}

float ValueNoise3(float x, float y, float z) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const int zi = static_cast<int>(std::floor(z));
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const float zf = z - static_cast<float>(zi);

    const float u = Fade(xf);
    const float v = Fade(yf);
    const float w = Fade(zf);

    const float c000 = Hash01_3(xi, yi, zi);
    const float c100 = Hash01_3(xi + 1, yi, zi);
    const float c010 = Hash01_3(xi, yi + 1, zi);
    const float c110 = Hash01_3(xi + 1, yi + 1, zi);
    const float c001 = Hash01_3(xi, yi, zi + 1);
    const float c101 = Hash01_3(xi + 1, yi, zi + 1);
    const float c011 = Hash01_3(xi, yi + 1, zi + 1);
    const float c111 = Hash01_3(xi + 1, yi + 1, zi + 1);

    const float x00 = Lerp(c000, c100, u);
    const float x10 = Lerp(c010, c110, u);
    const float x01 = Lerp(c001, c101, u);
    const float x11 = Lerp(c011, c111, u);

    return Lerp(Lerp(x00, x10, v), Lerp(x01, x11, v), w);
}

} // namespace

Chunk::Chunk(int cx, int cy, int cz) : cx(cx), cy(cy), cz(cz) {
    for (int i = 0; i < CHUNK_VOLUME; ++i) {
        blocks[i] = static_cast<BlockId>(BlockType::Air);
    }
}

BlockType Chunk::GetBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return BlockType::Air;
    }
    return static_cast<BlockType>(blocks[(y * CHUNK_SIZE + z) * CHUNK_SIZE + x]);
}

void Chunk::SetBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return;
    }
    blocks[(y * CHUNK_SIZE + z) * CHUNK_SIZE + x] = static_cast<BlockId>(type);
}

bool Chunk::IsSolid(int x, int y, int z) const {
    return IsCollidable(GetBlock(x, y, z));
}

uint64_t World::PackKey(int cx, int cy, int cz) {
    const uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(cx) & 0x1FFFFFu);
    const uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(cy) & 0x1FFFFFu);
    const uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(cz) & 0x1FFFFFu);
    return ux | (uy << 21) | (uz << 42);
}

void World::UnpackKey(uint64_t key, int& cx, int& cy, int& cz) {
    auto signExtend = [](uint64_t v) -> int {
        // Expand 21 bits back to a signed int.
        if (v & 0x100000u) {
            return static_cast<int>(v | 0xFFFFFFFFFFE00000ull);
        }
        return static_cast<int>(v);
    };
    cx = signExtend(key & 0x1FFFFFull);
    cy = signExtend((key >> 21) & 0x1FFFFFull);
    cz = signExtend((key >> 42) & 0x1FFFFFull);
}

World::World() = default;

Chunk& World::GetOrCreateChunk(int cx, int cy, int cz) {
    const uint64_t key = PackKey(cx, cy, cz);
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) {
        return it->second;
    }
    auto res = m_chunks.emplace(key, Chunk(cx, cy, cz));
    return res.first->second;
}

BlockType World::GetBlock(int x, int y, int z) const {
    if (y < 0 || y >= UNDERWORLD_TOP) {
        return BlockType::Air;
    }
    const int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
    const int cy = y / CHUNK_SIZE;
    const int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);

    const uint64_t key = PackKey(cx, cy, cz);
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        return BlockType::Air;
    }

    const int lx = x - cx * CHUNK_SIZE;
    const int ly = y - cy * CHUNK_SIZE;
    const int lz = z - cz * CHUNK_SIZE;
    return it->second.GetBlock(lx, ly, lz);
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    if (y < 0 || y >= UNDERWORLD_TOP) {
        return;
    }
    const int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
    const int cy = y / CHUNK_SIZE;
    const int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);

    Chunk& chunk = GetOrCreateChunk(cx, cy, cz);
    chunk.SetBlock(x - cx * CHUNK_SIZE, y - cy * CHUNK_SIZE, z - cz * CHUNK_SIZE, type);
}

void World::Clear() {
    m_chunks.clear();
}

int World::ColumnTop() const {
    return (m_dimension == Dimension::Underworld) ? UNDERWORLD_TOP : UPPERWORLD_TOP;
}

float World::ColumnHeight(int wx, int wz) const {
    if (m_dimension == Dimension::Underworld) {
        // Jagged emberstone ridges with lava lakes in between.
        const float detail = ValueNoise2(static_cast<float>(wx) * 0.030f,
                                         static_cast<float>(wz) * 0.030f);
        const float ridges = ValueNoise2(static_cast<float>(wx) * 0.010f + 91.3f,
                                         static_cast<float>(wz) * 0.010f - 17.7f);
        return 46.0f + detail * 26.0f + ridges * 14.0f; // 46..86
    }

    const float detail = ValueNoise2(static_cast<float>(wx) * 0.035f,
                                     static_cast<float>(wz) * 0.035f);
    const float hills = ValueNoise2(static_cast<float>(wx) * 0.011f + 13.7f,
                                    static_cast<float>(wz) * 0.011f - 7.1f);
    return 24.0f + detail * 20.0f + hills * 16.0f; // 24..60
}

bool World::CaveAt(int wx, int wy, int wz) const {
    const float fx = static_cast<float>(wx);
    const float fy = static_cast<float>(wy);
    const float fz = static_cast<float>(wz);

    if (m_dimension == Dimension::Underworld) {
        const float n = ValueNoise3(fx * 0.055f, fy * 0.070f, fz * 0.055f);
        if (n < 0.46f) return false;
        if (n > 0.66f) return true;
        const float m = ValueNoise3(fx * 0.110f + 33.0f, fy * 0.130f, fz * 0.110f - 9.0f);
        return n * 0.65f + m * 0.35f > 0.58f;
    }

    const float n = ValueNoise3(fx * 0.060f, fy * 0.090f, fz * 0.060f);
    if (n < 0.50f) return false;
    if (n > 0.70f) return true;
    const float m = ValueNoise3(fx * 0.130f + 50.0f, fy * 0.150f, fz * 0.130f);
    return n * 0.6f + m * 0.4f > 0.63f;
}

void World::GenerateChunk(int cx, int cz) {
    if (m_dimension == Dimension::Underworld) {
        GenerateUnderworldColumn(cx, cz);
    } else {
        GenerateUpperworldColumn(cx, cz);
    }
}

void World::GenerateUpperworldColumn(int cx, int cz) {
    const int cyCount = UPPERWORLD_TOP / CHUNK_SIZE; // 6
    Chunk* columns[cyCount];
    for (int cy = 0; cy < cyCount; ++cy) {
        columns[cy] = &GetOrCreateChunk(cx, cy, cz);
    }

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            const int wx = cx * CHUNK_SIZE + x;
            const int wz = cz * CHUNK_SIZE + z;
            const int height = static_cast<int>(ColumnHeight(wx, wz));
            const bool beach = height <= SEA_LEVEL + 1;

            for (int y = 0; y < UPPERWORLD_TOP; ++y) {
                BlockType type = BlockType::Air;

                if (y == 0 || (y == 1 && (Hash2(wx, wz) & 3u) == 0u)) {
                    type = BlockType::Bedrock;
                } else if (y <= height) {
                    const int depth = height - y;
                    if (depth == 0) {
                        type = beach ? BlockType::Sand : BlockType::Grass;
                    } else if (depth <= 3) {
                        type = beach ? BlockType::Sand : BlockType::Dirt;
                    } else {
                        type = BlockType::Stone;
                        const uint32_t ore = Hash3(wx, y, wz) % 1000u;
                        if (ore < 9u) {
                            type = BlockType::Emberlode;
                        } else if (ore < 16u) {
                            type = BlockType::Ironlode;
                        }
                    }

                    if (y > 2 && y < height - 1 && CaveAt(wx, y, wz)) {
                        type = BlockType::Air;
                    }
                } else if (y <= SEA_LEVEL) {
                    type = BlockType::Water;
                }

                columns[y / CHUNK_SIZE]->SetBlock(x, y % CHUNK_SIZE, z, type);
            }
        }
    }
}

void World::GenerateUnderworldColumn(int cx, int cz) {
    const int cyCount = UNDERWORLD_TOP / CHUNK_SIZE; // 8
    Chunk* columns[cyCount];
    for (int cy = 0; cy < cyCount; ++cy) {
        columns[cy] = &GetOrCreateChunk(cx, cy, cz);
    }

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            const int wx = cx * CHUNK_SIZE + x;
            const int wz = cz * CHUNK_SIZE + z;
            const int height = static_cast<int>(ColumnHeight(wx, wz));
            const int lavaTop = 26 + static_cast<int>(
                ValueNoise2(static_cast<float>(wx) * 0.05f + 40.0f,
                            static_cast<float>(wz) * 0.05f - 11.0f) * 10.0f);
            const bool pillar = (Hash2(wx, wz) % 173u) == 0u;

            for (int y = 0; y < UNDERWORLD_TOP; ++y) {
                BlockType type = BlockType::Air;

                if (y <= 1 || y >= UNDERWORLD_CEILING) {
                    type = BlockType::Bedrock;
                } else if (y <= lavaTop) {
                    type = BlockType::Magma;
                } else {
                    bool solid = (y <= height) && !CaveAt(wx, y, wz);

                    // Obsidian-like pillars reaching up out of the magma sea.
                    if (pillar && y <= height + 14) {
                        solid = true;
                    }

                    // Hanging formations below the roof.
                    if (!solid && y > UNDERWORLD_CEILING - 16 && y < UNDERWORLD_CEILING) {
                        const float hang = ValueNoise3(static_cast<float>(wx) * 0.120f,
                                                       static_cast<float>(y) * 0.160f,
                                                       static_cast<float>(wz) * 0.120f);
                        solid = hang > 0.62f;
                    }

                    if (solid) {
                        type = BlockType::Emberstone;
                        if (y < lavaTop + 3) {
                            type = BlockType::Cinderstone;
                        }
                        if (y > height - 3) {
                            type = BlockType::Ashsoil;
                        }
                        const uint32_t vein = Hash3(wx, y, wz) % 1000u;
                        if (vein < 6u) {
                            type = BlockType::Lumenstone;
                        } else if (vein < 10u) {
                            type = BlockType::PaleQuartz;
                        } else if (vein < 14u) {
                            type = BlockType::Glowmoss;
                        }
                    }
                }

                columns[y / CHUNK_SIZE]->SetBlock(x, y % CHUNK_SIZE, z, type);
            }
        }
    }
}

void World::DecorateUpperworldColumn(int cx, int cz) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            const int wx = cx * CHUNK_SIZE + x;
            const int wz = cz * CHUNK_SIZE + z;
            if ((Hash2(wx, wz) % 149u) != 0u) {
                continue;
            }

            const int height = static_cast<int>(ColumnHeight(wx, wz));
            if (height <= SEA_LEVEL + 2) {
                continue;
            }
            if (GetBlock(wx, height, wz) != BlockType::Grass) {
                continue;
            }
            if (GetBlock(wx, height + 2, wz) != BlockType::Air) {
                continue;
            }

            const int trunk = 4 + static_cast<int>(Hash2(wx + 7, wz - 3) % 3u);
            for (int i = 1; i <= trunk; ++i) {
                SetBlock(wx, height + i, wz, BlockType::Wood);
            }

            const int top = height + trunk;
            for (int dy = -2; dy <= 1; ++dy) {
                const int radius = (dy <= -1) ? 2 : 1;
                for (int dz = -radius; dz <= radius; ++dz) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (dx == 0 && dz == 0 && dy <= 0) {
                            continue; // keep the trunk
                        }
                        if (dx * dx + dz * dz > radius * radius + 1) {
                            continue;
                        }
                        if (GetBlock(wx + dx, top + dy, wz + dz) != BlockType::Air) {
                            continue;
                        }
                        SetBlock(wx + dx, top + dy, wz + dz, BlockType::Leaves);
                    }
                }
            }
        }
    }
}

void World::DecorateChunk(int cx, int cz) {
    if (m_dimension == Dimension::Upperworld) {
        DecorateUpperworldColumn(cx, cz);
    }
}

void World::GenerateChunks(int radiusInChunks) {
    GenerateChunksAround(0, 0, radiusInChunks);
}

void World::GenerateChunksAround(int centerCX, int centerCZ, int radiusInChunks) {
    if (radiusInChunks < 0) radiusInChunks = 0;
    for (int cz = -radiusInChunks; cz <= radiusInChunks; ++cz) {
        for (int cx = -radiusInChunks; cx <= radiusInChunks; ++cx) {
            GenerateChunk(centerCX + cx, centerCZ + cz);
        }
    }
    // Decorate only after every column exists so trees are not overwritten.
    for (int cz = -radiusInChunks; cz <= radiusInChunks; ++cz) {
        for (int cx = -radiusInChunks; cx <= radiusInChunks; ++cx) {
            DecorateChunk(centerCX + cx, centerCZ + cz);
        }
    }
}

size_t World::GetChunkCount() const {
    return m_chunks.size();
}

int World::SurfaceY(int x, int z) const {
    // Stay below the hanging ceiling formations so spawns land on real ground.
    const int start = (m_dimension == Dimension::Underworld)
                          ? (UNDERWORLD_CEILING - 20)
                          : (UPPERWORLD_TOP - 2);

    for (int y = start; y >= 0; --y) {
        if (IsCollidable(GetBlock(x, y, z)) &&
            !IsCollidable(GetBlock(x, y + 1, z)) &&
            !IsCollidable(GetBlock(x, y + 2, z))) {
            return y + 1;
        }
    }
    return 2;
}
