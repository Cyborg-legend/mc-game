#ifndef BLOCKFORGE_C_SCREENSHOT_H
#define BLOCKFORGE_C_SCREENSHOT_H

#include <stdbool.h>
#include <stddef.h>

/* Writes an uncompressed 24-bit BMP from a bottom-up RGBA buffer, the exact
 * layout glReadPixels hands back, so no flip is needed. Rows are padded to a
 * four-byte boundary as the format requires. */
bool screenshot_write_bmp(const char *path, int width, int height,
                          const unsigned char *rgba);

/* Finds the first free "screenshot_NNN.bmp" in a directory, filling out.
 * Returns false when every name up to 999 is taken. */
bool screenshot_next_path(const char *directory, char *out, size_t size);

#endif /* BLOCKFORGE_C_SCREENSHOT_H */
