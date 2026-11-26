#include "bg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------- names */

static const char *const kBlockNames[BG_BLOCK_COUNT] = {
    "Air", "Stone", "Dirt", "Grass", "Wood", "Leaves", "Sand", "Water",
    "Bedrock", "Planks", "Sticks", "Crafting Table", "Brick", "Emberstone",
    "Ashsoil", "Lumenstone", "Magma", "Voidstone", "Cinderbrick", "Pale Quartz",
    "Crimson Wood", "Bloodleaves", "Emberlode", "Ironlode", "Rift Portal",
    "Glowmoss", "Cinderstone", "Ashblock"
};

const char *item_name(uint16_t item) {
    if (item < BG_BLOCK_COUNT) {
        return kBlockNames[item];
    }
    switch (item) {
        case BG_ITEM_STICK:       return "Stick";
        case BG_ITEM_EMBER_SHARD: return "Ember Shard";
        case BG_ITEM_ASH_DUST:    return "Ash Dust";
        case BG_ITEM_MEAT:        return "Meat";
        case BG_ITEM_WOOD_PICK:   return "Wooden Pickaxe";
        case BG_ITEM_STONE_PICK:  return "Stone Pickaxe";
        case BG_ITEM_EMBER_PICK:  return "Ember Pickaxe";
        default:                  return "Unknown";
    }
}

bool item_block(uint16_t item, BlockId *block) {
    if (item == BG_AIR || item >= BG_BLOCK_COUNT) {
        return false;
    }
    if (block) {
        *block = (BlockId)item;
    }
    return true;
}

bool block_drop(BlockId block, uint16_t *item, int *count) {
    uint16_t drop = block;
    int amount = 1;

    switch (block) {
        case BG_AIR:
        case BG_BEDROCK:
        case BG_WATER:
            return false;
        case BG_GRASS:
            drop = BG_DIRT;
            break;
        case BG_LEAVES:
        case BG_BLOODLEAVES:
            /* Foliage is mostly air; it yields a stick when cleared. */
            drop = BG_ITEM_STICK;
            amount = 1;
            break;
        case BG_EMBERLODE:
            drop = BG_ITEM_EMBER_SHARD;
            amount = 2;
            break;
        case BG_IRONLODE:
            drop = BG_ITEM_ASH_DUST;
            amount = 2;
            break;
        case BG_ASHSOIL:
        case BG_ASHBLOCK:
            drop = BG_ITEM_ASH_DUST;
            break;
        default:
            break;
    }

    if (item) {
        *item = drop;
    }
    if (count) {
        *count = amount;
    }
    return true;
}

/* --------------------------------------------------------------- tools */

int item_pick_tier(uint16_t item) {
    switch (item) {
        case BG_ITEM_WOOD_PICK:  return 1;
        case BG_ITEM_STONE_PICK: return 2;
        case BG_ITEM_EMBER_PICK: return 3;
        default:                 return 0;
    }
}

/* Stone and anything fired or mortared needs a pick; the deep ores and the
 * rift's own voidstone need a stone pick or better. */
int block_pick_tier(BlockId block) {
    switch (block) {
        case BG_STONE:
        case BG_BRICK:
        case BG_EMBERSTONE:
        case BG_CINDERBRICK:
        case BG_CINDERSTONE:
        case BG_PALE_QUARTZ:
        case BG_LUMENSTONE:
        case BG_MAGMA:
            return 1;
        case BG_IRONLODE:
        case BG_EMBERLODE:
        case BG_VOIDSTONE:
            return 2;
        default:
            return 0;
    }
}

float block_hardness(BlockId block) {
    switch (block) {
        case BG_AIR:
        case BG_WATER:
        case BG_BEDROCK:
            return 0.0f;
        case BG_LEAVES:
        case BG_BLOODLEAVES:
        case BG_PORTAL:
            return 0.25f;
        case BG_GLOWMOSS:
            return 0.40f;
        case BG_DIRT:
        case BG_GRASS:
        case BG_SAND:
        case BG_ASHSOIL:
        case BG_ASHBLOCK:
            return 0.45f;
        case BG_PLANKS:
        case BG_STICKS:
        case BG_CRAFTING_TABLE:
            return 1.00f;
        case BG_BRICK:
            return 1.10f;
        case BG_PALE_QUARTZ:
            return 1.20f;
        case BG_LUMENSTONE:
            return 1.30f;
        case BG_CINDERBRICK:
        case BG_CINDERSTONE:
            return 1.40f;
        case BG_STONE:
            return 1.50f;
        case BG_MAGMA:
            return 1.60f;
        case BG_WOOD:
        case BG_CRIMSON_WOOD:
            return 1.70f;
        case BG_EMBERSTONE:
            return 1.80f;
        case BG_IRONLODE:
            return 2.20f;
        case BG_EMBERLODE:
            return 2.50f;
        case BG_VOIDSTONE:
            return 2.80f;
        default:
            return 1.00f;
    }
}

float item_pick_speed(uint16_t item) {
    switch (item_pick_tier(item)) {
        case 1:  return 2.2f;
        case 2:  return 4.0f;
        case 3:  return 6.5f;
        default: return 1.0f;
    }
}

/* ----------------------------------------------------------- inventory */

void inventory_clear(BgStack *slots) {
    memset(slots, 0, sizeof(BgStack) * INVENTORY_SLOTS);
}

