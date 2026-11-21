#pragma once

#include "core/world/chunk.h"
#include <array>
#include <vector>
#include <string>
#include <cstdint>

// Recipe output structure
struct RecipeOutput {
    BlockType type;
    int count; // stack size
};

// Crafting recipe definition
struct Recipe {
    std::array<std::array<BlockType, 3>, 3> grid; // 3x3 grid (crafting table) or 2x2 (player)
    bool isTableRecipe; // true = 3x3 table, false = 2x2 player inventory
    RecipeOutput output;
    std::string name;
};

// Crafting system: 2x2 (player) and 3x3 (crafting table)
class CraftingSystem {
public:
    CraftingSystem();

    // Player 2x2 crafting grid
    std::array<std::array<BlockType, 2>, 4> playerGrid; // 2x2 grid, indexed [i]

    // Crafting table 3x3 grid
    std::array<std::array<BlockType, 3>, 9> tableGrid; // 3x3 grid, indexed [i]

    // Active recipe output
    RecipeOutput lastOutput;

    // Known recipes (hardcoded)
    std::vector<Recipe> recipes;

    // Constructor - register default recipes
    void InitializeRecipes();

    // Try to craft using player 2x2 grid
    bool CraftPlayer();

    // Try to craft using crafting table 3x3 grid
    bool CraftTable();

    // Get output from current grid configuration
    RecipeOutput GetRecipeOutput(const std::array<std::array<BlockType, 3>, 9>& grid,
                                 bool isTable) const;

    // Get recipe by output type
    const Recipe* FindRecipe(BlockType outputType) const;

    // Add item to player inventory
    bool AddToInventory(BlockType type, int count);

    // Remove item from inventory
    bool RemoveFromInventory(BlockType type, int count);

    // Inventory: 36 slots (36 = 32 + 4 extra)
    InventorySlot inventory[36];

    // Hotbar: first 9 slots are the hotbar
    InventorySlot& GetHotbarSlot(int index) { return inventory[index]; }

    // Select hotbar slot
    void SelectHotbarSlot(int index);
};