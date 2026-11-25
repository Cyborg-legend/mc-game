#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/world/chunk.h"

// Vertex format shared by the mesher and the OpenGL renderer.
struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float ao;
};

struct MeshChunk {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Face order used everywhere: +X, -X, +Y, -Y, +Z, -Z
extern const std::array<std::array<float, 3>, 6> faceNormals;
extern const std::array<std::array<float, 4>, 6> faceUvCoords;

// ---- Procedural texture atlas -------------------------------------------
// A tile is 16x16 pixels; the atlas is a 16x16 grid of tiles (256x256 px).
// Every texture is painted at runtime from a solid colour plus a pattern, so
// the game contains no third-party image assets.
enum class TilePattern : int {
    Unused = -1,
    Flat = 0,
    Speckle = 1,
    GrassTop = 2,
    GrassSide = 3,
    Bark = 4,
    Rings = 5,
    Foliage = 6,
    Liquid = 7,
    Chunky = 8,
    Planks = 9,
    Grid = 10,
    Brick = 11,
    Ore = 12,
    Glow = 13,
    Swirl = 14,
    Crystal = 15,
    Ash = 16
};

struct AtlasTileDef {
    const char* name;
    unsigned char r, g, b;      // base colour
    unsigned char ar, ag, ab;   // accent colour
    TilePattern pattern;
};

int AtlasTileCount();
const AtlasTileDef& GetAtlasTile(int index);

// Atlas tile index for one block face. faceDir follows the face order above.
int TileForFace(BlockType type, int faceDir);

// Representative colour (0..1) of a block, used by the HUD hotbar.
void BlockColor(BlockType type, float outRgb[3]);
