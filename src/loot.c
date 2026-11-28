#include "loot.h"

int loot_mob_drops(MobType type, BgStack out[MAX_MOB_DROPS]) {
    if (!out) {
        return 0;
    }
    for (int i = 0; i < MAX_MOB_DROPS; ++i) {
        out[i].item = BG_AIR;
        out[i].count = 0;
        out[i].damage = 0;
    }

    switch (type) {
        case MOB_GRAZER:
            out[0].item = BG_ITEM_MEAT;
            out[0].count = 2;
            return 1;
        case MOB_CINDER_HOUND:
            out[0].item = BG_ITEM_EMBER_SHARD;
            out[0].count = 1;
            out[1].item = BG_ITEM_ASH_DUST;
            out[1].count = 2;
            return 2;
        case MOB_ASH_WISP:
            out[0].item = BG_ITEM_ASH_DUST;
            out[0].count = 2;
            return 1;
        case MOB_CINDER_ALPHA:
            out[0].item = BG_ITEM_EMBERHEART;
            out[0].count = 1;
            out[1].item = BG_ITEM_EMBER_SHARD;
            out[1].count = 4;
            return 2;
        case MOB_CINDER_SOVEREIGN:
            /* The trophy is guaranteed: it is the only way to reach the
             * Sovereign Pick, so a bad roll must not lock the reward out. */
            out[0].item = BG_ITEM_CINDER_CORE;
            out[0].count = 1;
            out[1].item = BG_ITEM_EMBER_SHARD;
            out[1].count = 6;
            out[2].item = BG_LUMENSTONE;
            out[2].count = 4;
            return 3;
        default:
            return 0;
    }
}

bool loot_is_trophy(uint16_t item) {
    return item == BG_ITEM_CINDER_CORE;
}
