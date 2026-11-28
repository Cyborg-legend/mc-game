#include "texture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ inflate */

/* A small DEFLATE reader, enough for the IDAT stream of an ordinary PNG. */

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t pos;
    unsigned bitbuf;
    int bitcount;
    int error;
} BitReader;

static unsigned br_bits(BitReader *reader, int need) {
    unsigned value = reader->bitbuf;
    while (reader->bitcount < need) {
        if (reader->pos >= reader->size) {
            reader->error = 1;
            return 0;
        }
        value |= (unsigned)reader->data[reader->pos++] << reader->bitcount;
        reader->bitcount += 8;
    }
    reader->bitbuf = value >> need;
    reader->bitcount -= need;
    return need == 0 ? 0 : value & ((1u << need) - 1u);
}

#define HUFF_MAX_BITS 15

typedef struct {
    short counts[HUFF_MAX_BITS + 1];
    short symbols[288];
} Huffman;

/* Canonical Huffman table built from code lengths; returns 0 when complete. */
static int huff_build(Huffman *huffman, const unsigned char *lengths, int count) {
    memset(huffman->counts, 0, sizeof(huffman->counts));
    for (int i = 0; i < count; ++i) {
        if (lengths[i] > HUFF_MAX_BITS) {
            return -1;
        }
        huffman->counts[lengths[i]]++;
    }
    if (huffman->counts[0] == count) {
        return 0;
    }
    int left = 1;
    for (int length = 1; length <= HUFF_MAX_BITS; ++length) {
        left <<= 1;
        left -= huffman->counts[length];
        if (left < 0) {
            return -1;
        }
    }
    short offsets[HUFF_MAX_BITS + 1];
    offsets[1] = 0;
    for (int length = 1; length < HUFF_MAX_BITS; ++length) {
        offsets[length + 1] = (short)(offsets[length] + huffman->counts[length]);
    }
    for (int i = 0; i < count; ++i) {
        if (lengths[i]) {
            huffman->symbols[offsets[lengths[i]]++] = (short)i;
        }
    }
    return left;
}

