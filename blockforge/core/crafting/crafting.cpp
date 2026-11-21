#include "crafting/recipes.h"
#include "core/world/world.h"
#include <algorithm>

// ============================================================
// CraftingSystem implementation
// ============================================================

CraftingSystem::CraftingSystem() {
    // Initialize grids to Air
    for (int i = 0; i < 4; ++i) {
        playerGrid[i / 2][i % 2] = BlockType::Air;
    }
    for (int i = 0; i < 9; ++i) {
        tableGrid[i / 3][i % 3] = BlockType::Air;
    }

    // Initialize recipes
    InitializeRecipes();
}

void CraftingSystem::InitializeRecipes() {
    // Hardcoded recipes for BlockForge

    // Wood planks from wood (any wood type)
    Recipe r1;
    r1.isTableRecipe = false;
    r1.grid = {{
        {BlockType::Wood, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air}
    }};
    r1.output = {BlockType::Planks, 4}; // 4 planks from 1 wood
    r1.name = "wood_to_planks";
    recipes.push_back(r1);

    // Planks to sticks
    Recipe r2;
    r2.isTableRecipe = false;
    r2.grid = {{
        {BlockType::Planks, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air}
    }};
    r2.output = {BlockType::Sticks, 4};
    r2.name = "planks_to_sticks";
    recipes.push_back(r2);

    // Crafting table from planks + sticks (3x3 recipe)
    Recipe r3;
    r3.isTableRecipe = true;
    r3.grid = {{
        {BlockType::Planks, BlockType::Planks, BlockType::Air},
        {BlockType::Planks, BlockType::Sticks, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air}
    }};
    r3.output = {BlockType::CraftingTable, 1};
    r3.name = "crafting_table";
    recipes.push_back(r3);

    // Dirt to brick (smelting-like, simplified)
    Recipe r4;
    r4.isTableRecipe = false;
    r4.grid = {{
        {BlockType::Dirt, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air}
    }};
    r4.output = {BlockType::Brick, 1};
    r4.name = "dirt_to_brick";
    recipes.push_back(r4);

    // Stone to something basic
    Recipe r5;
    r5.isTableRecipe = false;
    r5.grid = {{
        {BlockType::Stone, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air},
        {BlockType::Air, BlockType::Air, BlockType::Air}
    }};
    r5.output = {BlockType::Stone, 1}; // Just return stone (no recipe yet)
    r5.name = "no_recipe";
    recipes.push_back(r5);
}

RecipeOutput CraftingSystem::GetRecipeOutput(const std::array<std::array<BlockType, 3>, 9>& grid,
                                              bool isTable) const {
    (void)isTable;
    // Search through recipes for a match
    for (const auto& recipe : recipes) {
        if (recipe.isTableRecipe != isTable) continue;

        // Check if grid matches recipe
        bool match = true;
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                BlockType expected = recipe.grid[y][x];
                BlockType actual = isTable ? grid[y][x] : BlockType::Air;
                // Player grid is 2x2, so for player recipes we check differently
                if (expected != BlockType::Air && actual != expected) {
                    match = false;
                    break;
                }
            }
            if (!match) break;
        }

        if (match) {
            return recipe.output;
        }
    }

    // Default: no recipe found
    return {BlockType::Air, 0};
}

bool CraftingSystem::CraftPlayer() {
    // Try to craft using player 2x2 grid
    RecipeOutput output = GetRecipeOutput(
        [this](int y, int x) -> BlockType { return playerGrid[y][x]; }, // simplified
        false);

    if (output.type != BlockType::Air) {
        // Consume ingredients
        for (int i = 0; i < 4; ++i) {
            if (playerGrid[i / 2][i % 2] != BlockType::Air) {
                // Remove one of each ingredient
                // This is simplified - full implementation would track counts
                playerGrid[i / 2][i % 2] = BlockType::Air;
            }
        }

        // Add output to inventory
        lastOutput = output;
        return AddToInventory(output.type, output.count);
    }

    lastOutput = {BlockType::Air, 0};
    return false;
}

bool CraftingSystem::CraftTable() {
    // Try to craft using crafting table 3x3 grid
    RecipeOutput output = GetRecipeOutput(tableGrid, true);

    if (output.type != BlockType::Air) {
        // Consume ingredients from table grid
        for (int i = 0; i < 9; ++i) {
            if (tableGrid[i / 3][i % 3] != BlockType::Air) {
                tableGrid[i / 3][i % 3] = BlockType::Air;
            }
        }

        lastOutput = output;
        return AddToInventory(output.type, output.count);
    }

    lastOutput = {BlockType::Air, 0};
    return false;
}

bool CraftingSystem::AddToInventory(BlockType type, int count) {
    // Find slot - first try empty slot
    for (int i = 0; i < 36; ++i) {
        if (inventory[i].type == BlockType::Air) {
            inventory[i].type = type;
            inventory[i].count = count;
            return true;
        }
    }

    // Try stackable: find slot with same type that isn't full
    for (int i = 0; i < 36; ++i) {
        if (inventory[i].type == type && inventory[i].count < 64) {
            inventory[i].count += count;
            if (inventory[i].count > 64) {
                count -= (inventory[i].count - 64);
                inventory[i].count = 64;
            } else {
                count = 0;
            }
            if (count <= 0) return true;
        }
    }

    // No space - cannot add
    return false;
}

bool CraftingSystem::RemoveFromInventory(BlockType type, int count) {
    // Find and remove from inventory
    for (int i = 0; i < 36; ++i) {
        if (inventory[i].type == type && inventory[i].count >= count) {
            inventory[i].count -= count;
            if (inventory[i].count == 0) {
                inventory[i].type = BlockType::Air;
            }
            return true;
        }
    }
    return false;
}

void CraftingSystem::SelectHotbarSlot(int index) {
    // Validate index
    if (index >= 0 && index < 9) {
        // Hotbar slot selected - no special action needed beyond tracking
        // The index is used for rendering and placing
    }
}