#include "gltrace.h"

#include <stdio.h>
#include <string.h>

static char s_lines[BG_GLTRACE_SLOTS][BG_GLTRACE_TEXT];
static int s_count;
static int s_next;

void gltrace_clear(void) {
    for (int i = 0; i < BG_GLTRACE_SLOTS; ++i) {
        s_lines[i][0] = '\0';
    }
    s_count = 0;
    s_next = 0;
}

void gltrace_add(const char *message) {
    if (!message || message[0] == '\0') {
        return;
    }
    char *slot = s_lines[s_next];
    size_t used = 0;
    for (const char *c = message; *c && used + 1 < BG_GLTRACE_TEXT; ++c) {
        /* The report is line-oriented; keep one message on one line. */
        slot[used++] = (*c == '\n' || *c == '\r' || *c == '\t') ? ' ' : *c;
    }
    slot[used] = '\0';
    s_next = (s_next + 1) % BG_GLTRACE_SLOTS;
    if (s_count < BG_GLTRACE_SLOTS) {
        ++s_count;
    }
}

int gltrace_count(void) {
    return s_count;
}

const char *gltrace_line(int index) {
    if (index < 0 || index >= s_count) {
        return "";
    }
    /* Oldest first: when the window is full the next slot to overwrite is the
     * oldest, and before that the window starts at slot 0. */
    const int oldest = (s_count == BG_GLTRACE_SLOTS) ? s_next : 0;
    return s_lines[(oldest + index) % BG_GLTRACE_SLOTS];
}

size_t gltrace_append(char *out, size_t size) {
    if (!out || size == 0) {
        return 0;
    }
    size_t at = 0;
    out[0] = '\0';
    for (int i = 0; i < s_count; ++i) {
        const char *text = gltrace_line(i);
        if (text[0] == '\0') {
            continue;
        }
        const int written = snprintf(out + at, size - at, "gl: %s\n", text);
        if (written <= 0 || (size_t)written >= size - at) {
            at = size - 1;
            out[at] = '\0';
            return at;
        }
        at += (size_t)written;
    }
    return at;
}