static int huff_decode(BitReader *reader, const Huffman *huffman) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int length = 1; length <= HUFF_MAX_BITS; ++length) {
        code |= (int)br_bits(reader, 1);
        const int count = huffman->counts[length];
        if (code - count < first) {
            return huffman->symbols[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} ByteSink;

static bool sink_push(ByteSink *sink, unsigned char byte) {
    if (sink->size == sink->capacity) {
        size_t next = sink->capacity ? sink->capacity * 2 : 4096;
        unsigned char *grown = (unsigned char *)realloc(sink->data, next);
        if (!grown) {
            return false;
        }
        sink->data = grown;
        sink->capacity = next;
    }
    sink->data[sink->size++] = byte;
    return true;
}

static const short kLengthBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
    59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const short kLengthExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const short kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const short kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12,
    13, 13
};

static bool inflate_codes(BitReader *reader, const Huffman *lit, const Huffman *dist,
                          ByteSink *sink) {
    for (;;) {
        const int symbol = huff_decode(reader, lit);
        if (symbol < 0) {
            return false;
        }
        if (symbol < 256) {
            if (!sink_push(sink, (unsigned char)symbol)) {
                return false;
            }
            continue;
        }
        if (symbol == 256) {
            return true;
        }
        const int length_index = symbol - 257;
        if (length_index >= 29) {
            return false;
        }
        int length = kLengthBase[length_index] + (int)br_bits(reader, kLengthExtra[length_index]);
        const int distance_symbol = huff_decode(reader, dist);
        if (distance_symbol < 0 || distance_symbol >= 30) {
            return false;
        }
        const int distance =
            kDistBase[distance_symbol] + (int)br_bits(reader, kDistExtra[distance_symbol]);
        if ((size_t)distance > sink->size) {
            return false;
        }
        while (length-- > 0) {
            if (!sink_push(sink, sink->data[sink->size - (size_t)distance])) {
                return false;
            }
        }
    }
}

static bool inflate_raw(const unsigned char *input, size_t input_size, unsigned char **output,
                        size_t *output_size) {
    unsigned char fixed_lengths[288];
    int symbol = 0;
    for (; symbol < 144; ++symbol) fixed_lengths[symbol] = 8;
    for (; symbol < 256; ++symbol) fixed_lengths[symbol] = 9;
    for (; symbol < 280; ++symbol) fixed_lengths[symbol] = 7;
    for (; symbol < 288; ++symbol) fixed_lengths[symbol] = 8;
    unsigned char fixed_distance_lengths[30];
    for (int i = 0; i < 30; ++i) {
        fixed_distance_lengths[i] = 5;
    }

    Huffman fixed_lit;
    Huffman fixed_dist;
    huff_build(&fixed_lit, fixed_lengths, 288);
    huff_build(&fixed_dist, fixed_distance_lengths, 30);

    BitReader reader;
    memset(&reader, 0, sizeof(reader));
    reader.data = input;
    reader.size = input_size;

    ByteSink sink;
    memset(&sink, 0, sizeof(sink));

    int last = 0;
    do {
        last = (int)br_bits(&reader, 1);
        const int type = (int)br_bits(&reader, 2);
        if (reader.error) {
            break;
        }
        if (type == 0) {
            /* Stored block: skip to the next byte boundary first. */
            reader.bitbuf = 0;
            reader.bitcount = 0;
            if (reader.pos + 4 > reader.size) {
                reader.error = 1;
                break;
            }
            const unsigned length =
                (unsigned)input[reader.pos] | ((unsigned)input[reader.pos + 1] << 8);
            const unsigned complement =
                (unsigned)input[reader.pos + 2] | ((unsigned)input[reader.pos + 3] << 8);
            reader.pos += 4;
            if ((length ^ 0xFFFFu) != complement || reader.pos + length > reader.size) {
                reader.error = 1;
                break;
            }
            for (unsigned i = 0; i < length; ++i) {
                if (!sink_push(&sink, input[reader.pos++])) {
                    reader.error = 1;
                    break;
                }
            }
        } else if (type == 1) {
            if (!inflate_codes(&reader, &fixed_lit, &fixed_dist, &sink)) {
                reader.error = 1;
            }
        } else if (type == 2) {
            const int lit_count = (int)br_bits(&reader, 5) + 257;
            const int dist_count = (int)br_bits(&reader, 5) + 1;
            const int code_count = (int)br_bits(&reader, 4) + 4;
            static const int kOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                            11, 4, 12, 3, 13, 2, 14, 1, 15 };
            unsigned char lengths[320];
            memset(lengths, 0, sizeof(lengths));
            for (int i = 0; i < code_count; ++i) {
                lengths[kOrder[i]] = (unsigned char)br_bits(&reader, 3);
            }
            Huffman code_lengths;
            if (huff_build(&code_lengths, lengths, 19) < 0) {
                reader.error = 1;
                break;
            }
            int index = 0;
            while (index < lit_count + dist_count && !reader.error) {
                const int symbol = huff_decode(&reader, &code_lengths);
                if (symbol < 0) {
                    reader.error = 1;
                    break;
                }
                if (symbol < 16) {
                    lengths[index++] = (unsigned char)symbol;
                } else if (symbol == 16) {
                    if (index == 0) {
                        reader.error = 1;
                        break;
                    }
                    const int repeat = 3 + (int)br_bits(&reader, 2);
                    const unsigned char previous = lengths[index - 1];
                    for (int i = 0; i < repeat && index < lit_count + dist_count; ++i) {
                        lengths[index++] = previous;
                    }
                } else if (symbol == 17) {
                    const int repeat = 3 + (int)br_bits(&reader, 3);
                    for (int i = 0; i < repeat && index < lit_count + dist_count; ++i) {
                        lengths[index++] = 0;
                    }
                } else if (symbol == 18) {
                    const int repeat = 11 + (int)br_bits(&reader, 7);
                    for (int i = 0; i < repeat && index < lit_count + dist_count; ++i) {
                        lengths[index++] = 0;
                    }
                } else {
                    reader.error = 1;
                }
            }
            if (reader.error) {
                break;
            }
            Huffman lit;
            Huffman dist;
            if (huff_build(&lit, lengths, lit_count) < 0 ||
                huff_build(&dist, lengths + lit_count, dist_count) < 0) {
                reader.error = 1;
                break;
            }
            if (!inflate_codes(&reader, &lit, &dist, &sink)) {
                reader.error = 1;
            }
        } else {
            reader.error = 1;
        }
    } while (!last && !reader.error);

    if (reader.error || !sink.data) {
        free(sink.data);
        return false;
    }
    *output = sink.data;
    *output_size = sink.size;
    return true;
}

