#ifndef BLOCKFORGE_C_GLTRACE_H
#define BLOCKFORGE_C_GLTRACE_H

#include <stddef.h>

/* A crash report that names the GPU driver and the last few messages the
 * driver sent is far easier to act on than one that only has a position. The
 * window is small on purpose: only the messages near a fault matter, and the
 * report header has to stay cheap to rebuild.
 *
 * This module is deliberately GL-free so a headless check can drive it. The
 * renderer feeds it; the crash snapshot copies it into the pre-formatted
 * header. */

#define BG_GLTRACE_SLOTS 6
#define BG_GLTRACE_TEXT 120

/* Drops every stored line. */
void gltrace_clear(void);

/* Stores one message, replacing the oldest when the window is full. Null and
 * empty messages are ignored, and long ones are truncated. */
void gltrace_add(const char *message);

/* Number of stored lines, at most BG_GLTRACE_SLOTS. */
int gltrace_count(void);

/* Stored line by age: 0 is the oldest still kept. Returns "" past the end. */
const char *gltrace_line(int index);

/* Appends the stored lines to out, each prefixed with "gl: " and ending in a
 * newline. Returns the number of bytes written, excluding the terminator. The
 * result is always terminated, and is empty when nothing was stored. */
size_t gltrace_append(char *out, size_t size);

#endif /* BLOCKFORGE_C_GLTRACE_H */
