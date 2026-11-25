#pragma once

#include "core/world/chunk.h"

#include <array>
#include <cstdint>

// 2x2 crafting grid (player inventory)
struct CraftingGrid2x2 {
    std::array<std::array<BlockType, 2>, 2> slots{};
};

// 3x3 crafting grid (crafting table)
struct CraftingGrid3x3 {
    std::array<std::array<BlockType, 3>, 3> slots{};
};

struct RecipeResult {
    BlockType resultType = BlockType::Air;
    uint8_t resultCount = 0;
    int8_t resultHotbarSlot = -1;
};

// 2x2 recipes (player inventory)
constexpr std::array<RecipeResult, 4> kPlayerRecipes = {{
    {BlockType::Planks, 4, 0},
    {BlockType::Sticks, 4, 1},
    {BlockType::CraftingTable, 1, 2},
    {BlockType::Brick, 1, 3},
}};

// 3x3 recipes (crafting table) - none yet.
constexpr std::array<RecipeResult, 0> kTableRecipes = {};

inline RecipeResult LookupPlayerRecipe(const CraftingGrid2x2& grid) {
    for (const auto& recipe : kPlayerRecipes) {
        bool match = true;
        for (int i = 0; i < 2 && match; ++i) {
            for (int j = 0; j < 2; ++j) {
                if (recipe.resultType != grid.slots[i][j]) {
                    match = false;
                    break;
                }
            }
        }
        if (match) {
            return recipe;
        }
    }
    return {};
}

inline RecipeResult LookupTableRecipe(const CraftingGrid3x3& grid) {
    (void)grid;
    return {};
}