/* --------------------------------------------------------------------- PNG */

static unsigned read_be32(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) {
        return a;
    }
    return pb <= pc ? b : c;
}

static bool png_decode(const unsigned char *file, size_t size, unsigned char **rgba, int *out_w,
                       int *out_h) {
    static const unsigned char kSignature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (size < 8 || memcmp(file, kSignature, 8) != 0) {
        return false;
    }

    int width = 0;
    int height = 0;
    int depth = 0;
    int color = -1;
    int interlace = 0;
    unsigned char *compressed = NULL;
    size_t compressed_size = 0;
    size_t compressed_capacity = 0;
    unsigned char palette[256 * 3];
    unsigned char palette_alpha[256];
    int palette_count = 0;
    for (int i = 0; i < 256; ++i) {
        palette_alpha[i] = 255;
    }
    bool saw_header = false;

    size_t pos = 8;
    while (pos + 8 <= size) {
        const unsigned length = read_be32(file + pos);
        const unsigned char *type = file + pos + 4;
        const unsigned char *data = file + pos + 8;
        if (pos + 12 + (size_t)length > size) {
            break;
        }
        if (memcmp(type, "IHDR", 4) == 0 && length >= 13) {
            width = (int)read_be32(data);
            height = (int)read_be32(data + 4);
            depth = data[8];
            color = data[9];
            interlace = data[12];
            saw_header = true;
        } else if (memcmp(type, "PLTE", 4) == 0) {
            palette_count = (int)(length / 3);
            if (palette_count > 256) {
                palette_count = 256;
            }
            memcpy(palette, data, (size_t)palette_count * 3);
        } else if (memcmp(type, "tRNS", 4) == 0) {
            const unsigned count = length < 256 ? length : 256;
            memcpy(palette_alpha, data, count);
        } else if (memcmp(type, "IDAT", 4) == 0) {
            if (compressed_size + length > compressed_capacity) {
                size_t next = compressed_capacity ? compressed_capacity : 8192;
                while (next < compressed_size + length) {
                    next *= 2;
                }
                unsigned char *grown = (unsigned char *)realloc(compressed, next);
                if (!grown) {
                    free(compressed);
                    return false;
                }
                compressed = grown;
                compressed_capacity = next;
            }
            memcpy(compressed + compressed_size, data, length);
            compressed_size += length;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + (size_t)length;
    }

    if (!saw_header || !compressed || width <= 0 || height <= 0 || depth != 8 || interlace != 0) {
        free(compressed);
        return false;
    }
    int channels = 0;
    if (color == 0) {
        channels = 1;
    } else if (color == 2) {
        channels = 3;
    } else if (color == 3) {
        channels = 1;
    } else if (color == 4) {
        channels = 2;
    } else if (color == 6) {
        channels = 4;
    } else {
        free(compressed);
        return false;
    }
    if (color == 3 && palette_count == 0) {
        free(compressed);
        return false;
    }

    /* IDAT holds a zlib stream: skip its two-byte header (the trailing Adler-32
     * is simply left unread, since the final block ends the stream). */
    size_t deflate_offset = 0;
    if (compressed_size > 2) {
        const unsigned cmf = compressed[0];
        const unsigned flg = compressed[1];
        if ((cmf & 0x0Fu) == 8 && ((cmf << 8) | flg) % 31u == 0 && (flg & 0x20u) == 0) {
            deflate_offset = 2;
        }
    }

    unsigned char *raw = NULL;
    size_t raw_size = 0;
    const bool inflated = inflate_raw(compressed + deflate_offset,
                                      compressed_size - deflate_offset, &raw, &raw_size);
    free(compressed);
    if (!inflated) {
        return false;
    }

    const size_t stride = (size_t)width * (size_t)channels;
    const size_t expected = (stride + 1) * (size_t)height;
    if (raw_size < expected) {
        free(raw);
        return false;
    }

    /* Undo the per-scanline filters. */
    unsigned char *pixels = (unsigned char *)malloc(stride * (size_t)height);
    if (!pixels) {
        free(raw);
        return false;
    }
    for (int y = 0; y < height; ++y) {
        const unsigned char *line = raw + (stride + 1) * (size_t)y;
        const int filter = line[0];
        const unsigned char *src = line + 1;
        unsigned char *dst = pixels + stride * (size_t)y;
        const unsigned char *up = y > 0 ? pixels + stride * (size_t)(y - 1) : NULL;
        for (size_t x = 0; x < stride; ++x) {
            const int a = x >= (size_t)channels ? dst[x - channels] : 0;
            const int b = up ? up[x] : 0;
            const int c = (up && x >= (size_t)channels) ? up[x - channels] : 0;
            int value = src[x];
            switch (filter) {
                case 0: break;
                case 1: value += a; break;
                case 2: value += b; break;
                case 3: value += (a + b) / 2; break;
                case 4: value += paeth(a, b, c); break;
                default:
                    free(pixels);
                    free(raw);
                    return false;
            }
            dst[x] = (unsigned char)value;
        }
    }
    free(raw);

    unsigned char *out = (unsigned char *)malloc((size_t)width * (size_t)height * 4);
    if (!out) {
        free(pixels);
        return false;
    }
    for (int y = 0; y < height; ++y) {
        const unsigned char *src = pixels + stride * (size_t)y;
        unsigned char *dst = out + (size_t)width * 4 * (size_t)y;
        for (int x = 0; x < width; ++x) {
            unsigned char r = 0;
            unsigned char g = 0;
            unsigned char b = 0;
            unsigned char a = 255;
            const unsigned char *p = src + (size_t)x * channels;
            if (color == 0) {
                r = g = b = p[0];
            } else if (color == 2) {
                r = p[0];
                g = p[1];
                b = p[2];
            } else if (color == 3) {
                const int index = p[0];
                if (index >= palette_count) {
                    free(out);
                    free(pixels);
                    return false;
                }
                r = palette[index * 3 + 0];
                g = palette[index * 3 + 1];
                b = palette[index * 3 + 2];
                a = palette_alpha[index];
            } else if (color == 4) {
                r = g = b = p[0];
                a = p[1];
            } else {
                r = p[0];
                g = p[1];
                b = p[2];
                a = p[3];
            }
            dst[(size_t)x * 4 + 0] = r;
            dst[(size_t)x * 4 + 1] = g;
            dst[(size_t)x * 4 + 2] = b;
            dst[(size_t)x * 4 + 3] = a;
        }
    }
    free(pixels);
    *rgba = out;
    *out_w = width;
    *out_h = height;
    return true;
}

/* --------------------------------------------------------------------- BMP */

static bool bmp_decode(const unsigned char *file, size_t size, unsigned char **rgba, int *out_w,
                       int *out_h) {
    if (size < 54 || file[0] != 'B' || file[1] != 'M') {
        return false;
    }
    const unsigned data_offset = (unsigned)file[10] | ((unsigned)file[11] << 8) |
                                 ((unsigned)file[12] << 16) | ((unsigned)file[13] << 24);
    const unsigned header_size = (unsigned)file[14] | ((unsigned)file[15] << 8) |
                                 ((unsigned)file[16] << 16) | ((unsigned)file[17] << 24);
    if (header_size < 40 || 14 + header_size > size) {
        return false;
    }
    const int width = (int)((unsigned)file[18] | ((unsigned)file[19] << 8) |
                            ((unsigned)file[20] << 16) | ((unsigned)file[21] << 24));
    const int raw_height = (int)((unsigned)file[22] | ((unsigned)file[23] << 8) |
                                 ((unsigned)file[24] << 16) | ((unsigned)file[25] << 24));
    const unsigned planes = (unsigned)file[26] | ((unsigned)file[27] << 8);
    const unsigned bits = (unsigned)file[28] | ((unsigned)file[29] << 8);
    const unsigned compression = (unsigned)file[30] | ((unsigned)file[31] << 8);
    if (width <= 0 || raw_height == 0 || planes != 1 || compression != 0 ||
        (bits != 24 && bits != 32)) {
        return false;
    }
    const int bottom_up = raw_height > 0;
    const int height = raw_height > 0 ? raw_height : -raw_height;
    const size_t row_bytes = ((size_t)width * (bits / 8) + 3) & ~(size_t)3;
    if (data_offset + row_bytes * (size_t)height > size) {
        return false;
    }

    unsigned char *out = (unsigned char *)malloc((size_t)width * (size_t)height * 4);
    if (!out) {
        return false;
    }
    const int bytes_per_pixel = (int)(bits / 8);
    for (int y = 0; y < height; ++y) {
        const int source_row = bottom_up ? height - 1 - y : y;
        const unsigned char *row = file + data_offset + row_bytes * (size_t)source_row;
        unsigned char *dst = out + (size_t)width * 4 * (size_t)y;
        for (int x = 0; x < width; ++x) {
            const unsigned char *p = row + (size_t)x * bytes_per_pixel;
            dst[(size_t)x * 4 + 0] = p[2];
            dst[(size_t)x * 4 + 1] = p[1];
            dst[(size_t)x * 4 + 2] = p[0];
            dst[(size_t)x * 4 + 3] = bytes_per_pixel == 4 ? p[3] : 255;
        }
    }
    *rgba = out;
    *out_w = width;
    *out_h = height;
    return true;
}

/* --------------------------------------------------------------------- API */

void texture_blit_tile(unsigned char *dst, int dst_width, int dst_height, int tile_x,
                       int tile_y, int tile_px, const unsigned char *src, int src_width,
                       int src_height) {
    if (!dst || !src || src_width <= 0 || src_height <= 0 || tile_px <= 0) {
        return;
    }
    for (int y = 0; y < tile_px; ++y) {
        const int dy = tile_y + y;
        if (dy < 0 || dy >= dst_height) {
            continue;
        }
        const int sy = y * src_height / tile_px;
        for (int x = 0; x < tile_px; ++x) {
            const int dx = tile_x + x;
            if (dx < 0 || dx >= dst_width) {
                continue;
            }
            const int sx = x * src_width / tile_px;
            const unsigned char *s = src + ((size_t)sy * (size_t)src_width + (size_t)sx) * 4;
            unsigned char *d = dst + ((size_t)dy * (size_t)dst_width + (size_t)dx) * 4;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = s[3];
        }
    }
}

bool texture_load_rgba(const char *path, unsigned char **rgba, int *width, int *height) {
    if (!path || !rgba || !width || !height) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    const long length = ftell(file);
    if (length <= 0 || length > 64 * 1024 * 1024) {
        fclose(file);
        return false;
    }
    rewind(file);
    unsigned char *bytes = (unsigned char *)malloc((size_t)length);
    if (!bytes) {
        fclose(file);
        return false;
    }
    const size_t got = fread(bytes, 1, (size_t)length, file);
    fclose(file);
    if (got != (size_t)length) {
        free(bytes);
        return false;
    }

    bool ok = false;
    if (bytes[0] == 137) {
        ok = png_decode(bytes, got, rgba, width, height);
    } else if (bytes[0] == 'B' && bytes[1] == 'M') {
        ok = bmp_decode(bytes, got, rgba, width, height);
    }
    free(bytes);
    return ok;
}

static bool try_pair(const char *base, unsigned char **rgba, int *width, int *height) {
    char path[256];
    snprintf(path, sizeof(path), "%s.png", base);
    if (texture_load_rgba(path, rgba, width, height)) {
        return true;
    }
    snprintf(path, sizeof(path), "%s.bmp", base);
    return texture_load_rgba(path, rgba, width, height);
}

int texture_apply_overrides(unsigned char *rgba, int atlas_width, int atlas_height,
                            int tile_pixels) {
    if (!rgba || atlas_width <= 0 || atlas_height <= 0 || tile_pixels <= 0) {
        return 0;
    }
    int applied = 0;
    unsigned char *image = NULL;
    int width = 0;
    int height = 0;

    if (try_pair("textures/atlas", &image, &width, &height)) {
        if (width == atlas_width && height == atlas_height) {
            memcpy(rgba, image, (size_t)atlas_width * (size_t)atlas_height * 4);
            ++applied;
        } else {
            fprintf(stderr, "BlockForge: textures/atlas is %dx%d, expected %dx%d; ignored\n",
                    width, height, atlas_width, atlas_height);
        }
        free(image);
        image = NULL;
    }

    const int columns = atlas_width / tile_pixels;
    const int rows = atlas_height / tile_pixels;
    const int tiles = columns * rows;
    for (int tile = 0; tile < tiles; ++tile) {
        char base[64];
        snprintf(base, sizeof(base), "textures/tile_%02d", tile);
        if (!try_pair(base, &image, &width, &height)) {
            continue;
        }
        texture_blit_tile(rgba, atlas_width, atlas_height, (tile % columns) * tile_pixels,
                          (tile / columns) * tile_pixels, tile_pixels, image, width, height);
        free(image);
        image = NULL;
        ++applied;
    }
    return applied;
}
