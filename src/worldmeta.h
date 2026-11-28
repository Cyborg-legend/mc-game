#ifndef BLOCKFORGE_C_WORLDMETA_H
#define BLOCKFORGE_C_WORLDMETA_H

#include <stddef.h>
#include <stdint.h>

/* The world list is a tab-separated file. Version 1 stored id, name, seed and
 * the realm last stood in; version 2 adds the last-played time as a Unix
 * stamp (0 for a world that has never been entered). A version 1 line is still
 * read, so an existing list is not thrown away.
 *
 * Nothing here touches the filesystem or GL, so a headless check can pin the
 * format and the relative-time wording. */

#define BG_WORLD_NAME_MAX 24
#define BG_WORLD_META_VERSION 2
#define BG_WORLD_META_HEADER "BLOCKFORGE-WORLDS"
#define BG_WORLD_REALMS 3

/* The world-list thumbnail: a small deterministic sample of the seed, so two
 * different seeds cannot show the same picture by accident. */
#define BG_WORLD_PREVIEW_W 5
#define BG_WORLD_PREVIEW_H 3
#define BG_WORLD_PREVIEW_CELLS (BG_WORLD_PREVIEW_W * BG_WORLD_PREVIEW_H)
#define BG_WORLD_PREVIEW_TONES 6

typedef struct BgWorldMeta {
    int id;
    char name[BG_WORLD_NAME_MAX];
    uint64_t seed;
    int dimension;   /* realm id, 0..BG_WORLD_REALMS-1 after parsing */
    uint64_t played; /* Unix seconds when the world was last opened */
} BgWorldMeta;

/* Parses one index line (version 1 or 2). Returns 1 when the line names a
 * usable world, 0 for blank, malformed or header lines. Name tabs, newlines
 * and a missing stamp are handled; an out-of-range realm becomes 0. */
int worldmeta_parse(const char *line, BgWorldMeta *out);

/* Formats one line in the current version, ending in '\n'. Returns 1 on
 * success, 0 when the buffer is too small. The name is truncated to fit and
 * control characters are replaced so a stray tab cannot split the record. */
int worldmeta_format(const BgWorldMeta *meta, char *out, size_t size);

/* Human wording for a last-played stamp: "NEVER", "JUST NOW", "5 MIN AGO",
 * "3 H AGO", "2 DAYS AGO", "3 WEEKS AGO" or "5 MONTHS AGO". Writes into out
 * and returns it, so it can be used inside snprintf. */
const char *worldmeta_ago(uint64_t played, uint64_t now, char *out, size_t size);

/* One thumbnail cell's tone, 0..BG_WORLD_PREVIEW_TONES-1. Deterministic for a
 * seed and cell, and mixed from two scales so the picture reads as land and
 * water rather than static. */
int worldmeta_preview_tone(uint64_t seed, int x, int z);

#endif /* BLOCKFORGE_C_WORLDMETA_H */
