#include "crafting/recipes.h"
#include <algorithm>

// Crafting system implementation
// Inventory: 36 slots total, 9 hotbar slots

CraftingGrid2x2 GetPlayerGrid(const int8_t* inventory, int8_t selectedSlot) {
    CraftingGrid2x2 grid{};

    // Fill 2x2 grid from inventory slots 32-35 (top row) and hotbar
    // Grid layout:
    // [32][33]
    // [34][35]
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            int slotIdx = 32 + (i * 2 + j);
            if (slotIdx >= 0 && slotIdx < 36) {
                // Convert block type index to BlockType enum
                // For now, just use the raw value
                grid.slots[i][j] = static_cast<BlockType>(inventory[slotIdx]);
            }
        }
    }

    return grid;
}

void CraftPlayer(int8_t* inventory, int8_t& selectedSlot, CraftingGrid2x2& grid) {
    // Look up recipe
    RecipeResult result = LookupPlayerRecipe(grid);

    if (result.resultType != BlockType::Air) {
        // Consume input items from grid
        // Grid slots 32-35 are the 2x2 crafting grid
        // We need to find and remove the used ingredients
        // For simplicity, just shift remaining items
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                int slotIdx = 32 + (i * 2 + j);
                if (slotIdx >= 0 && slotIdx < 36) {
                    // Mark slot as empty (Air)
                    inventory[slotIdx] = static_cast<int8_t>(BlockType::Air);
                }
            }
        }

        // Add result to inventory
        // Find first empty slot
        for (int i = 0; i < 36; ++i) {
            if (inventory[i] == static_cast<int8_t>(BlockType::Air)) {
                inventory[i] = result.resultType;
                break;
            }
        }

        // Update hotbar if resultHotbarSlot is set
        if (result.resultHotbarSlot >= 0 && result.resultHotbarSlot < 9) {
            selectedSlot = result.resultHotbarSlot;
        }
    }
}

// Activate crafting table (3x3 grid from all 36 inventory slots)
void CraftTable(int8_t* inventory) {
    // Build 3x3 grid from first 9 inventory slots (or all 36)
    CraftingGrid3x3 grid{};

    for (int i = 0; i < 3 && i < 36; ++i) {
        for (int j = 0; j < 3; ++j) {
            int slotIdx = i * 3 + j;
            if (slotIdx < 36) {
                grid.slots[i][j] = static_cast<BlockType>(inventory[slotIdx]);
            }
        }
    }

    // Look up recipe
    RecipeResult result = LookupTableRecipe(grid);

    if (result.resultType != BlockType::Air) {
        // Consume inputs and produce result
        // For now, just place result in first empty slot
        for (int i = 0; i < 36; ++i) {
            if (inventory[i] == static_cast<int8_t>(BlockType::Air)) {
                inventory[i] = result.resultType;
                break;
            }
        }
    }
}