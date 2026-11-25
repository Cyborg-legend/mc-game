#include "crafting/recipes.h"

// Crafting system implementation.

CraftingGrid2x2 GetPlayerGrid(const int8_t* inventory, int8_t selectedSlot) {
    (void)selectedSlot;
    CraftingGrid2x2 grid{};
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            const int slotIdx = 32 + (i * 2 + j);
            grid.slots[i][j] = static_cast<BlockType>(static_cast<uint8_t>(inventory[slotIdx]));
        }
    }
    return grid;
}

void CraftPlayer(int8_t* inventory, int8_t& selectedSlot, CraftingGrid2x2& grid) {
    const RecipeResult result = LookupPlayerRecipe(grid);
    if (result.resultType == BlockType::Air) {
        return;
    }

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            const int slotIdx = 32 + (i * 2 + j);
            inventory[slotIdx] = static_cast<int8_t>(BlockType::Air);
        }
    }

    for (int i = 0; i < 36; ++i) {
        if (inventory[i] == static_cast<int8_t>(BlockType::Air)) {
            inventory[i] = static_cast<int8_t>(result.resultType);
            break;
        }
    }

    if (result.resultHotbarSlot >= 0 && result.resultHotbarSlot < 9) {
        selectedSlot = result.resultHotbarSlot;
    }
}

void CraftTable(int8_t* inventory) {
    CraftingGrid3x3 grid{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const int slotIdx = i * 3 + j;
            grid.slots[i][j] = static_cast<BlockType>(static_cast<uint8_t>(inventory[slotIdx]));
        }
    }

    const RecipeResult result = LookupTableRecipe(grid);
    if (result.resultType == BlockType::Air) {
        return;
    }

    for (int i = 0; i < 36; ++i) {
        if (inventory[i] == static_cast<int8_t>(BlockType::Air)) {
            inventory[i] = static_cast<int8_t>(result.resultType);
            break;
        }
    }
}
