/* Headless check: the world index format reads both versions, round-trips,
 * words the last-played stamp and gives every seed its own thumbnail. */
#include "worldmeta.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void expect(int ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

int main(void) {
    BgWorldMeta meta;
    char line[160];

    /* --- version 2 round trip --- */
    BgWorldMeta source;
    memset(&source, 0, sizeof(source));
    source.id = 7;
    snprintf(source.name, sizeof(source.name), "Stone Harbour");
    source.seed = 987654321ull;
    source.dimension = 2;
    source.played = 1764300000ull;
    expect(worldmeta_format(&source, line, sizeof(line)) == 1, "a record formats");
    expect(strchr(line, '\n') == line + strlen(line) - 1, "the record ends in one newline");
    expect(strstr(line, "BLOCKFORGE") == NULL, "records do not carry the header");

    memset(&meta, 0, sizeof(meta));
    expect(worldmeta_parse(line, &meta) == 1, "the record parses back");
    expect(meta.id == 7, "id survives the round trip");
    expect(strcmp(meta.name, "Stone Harbour") == 0, "the name survives, spaces and all");
    expect(meta.seed == 987654321ull, "the seed survives");
    expect(meta.dimension == 2, "the realm survives");
    expect(meta.played == 1764300000ull, "the last-played stamp survives");

    /* --- version 1 lines still load --- */
    expect(worldmeta_parse("3\tOld Save\t42\t1\n", &meta) == 1, "a version 1 line parses");
    expect(meta.id == 3 && meta.seed == 42ull, "version 1 keeps id and seed");
    expect(meta.dimension == 1, "version 1 keeps the realm");
    expect(meta.played == 0, "a version 1 world reads as never played");

    expect(worldmeta_parse("BLOCKFORGE-WORLDS 2\n", &meta) == 0, "the header is not a world");
    expect(worldmeta_parse("\n", &meta) == 0, "a blank line is skipped");
    expect(worldmeta_parse("nonsense\n", &meta) == 0, "garbage is skipped");
    expect(worldmeta_parse("\tName\t1\t0\t0\n", &meta) == 0, "a line without an id is skipped");
    expect(worldmeta_parse("4\t\t1\t0\t0\n", &meta) == 0, "a line without a name is skipped");

    /* --- odd realms and names --- */
    expect(worldmeta_parse("5\tRealm\t1\t9\t0\n", &meta) == 1 && meta.dimension == 0,
           "an out-of-range realm falls back to the upperworld");
    expect(worldmeta_parse("6\tNegative\t1\t-4\t0\n", &meta) == 1 && meta.dimension == 0,
           "a negative realm falls back too");

    BgWorldMeta tabs;
    memset(&tabs, 0, sizeof(tabs));
    tabs.id = 1;
    snprintf(tabs.name, sizeof(tabs.name), "a\tb\nc");
    expect(worldmeta_format(&tabs, line, sizeof(line)) == 1, "a name with control characters formats");
    expect(strchr(line, '\n') == line + strlen(line) - 1, "the record still has one newline");
    expect(worldmeta_parse(line, &meta) == 1 && strcmp(meta.name, "a b c") == 0,
           "control characters become spaces so the record still parses");

    BgWorldMeta empty;
    memset(&empty, 0, sizeof(empty));
    empty.id = 2;
    expect(worldmeta_format(&empty, line, sizeof(line)) == 1, "an empty name still formats");
    expect(worldmeta_parse(line, &meta) == 1 && strcmp(meta.name, "World") == 0,
           "an empty name becomes World");

    char tiny[8];
    expect(worldmeta_format(&source, tiny, sizeof(tiny)) == 0, "a short buffer is refused");
    expect(worldmeta_format(NULL, line, sizeof(line)) == 0, "a null record is refused");

    /* --- last-played wording --- */
    char ago[32];
    const uint64_t now = 1800000000ull;
    expect(strcmp(worldmeta_ago(0, now, ago, sizeof(ago)), "NEVER") == 0, "0 reads as never");
    expect(strcmp(worldmeta_ago(now - 5, now, ago, sizeof(ago)), "JUST NOW") == 0,
           "seconds read as just now");
    expect(strcmp(worldmeta_ago(now - 300, now, ago, sizeof(ago)), "5 MIN AGO") == 0,
           "minutes are counted");
    expect(strcmp(worldmeta_ago(now - 3 * 3600, now, ago, sizeof(ago)), "3 H AGO") == 0,
           "hours are counted");
    expect(strcmp(worldmeta_ago(now - 86400, now, ago, sizeof(ago)), "1 DAY AGO") == 0,
           "one day is singular");
    expect(strcmp(worldmeta_ago(now - 2 * 86400, now, ago, sizeof(ago)), "2 DAYS AGO") == 0,
           "days are counted");
    expect(strcmp(worldmeta_ago(now - 21 * 86400, now, ago, sizeof(ago)), "3 WEEKS AGO") == 0,
           "weeks take over after a fortnight");
    expect(strcmp(worldmeta_ago(now - 65 * 86400, now, ago, sizeof(ago)), "2 MONTHS AGO") == 0,
           "months take over after two moons");
    expect(strcmp(worldmeta_ago(now + 500, now, ago, sizeof(ago)), "JUST NOW") == 0,
           "a stamp in the future reads as just now");

    /* --- preview tones --- */
    int in_range = 1;
    int varied = 0;
    for (int x = 0; x < BG_WORLD_PREVIEW_W; ++x) {
        for (int z = 0; z < BG_WORLD_PREVIEW_H; ++z) {
            const int tone = worldmeta_preview_tone(1337u, x, z);
            if (tone < 0 || tone >= BG_WORLD_PREVIEW_TONES) {
                in_range = 0;
            }
            if (tone != 0) {
                varied = 1;
            }
        }
    }
    expect(in_range, "every preview cell is a palette tone");
    expect(varied, "the preview is not one flat colour");

    int stable = 1;
    int differs = 0;
    for (int x = 0; x < BG_WORLD_PREVIEW_W; ++x) {
        for (int z = 0; z < BG_WORLD_PREVIEW_H; ++z) {
            const int a = worldmeta_preview_tone(1337u, x, z);
            const int b = worldmeta_preview_tone(1337u, x, z);
            if (a != b) {
                stable = 0;
            }
            if (a != worldmeta_preview_tone(4242u, x, z)) {
                differs = 1;
            }
        }
    }
    expect(stable, "the same seed and cell always give the same tone");
    expect(differs, "a different seed gives a different picture");

    /* Tones should use the whole palette rather than just one or two entries. */
    int seen[BG_WORLD_PREVIEW_TONES] = { 0 };
    int used = 0;
    for (int x = 0; x < 40; ++x) {
        for (int z = 0; z < 40; ++z) {
            const int tone = worldmeta_preview_tone(2026u, x, z);
            if (!seen[tone]) {
                seen[tone] = 1;
                ++used;
            }
        }
    }
    expect(used >= 4, "a large sample uses at least four palette tones");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