int inventory_count(const BgStack *slots, uint16_t item) {
    if (item == BG_AIR) {
        return 0;
    }
    int total = 0;
    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        if (slots[i].item == item) {
            total += slots[i].count;
        }
    }
    return total;
}

bool inventory_add(BgStack *slots, uint16_t item, int count) {
    if (item == BG_AIR || count <= 0) {
        return true;
    }

    /* Top up existing stacks first so the hotbar fills before the pack. */
    for (int i = 0; i < INVENTORY_SLOTS && count > 0; ++i) {
        if (slots[i].item != item || slots[i].count >= MAX_STACK) {
            continue;
        }
        const int room = MAX_STACK - slots[i].count;
        const int moved = room < count ? room : count;
        slots[i].count = (uint8_t)(slots[i].count + moved);
        count -= moved;
    }
    for (int i = 0; i < INVENTORY_SLOTS && count > 0; ++i) {
        if (slots[i].item != BG_AIR) {
            continue;
        }
        const int moved = MAX_STACK < count ? MAX_STACK : count;
        slots[i].item = item;
        slots[i].count = (uint8_t)moved;
        count -= moved;
    }
    return count == 0;
}

bool inventory_remove(BgStack *slots, uint16_t item, int count) {
    if (count <= 0) {
        return true;
    }
    if (inventory_count(slots, item) < count) {
        return false;
    }
    for (int i = 0; i < INVENTORY_SLOTS && count > 0; ++i) {
        if (slots[i].item != item) {
            continue;
        }
        const int taken = slots[i].count < count ? slots[i].count : count;
        slots[i].count = (uint8_t)(slots[i].count - taken);
        count -= taken;
        if (slots[i].count == 0) {
            slots[i].item = BG_AIR;
        }
    }
    return true;
}

/* ------------------------------------------------------------ crafting */

static const BgRecipe kRecipes[] = {
    {
        { { BG_WOOD, 1 } }, 1,
        { BG_PLANKS, 4 }, false
    },
    {
        { { BG_PLANKS, 2 } }, 1,
        { BG_ITEM_STICK, 4 }, false
    },
    {
        { { BG_PLANKS, 4 } }, 1,
        { BG_CRAFTING_TABLE, 1 }, false
    },
    {
        { { BG_STONE, 4 } }, 1,
        { BG_BRICK, 4 }, false
    },
    {
        { { BG_ITEM_ASH_DUST, 2 }, { BG_STONE, 2 } }, 2,
        { BG_ASHBLOCK, 2 }, false
    },
    {
        { { BG_ITEM_EMBER_SHARD, 2 }, { BG_STONE, 3 } }, 2,
        { BG_LUMENSTONE, 2 }, true
    },
    {
        { { BG_ITEM_EMBER_SHARD, 1 }, { BG_ITEM_ASH_DUST, 2 } }, 2,
        { BG_CINDERBRICK, 2 }, true
    },
    {
        { { BG_ITEM_EMBER_SHARD, 4 } }, 1,
        { BG_VOIDSTONE, 2 }, true
    },
    {
        { { BG_ITEM_MEAT, 2 }, { BG_ITEM_ASH_DUST, 1 } }, 2,
        { BG_GLOWMOSS, 1 }, true
    },
    {
        { { BG_PLANKS, 3 }, { BG_ITEM_STICK, 2 } }, 2,
        { BG_ITEM_WOOD_PICK, 1 }, true
    },
    {
        { { BG_STONE, 3 }, { BG_ITEM_STICK, 2 } }, 2,
        { BG_ITEM_STONE_PICK, 1 }, true
    },
    {
        { { BG_ITEM_EMBER_SHARD, 3 }, { BG_ITEM_STICK, 2 } }, 2,
        { BG_ITEM_EMBER_PICK, 1 }, true
    }
};

int recipe_count(void) {
    return (int)(sizeof(kRecipes) / sizeof(kRecipes[0]));
}

const BgRecipe *recipe_at(int index) {
    if (index < 0 || index >= recipe_count()) {
        return NULL;
    }
    return &kRecipes[index];
}

bool recipe_can_craft(const BgStack *slots, const BgRecipe *recipe, bool near_table) {
    if (!recipe || (recipe->needs_table && !near_table)) {
        return false;
    }
    for (int i = 0; i < recipe->input_count; ++i) {
        if (inventory_count(slots, recipe->inputs[i].item) < (int)recipe->inputs[i].count) {
            return false;
        }
    }
    return true;
}

bool recipe_craft(BgStack *slots, const BgRecipe *recipe, bool near_table) {
    if (!recipe_can_craft(slots, recipe, near_table)) {
        return false;
    }
    for (int i = 0; i < recipe->input_count; ++i) {
        inventory_remove(slots, recipe->inputs[i].item, (int)recipe->inputs[i].count);
    }
    inventory_add(slots, recipe->output.item, (int)recipe->output.count);
    return true;
}

bool player_near_table(const World *world, const BgPlayer *player) {
    const int px = (int)(player->x + 0.5f);
    const int py = (int)(player->y + 0.5f);
    const int pz = (int)(player->z + 0.5f);
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dz = -4; dz <= 4; ++dz) {
            for (int dx = -4; dx <= 4; ++dx) {
                if (world_get_block(world, px + dx, py + dy, pz + dz) == BG_CRAFTING_TABLE) {
                    return true;
                }
            }
        }
    }
    return false;
}
