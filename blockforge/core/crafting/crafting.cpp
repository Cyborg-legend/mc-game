#include "core/world/chunk.h"
#include <algorithm>

struct Recipe {
    BlockType grid[9];
    bool isTable;
    BlockType output;
    int outputCount;
};

static Recipe recipes[] = {
    {{BlockType::Wood, BlockType::Air, BlockType::Air,
      BlockType::Air, BlockType::Air, BlockType::Air,
      BlockType::Air, BlockType::Air, BlockType::Air}, false, BlockType::Planks, 4},
    {{BlockType::Planks, BlockType::Air, BlockType::Air,
      BlockType::Air, BlockType::Air, BlockType::Air,
      BlockType::Air, BlockType::Air, BlockType::Air}, false, BlockType::Sticks, 4},
    {{BlockType::Planks, BlockType::Planks, BlockType::Planks,
      BlockType::Planks, BlockType::Planks, BlockType::Planks,
      BlockType::Air, BlockType::Air, BlockType::Air}, true, BlockType::CraftingTable, 1},
    {{BlockType::Dirt, BlockType::Air, BlockType::Air,
      BlockType::Air, BlockType::Air, BlockType::Air,
      BlockType::Air, BlockType::Air, BlockType::Air}, false, BlockType::Brick, 1},
};

static int recipeCount = 4;

bool TryCraft(BlockType grid[], int size, bool isTable, BlockType& outType, int& outCount) {
    for (int r = 0; r < recipeCount; ++r) {
        Recipe& rec = recipes[r];
        if (rec.isTable != isTable) continue;
        bool match = true;
        for (int i = 0; i < 9; ++i) {
            BlockType cell = (i < size * (isTable ? 3 : 2)) ? grid[i] : BlockType::Air;
            if (cell != rec.grid[i]) { match = false; break; }
        }
        if (match) {
            outType = rec.output;
            outCount = rec.outputCount;
            return true;
        }
    }
    return false;
}   