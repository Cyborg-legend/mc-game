/* Headless check: every mob's drop table is well formed and the Sovereign's
 * trophy is guaranteed, which is what makes the post-boss recipe reachable. */
#include "loot.h"

#include <stdio.h>

static int checks = 0;
static int failures = 0;

static void expect(int ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

static int contains(const BgStack *drops, int count, uint16_t item) {
    for (int i = 0; i < count; ++i) {
        if (drops[i].item == item) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    BgStack drops[MAX_MOB_DROPS];

    for (int type = 0; type < MOB_TYPE_COUNT; ++type) {
        const int count = loot_mob_drops((MobType)type, drops);
        expect(count >= 0 && count <= MAX_MOB_DROPS, "a mob's drop count fits the list");

        int clean = 1;
        for (int i = 0; i < count; ++i) {
            if (drops[i].item == BG_AIR || drops[i].item >= BG_ITEM_COUNT) {
                clean = 0;
            }
            if (drops[i].count == 0 || drops[i].count > MAX_STACK) {
                clean = 0;
            }
        }
        expect(clean, "every reported drop is a real item in a legal amount");

        int tail_empty = 1;
        for (int i = count; i < MAX_MOB_DROPS; ++i) {
            if (drops[i].item != BG_AIR || drops[i].count != 0) {
                tail_empty = 0;
            }
        }
        expect(tail_empty, "slots past the reported count are cleared");
    }

    expect(loot_mob_drops(MOB_GRAZER, drops) == 1, "a grazer drops one stack");
    expect(drops[0].item == BG_ITEM_MEAT && drops[0].count == 2, "a grazer drops two meat");

    expect(loot_mob_drops(MOB_CINDER_ALPHA, drops) == 2, "the Alpha drops two stacks");
    expect(contains(drops, 2, BG_ITEM_EMBERHEART), "the Alpha drops the Emberheart");

    const int boss = loot_mob_drops(MOB_CINDER_SOVEREIGN, drops);
    expect(boss == 3, "the Sovereign drops three stacks");
    expect(contains(drops, boss, BG_ITEM_CINDER_CORE), "the Sovereign always drops the Core");
    expect(drops[0].item == BG_ITEM_CINDER_CORE, "the Core is the first thing listed");
    expect(drops[0].count == 1, "exactly one Core");
    expect(contains(drops, boss, BG_ITEM_EMBER_SHARD), "the Sovereign still drops shards");
    expect(loot_is_trophy(BG_ITEM_CINDER_CORE), "the Core is the trophy");
    expect(!loot_is_trophy(BG_ITEM_EMBER_SHARD), "a shard is not the trophy");
    expect(!loot_is_trophy(BG_AIR), "nothing is not the trophy");

    /* Repeated calls must not accumulate or drift. */
    BgStack again[MAX_MOB_DROPS];
    loot_mob_drops(MOB_CINDER_SOVEREIGN, again);
    expect(again[0].item == drops[0].item && again[0].count == drops[0].count,
           "the same kill pays the same items");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
