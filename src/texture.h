#ifndef BLOCKFORGE_C_TEXTURE_H
#define BLOCKFORGE_C_TEXTURE_H

#include <stdbool.h>

/* Decodes a PNG or a BMP into a freshly allocated RGBA buffer. PNGs must be
 * 8-bit and non-interlaced (grey, grey+alpha, RGB, RGBA or palette); BMPs must
 * be uncompressed 24- or 32-bit. The caller frees *rgba. */
bool texture_load_rgba(const char *path, unsigned char **rgba, int *width, int *height);

/* Nearest-neighbour scales an image into one tile of a larger RGBA image. */
void texture_blit_tile(unsigned char *dst, int dst_width, int dst_height, int tile_x,
                       int tile_y, int tile_px, const unsigned char *src, int src_width,
                       int src_height);

/* Paints textures/atlas.(png|bmp) and textures/tile_NN.(png|bmp) over an atlas
 * that was already painted in code. Returns how many images were applied; a
 * missing folder or unreadable file is not an error, the painted tile stays. */
int texture_apply_overrides(unsigned char *rgba, int atlas_width, int atlas_height,
                            int tile_pixels);

#endif /* BLOCKFORGE_C_TEXTURE_H */
