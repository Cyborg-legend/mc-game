#include "worldmeta.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *out, size_t size, const char *text) {
    size_t used = 0;
    while (text[used] && used + 1 < size) {
        out[used] = text[used];
        ++used;
    }
    out[used] = '\0';
}

static int clamp_realm(int dimension) {
    if (dimension < 0 || dimension >= BG_WORLD_REALMS) {
        return 0;
    }
    return dimension;
}

int worldmeta_parse(const char *line, BgWorldMeta *out) {
    if (!line || !out || line[0] == '\0' || line[0] == '\n') {
        return 0;
    }
    if (strncmp(line, BG_WORLD_META_HEADER, strlen(BG_WORLD_META_HEADER)) == 0) {
        return 0;
    }

    /* Split on tabs by hand: scanf treats a '\t' in the format as "skip any
     * whitespace", which would quietly accept an empty name field. */
    char buffer[192];
    copy_text(buffer, sizeof(buffer), line);
    size_t length = strlen(buffer);
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
        buffer[--length] = '\0';
    }

    char *field[5];
    int count = 0;
    char *cursor = buffer;
    while (count < 5) {
        field[count++] = cursor;
        char *tab = strchr(cursor, '\t');
        if (!tab) {
            break;
        }
        *tab = '\0';
        cursor = tab + 1;
    }
    if (count < 2 || field[0][0] == '\0' || field[1][0] == '\0') {
        return 0;
    }

    char *end = NULL;
    const long id = strtol(field[0], &end, 10);
    if (!end || *end != '\0' || id < 0 || id > INT_MAX) {
        return 0;
    }

    unsigned long long seed = 0;
    if (count > 2 && field[2][0] != '\0') {
        seed = strtoull(field[2], &end, 10);
        if (!end || *end != '\0') {
            seed = 0;
        }
    }

    int dimension = 0;
    if (count > 3 && field[3][0] != '\0') {
        const long parsed = strtol(field[3], &end, 10);
        if (end && *end == '\0') {
            dimension = clamp_realm((int)parsed);
        }
    }

    unsigned long long played = 0;
    if (count > 4 && field[4][0] != '\0') {
        played = strtoull(field[4], &end, 10);
        if (!end || *end != '\0') {
            played = 0;
        }
    }

    memset(out, 0, sizeof(*out));
    out->id = (int)id;
    copy_text(out->name, sizeof(out->name), field[1]);
    out->seed = (uint64_t)seed;
    out->dimension = dimension;
    out->played = (uint64_t)played;
    return 1;
}

int worldmeta_format(const BgWorldMeta *meta, char *out, size_t size) {
    if (!meta || !out || size == 0) {
        return 0;
    }
    char name[BG_WORLD_NAME_MAX];
    size_t used = 0;
    for (const char *c = meta->name; *c && used + 1 < sizeof(name); ++c) {
        /* Tabs and line breaks would split the record on the next read. */
        name[used++] = (*c == '\t' || *c == '\n' || *c == '\r') ? ' ' : *c;
    }
    name[used] = '\0';
    if (used == 0) {
        snprintf(name, sizeof(name), "World");
    }

    const int written = snprintf(out, size, "%d\t%s\t%llu\t%d\t%llu\n", meta->id, name,
                                 (unsigned long long)meta->seed,
                                 clamp_realm(meta->dimension),
                                 (unsigned long long)meta->played);
    if (written <= 0 || (size_t)written >= size) {
        return 0;
    }
    return 1;
}

const char *worldmeta_ago(uint64_t played, uint64_t now, char *out, size_t size) {
    if (!out || size == 0) {
        return "";
    }
    if (played == 0) {
        snprintf(out, size, "NEVER");
        return out;
    }
    if (played >= now) {
        snprintf(out, size, "JUST NOW");
        return out;
    }

    const uint64_t delta = now - played;
    if (delta < 60) {
        snprintf(out, size, "JUST NOW");
    } else if (delta < 3600) {
        snprintf(out, size, "%llu MIN AGO", (unsigned long long)(delta / 60));
    } else if (delta < 86400) {
        snprintf(out, size, "%llu H AGO", (unsigned long long)(delta / 3600));
    } else if (delta < 14 * 86400) {
        const uint64_t days = delta / 86400;
        snprintf(out, size, "%llu DAY%s AGO", (unsigned long long)days,
                 days == 1 ? "" : "S");
    } else if (delta < 60 * 86400) {
        snprintf(out, size, "%llu WEEKS AGO", (unsigned long long)(delta / (7 * 86400)));
    } else {
        snprintf(out, size, "%llu MONTHS AGO", (unsigned long long)(delta / (30 * 86400)));
    }
    return out;
}

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static uint32_t cell_hash(uint64_t seed, int x, int z) {
    uint32_t h = (uint32_t)(seed ^ (seed >> 32));
    h ^= (uint32_t)((uint32_t)x * 73856093u);
    h ^= (uint32_t)((uint32_t)z * 19349663u);
    return mix32(h);
}

int worldmeta_preview_tone(uint64_t seed, int x, int z) {
    /* Two scales: fine grain for coastlines, coarse for land masses. */
    const uint32_t fine = cell_hash(seed, x, z);
    const uint32_t coarse = cell_hash(seed ^ 0x5bd1e995u, x / 3, z / 3);
    const uint32_t blended = (fine % BG_WORLD_PREVIEW_TONES) +
                             (coarse % BG_WORLD_PREVIEW_TONES) * 2u;
    int tone = (int)(blended / 3u);
    if (tone >= BG_WORLD_PREVIEW_TONES) {
        tone = BG_WORLD_PREVIEW_TONES - 1;
    }
    return tone;
}
