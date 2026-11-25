#include "meshing/mesh.h"

const std::array<std::array<float, 3>, 6> faceNormals = {{
    {{ 1.0f,  0.0f,  0.0f}},
    {{-1.0f,  0.0f,  0.0f}},
    {{ 0.0f,  1.0f,  0.0f}},
    {{ 0.0f, -1.0f,  0.0f}},
    {{ 0.0f,  0.0f,  1.0f}},
    {{ 0.0f,  0.0f, -1.0f}}
}};

// UV corners for the four quad vertices, in winding order.
const std::array<std::array<float, 4>, 6> faceUvCoords = {{
    {{0.0f, 0.0f, 1.0f, 1.0f}},
    {{0.0f, 0.0f, 1.0f, 1.0f}},
    {{0.0f, 0.0f, 1.0f, 1.0f}},
    {{0.0f, 0.0f, 1.0f, 1.0f}},
    {{0.0f, 0.0f, 1.0f, 1.0f}},
    {{0.0f, 0.0f, 1.0f, 1.0f}}
}};

namespace {

const AtlasTileDef kAtlasTiles[] = {
    {"Void",         0,   0,   0,   0,   0,   0,   TilePattern::Unused},
    {"Stone",        128, 128, 132, 90,  90,  94,  TilePattern::Speckle},
    {"Dirt",         122, 88,  60,  96,  68,  45,  TilePattern::Speckle},
    {"Grass Top",    96,  158, 62,  74,  124, 48,  TilePattern::GrassTop},
    {"Grass Side",   122, 88,  60,  96,  158, 62,  TilePattern::GrassSide},
    {"Bark",         96,  74,  48,  70,  54,  35,  TilePattern::Bark},
    {"Wood Rings",   150, 118, 78,  120, 92,  60,  TilePattern::Rings},
    {"Leaves",       58,  118, 48,  40,  86,  34,  TilePattern::Foliage},
    {"Sand",         222, 210, 164, 200, 188, 145, TilePattern::Speckle},
    {"Water",        54,  98,  190, 90,  140, 220, TilePattern::Liquid},
    {"Bedrock",      52,  52,  56,  30,  30,  34,  TilePattern::Chunky},
    {"Planks",       166, 132, 84,  132, 102, 62,  TilePattern::Planks},
    {"Sticks",       140, 110, 72,  110, 84,  54,  TilePattern::Bark},
    {"Craft Top",    150, 112, 64,  110, 80,  45,  TilePattern::Grid},
    {"Craft Side",   136, 100, 58,  104, 74,  42,  TilePattern::Planks},
    {"Brick",        158, 72,  58,  190, 180, 170, TilePattern::Brick},

    {"Emberstone",   110, 44,  40,  78,  28,  26,  TilePattern::Speckle},
    {"Ashsoil",      74,  62,  66,  54,  44,  48,  TilePattern::Ash},
    {"Lumenstone",   232, 206, 110, 255, 240, 180, TilePattern::Glow},
    {"Magma",        196, 82,  28,  255, 150, 40,  TilePattern::Liquid},
    {"Voidstone",    46,  34,  58,  90,  66,  120, TilePattern::Crystal},
    {"Cinderbrick",  92,  60,  54,  140, 120, 110, TilePattern::Brick},
    {"Pale Quartz",  226, 224, 214, 200, 198, 190, TilePattern::Speckle},
    {"Crimson Bark", 104, 44,  44,  78,  32,  32,  TilePattern::Bark},
    {"Crimson Rings",150, 74,  66,  120, 55,  50,  TilePattern::Rings},
    {"Bloodleaves",  126, 44,  44,  92,  30,  30,  TilePattern::Foliage},
    {"Emberlode",    128, 128, 132, 230, 140, 50,  TilePattern::Ore},
    {"Ironlode",     128, 128, 132, 210, 190, 160, TilePattern::Ore},
    {"Rift Portal",  86,  44,  140, 200, 120, 255, TilePattern::Swirl},
    {"Glowmoss",     70,  150, 120, 170, 255, 210, TilePattern::Glow},
    {"Cinderstone",  74,  70,  72,  100, 96,  96,  TilePattern::Chunky},
    {"Ashblock",     176, 170, 160, 150, 144, 136, TilePattern::Ash},
};

enum TileIndex : int {
    T_Stone = 1, T_Dirt, T_GrassTop, T_GrassSide, T_WoodSide, T_WoodTop, T_Leaves,
    T_Sand, T_Water, T_Bedrock, T_Planks, T_Sticks, T_CraftTop, T_CraftSide, T_Brick,
    T_Emberstone, T_Ashsoil, T_Lumenstone, T_Magma, T_Voidstone, T_Cinderbrick,
    T_PaleQuartz, T_CrimsonSide, T_CrimsonTop, T_Bloodleaves, T_Emberlode,
    T_Ironlode, T_Portal, T_Glowmoss, T_Cinderstone, T_Ashblock
};

} // namespace

