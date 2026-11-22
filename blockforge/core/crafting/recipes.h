#pragma once

#include <array>
#include <cstdint>

// Block types for crafting
enum class BlockType : uint8_t {
    Air = 0,
    Stone,
    Dirt,
    Grass,
    Wood,
    Planks,
    Sticks,
    CraftingTable,
    Brick,
    Count
};

// 2x2 crafting grid (player inventory)
struct CraftingGrid2x2 {
    std::array<std::array<BlockType, 2>, 2> slots; // 2x2 grid
};

// 3x3 crafting grid (crafting table)
struct CraftingGrid3x3 {
    std::array<std::array<BlockType, 3>, 3> slots; // 3x3 grid
};

// Recipe output
struct RecipeResult {
    BlockType resultType;      // Type of block produced
    uint8_t resultCount;       // Number of items produced (1-64)
    int8_t resultHotbarSlot;   // Hotbar slot to place result (-1 = none)
};

// Hardcoded crafting recipes
// 2x2 recipes (player inventory)
constexpr std::array<RecipeResult, 4> kPlayerRecipes = {{
    // Wood -> Planks: 1 wood -> 4 planks
    {BlockType::Planks, 4, 0},
    // Planks -> Sticks: 2 planks -> 4 sticks
    {BlockType::Sticks, 4, 1},
    // Planks + Sticks -> Crafting Table: fill 2x2 with planks+sticks
    {BlockType::CraftingTable, 1, 2},
    // Dirt -> Brick: 4 dirt -> 1 brick
    {BlockType::Brick, 1, 3},
}};

// 3x3 recipes (crafting table - currently empty, placeholder)
constexpr std::array<RecipeResult, 0> kTableRecipes = {};

// Look up 2x2 recipe by grid contents
// Returns result if match found, otherwise resultType = Air
inline RecipeResult LookupPlayerRecipe(const CraftingGrid2x2& grid) {
    for (const auto& recipe : kPlayerRecipes) {
        // Check if recipe matches the grid
        bool match = true;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                // Recipe expects specific block type at this position
                // Air in recipe means "any block"
                if (recipe.resultType != BlockType::Air && grid.slots[i][j] != recipe.resultType) {
                    match = false;
                    break;
                }
            }
            if (!match) break;
        }
        if (match) return recipe;
    }
    // No recipe matched
    return {BlockType::Air, 0, -1};
}

// Look up 3x3 recipe by grid contents
inline RecipeResult LookupTableRecipe(const CraftingGrid3x3& grid) {
    (void)grid; // Placeholder - no recipes yet
    return {BlockType::Air, 0, -1};
}