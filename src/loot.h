#ifndef BLOCKFORGE_C_LOOT_H
#define BLOCKFORGE_C_LOOT_H

#include "bg.h"

/* What each mob leaves behind. The table lives on its own so a headless check
 * can pin the rewards without a world: in particular that the Cinder
 * Sovereign always yields a Cinder Core, the trophy that reforges into the
 * Sovereign Pick at a crafting table. */

int loot_mob_drops(MobType type, BgStack out[MAX_MOB_DROPS]);

/* True for the boss trophy, the item that opens the post-boss recipe. */
bool loot_is_trophy(uint16_t item);

#endif /* BLOCKFORGE_C_LOOT_H */