int AtlasTileCount() {
    return static_cast<int>(sizeof(kAtlasTiles) / sizeof(kAtlasTiles[0]));
}

const AtlasTileDef& GetAtlasTile(int index) {
    static const AtlasTileDef kFallback{"Void", 0, 0, 0, 0, 0, 0, TilePattern::Unused};
    if (index < 0 || index >= AtlasTileCount()) {
        return kFallback;
    }
    return kAtlasTiles[index];
}

int TileForFace(BlockType type, int faceDir) {
    const bool top = (faceDir == 2);
    const bool bottom = (faceDir == 3);

    switch (type) {
        case BlockType::Air:          return 0;
        case BlockType::Stone:        return T_Stone;
        case BlockType::Dirt:         return T_Dirt;
        case BlockType::Grass:        return top ? T_GrassTop : (bottom ? T_Dirt : T_GrassSide);
        case BlockType::Wood:         return (top || bottom) ? T_WoodTop : T_WoodSide;
        case BlockType::Leaves:       return T_Leaves;
        case BlockType::Sand:         return T_Sand;
        case BlockType::Water:        return T_Water;
        case BlockType::Bedrock:      return T_Bedrock;
        case BlockType::Planks:       return T_Planks;
        case BlockType::Sticks:       return T_Sticks;
        case BlockType::CraftingTable:return top ? T_CraftTop : T_CraftSide;
        case BlockType::Brick:        return T_Brick;
        case BlockType::Emberstone:   return T_Emberstone;
        case BlockType::Ashsoil:      return T_Ashsoil;
        case BlockType::Lumenstone:   return T_Lumenstone;
        case BlockType::Magma:        return T_Magma;
        case BlockType::Voidstone:    return T_Voidstone;
        case BlockType::Cinderbrick:  return T_Cinderbrick;
        case BlockType::PaleQuartz:   return T_PaleQuartz;
        case BlockType::CrimsonWood:  return (top || bottom) ? T_CrimsonTop : T_CrimsonSide;
        case BlockType::Bloodleaves:  return T_Bloodleaves;
        case BlockType::Emberlode:    return T_Emberlode;
        case BlockType::Ironlode:     return T_Ironlode;
        case BlockType::Portal:       return T_Portal;
        case BlockType::Glowmoss:     return T_Glowmoss;
        case BlockType::Cinderstone:  return T_Cinderstone;
        case BlockType::Ashblock:     return T_Ashblock;
        case BlockType::Count:        break;
    }
    return T_Stone;
}

void BlockColor(BlockType type, float outRgb[3]) {
    const int tile = TileForFace(type, 2);
    const AtlasTileDef& def = GetAtlasTile(tile);
    if (def.pattern == TilePattern::Unused) {
        outRgb[0] = 0.10f;
        outRgb[1] = 0.10f;
        outRgb[2] = 0.12f;
        return;
    }
    outRgb[0] = static_cast<float>(def.r) / 255.0f;
    outRgb[1] = static_cast<float>(def.g) / 255.0f;
    outRgb[2] = static_cast<float>(def.b) / 255.0f;
}
