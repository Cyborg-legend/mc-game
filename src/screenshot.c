#include "screenshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u16(unsigned char *out, unsigned value) {
    out[0] = (unsigned char)(value & 0xFFu);
    out[1] = (unsigned char)((value >> 8) & 0xFFu);
}

static void put_u32(unsigned char *out, unsigned long value) {
    out[0] = (unsigned char)(value & 0xFFul);
    out[1] = (unsigned char)((value >> 8) & 0xFFul);
    out[2] = (unsigned char)((value >> 16) & 0xFFul);
    out[3] = (unsigned char)((value >> 24) & 0xFFul);
}

bool screenshot_write_bmp(const char *path, int width, int height,
                          const unsigned char *rgba) {
    if (!path || !rgba || width <= 0 || height <= 0) {
        return false;
    }

    /* A row is width * 3 bytes, rounded up to a multiple of four. */
    const unsigned long stride = (((unsigned long)width * 3ul) + 3ul) & ~3ul;
    const unsigned long image_size = stride * (unsigned long)height;
    const unsigned long file_size = 54ul + image_size;

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    unsigned char header[54];
    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    put_u32(header + 2, file_size);
    put_u32(header + 10, 54ul);          /* pixel data offset */
    put_u32(header + 14, 40ul);          /* BITMAPINFOHEADER size */
    put_u32(header + 18, (unsigned long)width);
    put_u32(header + 22, (unsigned long)height); /* positive: bottom-up rows */
    put_u16(header + 26, 1);             /* planes */
    put_u16(header + 28, 24);            /* bits per pixel */
    put_u32(header + 34, image_size);
    put_u32(header + 38, 2835ul);        /* 72 DPI */
    put_u32(header + 42, 2835ul);

    bool ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);

    unsigned char *row = (unsigned char *)calloc((size_t)stride, 1);
    if (!row) {
        fclose(file);
        return false;
    }
    const unsigned long padding = stride - (unsigned long)width * 3ul;
    for (int y = 0; ok && y < height; ++y) {
        const unsigned char *src = rgba + (size_t)y * (size_t)width * 4u;
        for (int x = 0; x < width; ++x) {
            /* BMP stores blue, green, red. */
            row[x * 3 + 0] = src[x * 4 + 2];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 0];
        }
        for (unsigned long p = 0; p < padding; ++p) {
            row[(unsigned long)width * 3ul + p] = 0;
        }
        ok = fwrite(row, 1, (size_t)((unsigned long)width * 3ul + padding), file) ==
             (size_t)((unsigned long)width * 3ul + padding);
    }

    free(row);
    if (fclose(file) != 0) {
        ok = false;
    }
    return ok;
}

bool screenshot_next_path(const char *directory, char *out, size_t size) {
    if (!out || size == 0) {
        return false;
    }
    const char *dir = (directory && directory[0]) ? directory : ".";
    for (int i = 1; i <= 999; ++i) {
        int written = snprintf(out, size, "%s/screenshot_%03d.bmp", dir, i);
        if (written <= 0 || (size_t)written >= size) {
            return false;
        }
        FILE *probe = fopen(out, "rb");
        if (!probe) {
            return true;
        }
        fclose(probe);
    }
    return false;
}
