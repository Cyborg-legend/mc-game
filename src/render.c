#include "render.h"
#include "texture.h"
#include "shaders.h"
#include "interp.h"
#include "effects.h"
#include "gltrace.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------- shaders */

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(const char *vertex_source, const char *fragment_source) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(program);
        program = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

/* -------------------------------------------------------------- camera */

static void camera_basis(const BgCamera *camera, float forward[3], float right[3], float up[3]) {
    const float cp = cosf(camera->pitch);
    forward[0] = cp * cosf(camera->yaw);
    forward[1] = sinf(camera->pitch);
    forward[2] = cp * sinf(camera->yaw);

    right[0] = -sinf(camera->yaw);
    right[1] = 0.0f;
    right[2] = cosf(camera->yaw);

    up[0] = -cosf(camera->yaw) * forward[1];
    up[1] = cosf(camera->yaw) * forward[0] + sinf(camera->yaw) * forward[2];
    up[2] = -sinf(camera->yaw) * forward[1];
}

static void camera_view_matrix(const BgCamera *camera, float m[16]) {
    float f[3], r[3], u[3];
    camera_basis(camera, f, r, u);
    const float eye[3] = { camera->x, camera->y, camera->z };

    m[0] = r[0];  m[1] = u[0];  m[2] = -f[0]; m[3] = 0.0f;
    m[4] = r[1];  m[5] = u[1];  m[6] = -f[1]; m[7] = 0.0f;
    m[8] = r[2];  m[9] = u[2];  m[10] = -f[2]; m[11] = 0.0f;
    m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
    m[15] = 1.0f;
}

static void camera_projection_matrix(const BgCamera *camera, float m[16]) {
    const float aspect = camera->aspect > 0.0001f ? camera->aspect : 16.0f / 9.0f;
    const float f = 1.0f / tanf(camera->fov * 0.5f);
    const float n = camera->near_plane;
    const float far = camera->far_plane;

    memset(m, 0, sizeof(float) * 16);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + n) / (n - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * n) / (n - far);
}

/* --------------------------------------------------------------- atlas */

typedef enum {
    PAT_UNUSED = -1,
    PAT_FLAT = 0,
    PAT_COBBLE,
    PAT_DIRT,
    PAT_GRASS_TOP,
    PAT_GRASS_SIDE,
    PAT_BARK,
    PAT_RINGS,
    PAT_FOLIAGE,
    PAT_LIQUID,
    PAT_CHUNKY,
    PAT_PLANKS,
    PAT_GRID,
    PAT_BRICK,
    PAT_ORE,
    PAT_GLOW,
    PAT_SWIRL,
    PAT_CRYSTAL,
    PAT_ASH,
    PAT_MOSS,
    PAT_SPECKLE
} Pattern;

/* Tiles are painted at 32x32 so cobbles, mortar, grain and ore veins have room
 * to read. The grid stays 16x16 like the mesh UVs expect, so every tile is one
 * square of a power-of-two atlas and mip level 4 keeps tiles from bleeding. */
#define ATLAS_TILE 32
#define ATLAS_COLS 16
#define ATLAS_ROWS 16
#define ATLAS_WIDTH (ATLAS_COLS * ATLAS_TILE)
#define ATLAS_HEIGHT (ATLAS_ROWS * ATLAS_TILE)
#define ATLAS_MAX_LEVEL 4

typedef struct {
    unsigned char r, g, b;
    unsigned char ar, ag, ab;
    Pattern pattern;
} TileDef;

static const TileDef kTiles[45] = {
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },      /* void */
    { 126, 126, 131,  86,  86,  92, PAT_COBBLE },      /* stone */
    { 120,  86,  58,  86,  60,  40, PAT_DIRT },        /* dirt */
    {  92, 156,  60,  58, 108,  38, PAT_GRASS_TOP },   /* grass top */
    { 120,  86,  58,  98, 162,  62, PAT_GRASS_SIDE },  /* grass side */
    { 104,  80,  52,  64,  46,  28, PAT_BARK },        /* bark */
    { 152, 120,  80, 112,  84,  52, PAT_RINGS },       /* wood rings */
    {  56, 116,  46,  34,  76,  28, PAT_FOLIAGE },     /* leaves */
    { 226, 214, 168, 200, 186, 140, PAT_SPECKLE },     /* sand */
    {  54, 100, 190, 120, 172, 232, PAT_LIQUID },      /* water */
    {  56,  56,  60,  26,  26,  30, PAT_CHUNKY },      /* bedrock */
    { 168, 134,  86, 118,  90,  54, PAT_PLANKS },      /* planks */
    { 142, 112,  74, 102,  76,  48, PAT_BARK },        /* sticks */
    { 154, 116,  66, 104,  74,  42, PAT_GRID },        /* craft top */
    { 138, 102,  60,  98,  70,  40, PAT_PLANKS },      /* craft side */
    { 158,  74,  58, 200, 190, 178, PAT_BRICK },       /* brick */
    { 116,  44,  40,  72,  24,  22, PAT_COBBLE },      /* emberstone */
    {  78,  64,  68,  54,  44,  48, PAT_ASH },         /* ashsoil */
    { 232, 206, 110, 255, 246, 190, PAT_GLOW },        /* lumenstone */
    { 192,  80,  26, 255, 158,  48, PAT_LIQUID },      /* magma */
    {  48,  36,  62, 126,  90, 172, PAT_CRYSTAL },     /* voidstone */
    {  92,  60,  54, 152, 132, 122, PAT_BRICK },       /* cinderbrick */
    { 228, 226, 216, 198, 196, 188, PAT_SPECKLE },     /* pale quartz */
    { 104,  44,  44,  64,  24,  24, PAT_BARK },        /* crimson bark */
    { 152,  74,  66, 106,  44,  40, PAT_RINGS },       /* crimson rings */
    { 128,  44,  44,  76,  22,  22, PAT_FOLIAGE },     /* bloodleaves */
    { 128, 128, 132, 240, 146,  52, PAT_ORE },         /* emberlode */
    { 128, 128, 132, 214, 196, 168, PAT_ORE },         /* ironlode */
    {  84,  42, 150, 214, 140, 255, PAT_SWIRL },       /* rift portal */
    {  68, 148, 120, 176, 255, 214, PAT_MOSS },        /* glowmoss */
    {  76,  72,  74, 106, 102, 102, PAT_CHUNKY },      /* cinderstone */
    { 178, 172, 162, 146, 140, 132, PAT_ASH },         /* ashblock */
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },       /* item sprites 32..38 */
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },
    [41] = { 196, 148,  82, 255, 226, 150, PAT_FLAT },   /* torch */
    [42] = { 150, 102,  54,  96,  62,  30, PAT_PLANKS }, /* chest side */
    [43] = { 168, 122,  66,  96,  62,  30, PAT_GRID },   /* chest top */
    [44] = { 110,  30,  24, 255, 190,  90, PAT_SWIRL }   /* cinder gate */
};

static uint32_t pixel_hash(int tile, int px, int py) {
    uint32_t h = (uint32_t)tile * 374761393u + (uint32_t)px * 668265263u +
                 (uint32_t)py * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

static float pixel_noise(int tile, int px, int py) {
    return (float)(pixel_hash(tile, px, py) & 0xFFFFu) / 65535.0f;
}

static float cell_noise(int tile, int cx, int cy, int salt) {
    uint32_t h = (uint32_t)tile * 374761393u + (uint32_t)cx * 668265263u +
                 (uint32_t)cy * 2246822519u + (uint32_t)salt * 2654435761u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static unsigned char clamp255(float value) {
    if (value < 0.0f) return 0;
    if (value > 255.0f) return 255;
    return (unsigned char)value;
}

static void cobble_shade(int tile, int px, int py, float n, float *shade, float *mix) {
    const int row = py / 8;
    const int gx = (px + ((row & 1) ? 4 : 0)) & (ATLAS_TILE - 1);
    const int lx = gx & 7;
    const int ly = py & 7;
    const float cell = cell_noise(tile, gx >> 3, row, 3);
    const float edge = fminf(fminf((float)lx, 7.0f - (float)lx),
                             fminf((float)ly, 7.0f - (float)ly));
    *shade = 0.84f + cell * 0.30f + (n - 0.5f) * 0.10f;
    *mix = 0.0f;
    if (edge < 1.0f) {
        *mix = 1.0f;
    } else if (edge < 1.9f) {
        *shade *= 0.90f;
    }
    if (n > 0.94f) *shade *= 1.10f;
}

static void paint_pixel(const TileDef *def, int tile, int px, int py, unsigned char out[3]) {
    const float base[3] = { (float)def->r, (float)def->g, (float)def->b };
    const float accent[3] = { (float)def->ar, (float)def->ag, (float)def->ab };
    const float n = pixel_noise(tile, px, py);
    const float dx = (float)px - 15.5f;
    const float dy = (float)py - 15.5f;
    const float radius = sqrtf(dx * dx + dy * dy);

    float shade = 1.0f;
    float accent_mix = 0.0f;

    switch (def->pattern) {
        case PAT_FLAT:
            shade = 0.94f + n * 0.08f;
            break;
        case PAT_COBBLE:
            cobble_shade(tile, px, py, n, &shade, &accent_mix);
            break;
        case PAT_DIRT: {
            const float clod = cell_noise(tile, px / 4, py / 4, 9);
            shade = 0.82f + n * 0.26f;
            if (clod > 0.78f) shade *= 0.84f;
            else if (clod < 0.18f) shade *= 1.14f;
            if ((px * 5 + py * 11) % 31 == 0) shade *= 0.68f;
            if ((px * 7 + py * 3) % 37 == 0) accent_mix = 0.55f;
            break;
        }
        case PAT_GRASS_TOP: {
            const float clump = cell_noise(tile, px / 5, py / 5, 5);
            shade = (0.86f + clump * 0.26f) * (0.94f + n * 0.12f);
            if ((px * 9 + py * 4) % 27 == 0) shade *= 0.74f;
            if ((px * 4 + py * 11) % 29 == 0) accent_mix = 0.85f;
            if ((px * 13 + py * 5) % 41 == 0) shade *= 1.16f;
            break;
        }
        case PAT_GRASS_SIDE: {
            const float fringe_noise = cell_noise(tile, px, 0, 7);
            const int fringe = 8 + (int)(fringe_noise * 5.0f) + (((px + 1) % 5 == 0) ? 2 : 0);
            if (py < fringe) {
                accent_mix = 0.90f;
                shade = 0.94f + n * 0.16f;
                if ((px * 7 + py * 5) % 23 == 0) shade *= 0.74f;
                if ((px * 5 + py * 3) % 31 == 0) shade *= 1.12f;
            } else {
                const float clod = cell_noise(tile, px / 4, py / 4, 9);
                shade = 0.82f + n * 0.26f;
                if (clod > 0.76f) shade *= 0.86f;
                if ((px * 5 + py * 11) % 31 == 0) shade *= 0.70f;
            }
            break;
        }
        case PAT_BARK:
            shade = 0.88f + 0.16f * sinf((float)px * 0.8f + (float)tile * 0.6f) + n * 0.06f;
            if (px % 7 == 0) accent_mix = 0.95f;
            else if (px % 7 == 3) shade *= 0.78f;
            if ((px + py * 5) % 41 == 0) shade *= 1.14f;
            break;
        case PAT_RINGS:
            shade = 0.80f + 0.24f * sinf(radius * 1.05f + n * 0.35f);
            if (radius < 2.6f) accent_mix = 0.95f;
            if (radius > 15.2f) shade *= 0.84f;
            break;
        case PAT_FOLIAGE: {
            const float clump = cell_noise(tile, px / 3, py / 3, 11);
            shade = (0.64f + clump * 0.54f) * (0.95f + n * 0.10f);
            if (clump > 0.84f) accent_mix = 1.0f;
            if (clump < 0.16f) shade = 1.16f;
            break;
        }
        case PAT_LIQUID: {
            const float wave = sinf((float)py * 0.62f + sinf((float)px * 0.22f) * 1.7f +
                                    n * 0.9f);
            shade = 0.90f + wave * 0.14f;
            if (wave > 0.84f) {
                accent_mix = 1.0f;
                shade = 1.06f;
            }
            break;
        }
        case PAT_CHUNKY: {
            const float cell = cell_noise(tile, px / 4, py / 4, 13);
            shade = 0.78f + cell * 0.32f;
            if (px % 4 == 0 || py % 4 == 0) accent_mix = 0.85f;
            if (n > 0.90f) shade *= 0.80f;
            break;
        }
        case PAT_PLANKS: {
            const int plank = py / 8;
            const float grain = cell_noise(tile, plank, (px + plank * 7) / 9, 17);
            shade = (0.88f + (grain - 0.5f) * 0.22f) * (0.96f + n * 0.08f);
            if (py % 8 == 0) accent_mix = 0.90f;
            if ((px + plank * 11) % 16 == 0) accent_mix = 0.55f;
            if (((py * 3 + (int)(2.0f * sinf((float)px * 0.55f + (float)plank))) % 10) == 0)
                shade *= 0.92f;
            if ((px == 3 || px == 28) && (py % 8 == 3)) accent_mix = 1.0f;
            break;
        }
        case PAT_GRID:
            shade = 0.88f + n * 0.10f;
            if ((px * 3 + py) % 9 == 0) shade *= 0.94f;
            if (px < 3 || py < 3 || px > 28 || py > 28) accent_mix = 0.90f;
            else if ((px - 3) % 9 == 0 || (py - 3) % 9 == 0) accent_mix = 0.70f;
            break;
        case PAT_BRICK: {
            const int row = py / 8;
            const int bx = (px + ((row & 1) ? 8 : 0)) & (ATLAS_TILE - 1);
            const float brick = cell_noise(tile, bx / 16, row, 19);
            if (py % 8 == 0 || bx % 16 == 0) {
                accent_mix = 1.0f;
                shade = 0.96f;
            } else {
                shade = 0.82f + brick * 0.24f + n * 0.06f;
                if (n > 0.93f) shade *= 0.86f;
            }
            break;
        }
        case PAT_ORE: {
            float stone_shade = 1.0f;
            float stone_mix = 0.0f;
            cobble_shade(tile, px, py, n, &stone_shade, &stone_mix);
            shade = stone_shade;
            accent_mix = stone_mix * 0.35f;
            float ore = 0.0f;
            for (int k = 0; k < 3; ++k) {
                const int ox = 6 + (int)(cell_noise(tile, k, 0, 23) * 20.0f);
                const int oy = 6 + (int)(cell_noise(tile, k, 1, 29) * 20.0f);
                const float ox_d = (float)(px - ox);
                const float oy_d = (float)(py - oy);
                if (sqrtf(ox_d * ox_d + oy_d * oy_d) < 3.6f + n * 1.6f) {
                    ore = 1.0f;
                }
            }
            if (ore > 0.0f) {
                accent_mix = 1.0f;
                shade = 0.98f + n * 0.16f;
            }
            break;
        }
        case PAT_GLOW:
            shade = 1.12f - radius * 0.028f + n * 0.07f;
            if (((px * 5 + py * 3) % 13) == 0) shade *= 0.80f;
            if (radius < 2.6f) accent_mix = 0.90f;
            if (radius > 13.5f) accent_mix = 0.45f;
            break;
        case PAT_SWIRL: {
            const float angle = atan2f(dy, dx);
            const float band = sinf(radius * 1.15f - angle * 2.0f + n * 0.5f);
            accent_mix = band > 0.0f ? 1.0f : 0.0f;
            shade = 0.90f + band * 0.12f + n * 0.06f;
            if (radius < 3.0f) shade = 1.28f;
            break;
        }
        case PAT_CRYSTAL: {
            const int band = (px + py * 2) % 13;
            const float facet = cell_noise(tile, px / 8, py / 8, 31);
            if (band < 3) {
                accent_mix = 0.60f;
                shade = 1.18f;
            } else if (band < 6) {
                shade = 0.70f;
            } else {
                shade = 0.92f + n * 0.10f;
            }
            if (px % 8 == 0 || py % 8 == 0) shade *= 0.72f;
            shade *= 0.90f + facet * 0.20f;
            break;
        }
        case PAT_ASH: {
            const float clump = cell_noise(tile, px / 4, py / 4, 37);
            shade = 0.86f + n * 0.20f;
            if (clump > 0.85f) shade *= 0.80f;
            if (n > 0.96f) accent_mix = 0.70f;
            if ((px * 7 + py * 5) % 43 == 0) shade *= 1.12f;
            break;
        }
        case PAT_MOSS: {
            const float clump = cell_noise(tile, px / 4, py / 4, 41);
            const float fuzz = pixel_noise(tile + 53, px, py);
            if (clump > 0.42f) {
                accent_mix = clamp01((clump - 0.42f) * 2.4f);
                shade = 0.98f + fuzz * 0.20f;
            } else {
                shade = 0.66f + fuzz * 0.20f;
            }
            if (fuzz > 0.95f) shade *= 1.20f;
            break;
        }
        case PAT_SPECKLE:
            shade = 0.90f + n * 0.12f;
            if (((px * 5 + py * 11 + tile * 3) % 19) == 0) shade *= 0.84f;
            if (((px * 13 + py * 7) % 37) == 0) shade *= 1.10f;
            break;
        case PAT_UNUSED:
        default:
            shade = 0.0f;
            break;
    }

    accent_mix = clamp01(accent_mix);
    for (int i = 0; i < 3; ++i) {
        const float lit = base[i] * shade;
        const float alt = accent[i] * shade;
        out[i] = clamp255(lit + (alt - lit) * accent_mix);
    }
}

/* Kept after upload so the hotbar can show the real block faces as icons. */
static unsigned char g_atlas_pixels[ATLAS_WIDTH * ATLAS_HEIGHT * 4];

/* ------------------------------------------------------------- item tiles */

/* Items are not blocks, so they get their own 32x32 tiles: an 8x8 pixel-art
 * sprite scaled 4x with a transparent background the hotbar lets through. */
#define ITEM_TILE_BASE 32
#define ITEM_TILE_COUNT 9

typedef struct {
    const char *rows[8];
    unsigned char palette[4][3];
} ItemArt;

static const ItemArt kItemArt[ITEM_TILE_COUNT] = {
    {   /* stick */
        { "......12", ".....123", "....123.", "...123..",
          "..123...", ".123....", "123.....", "12......" },
        { {  92,  64,  34 }, { 150, 112,  64 }, { 198, 162, 106 }, {   0,   0,   0 } }
    },
    {   /* ember shard */
        { "...11...", "..1221..", "..1321..", ".123321.",
          ".123321.", "..1331..", "...11...", "........" },
        { {  86,  28,  10 }, { 222, 110,  30 }, { 255, 214, 120 }, {   0,   0,   0 } }
    },
    {   /* ash dust */
        { "....3...", "........", ".3......", "....2...",
          "..1221..", ".122221.", "12222221", "11111111" },
        { {  78,  76,  74 }, { 150, 146, 140 }, { 214, 210, 204 }, {   0,   0,   0 } }
    },
    {   /* meat */
        { "..111...", ".12421..", ".122221.", ".122221.",
          "..12221.", ".3322...", "333.....", "33......" },
        { { 116,  30,  30 }, { 196,  64,  64 }, { 232, 226, 204 }, { 226, 120, 110 } }
    },
    {   /* wooden pickaxe */
        { "..1221..", ".1....21", "12..11.2", "...33...",
          "...34...", "...34...", "...34...", "...34..." },
        { { 150, 116,  72 }, { 200, 166, 108 }, { 108,  80,  46 }, { 146, 112,  68 } }
    },
    {   /* stone pickaxe */
        { "..1221..", ".1....21", "12..11.2", "...33...",
          "...34...", "...34...", "...34...", "...34..." },
        { { 118, 118, 124 }, { 178, 178, 184 }, { 108,  80,  46 }, { 146, 112,  68 } }
    },
    {   /* ember pickaxe */
        { "..1221..", ".1....21", "12..11.2", "...33...",
          "...34...", "...34...", "...34...", "...34..." },
        { { 196,  84,  30 }, { 255, 206, 120 }, { 108,  80,  46 }, { 146, 112,  68 } }
    },
    {   /* rift key */
        { "...11...", "..1221..", "..1331..", "..1331..",
          "..1331..", "..1221..", "...33...", "...34..." },
        { {  84,  42, 150 }, { 214, 140, 255 }, { 255, 214, 120 }, { 176, 128,  60 } }
    },
    {   /* emberheart */
        { "..1..1..", ".121121.", "12222221", "12333221",
          ".123321.", "..1331..", "...11...", "........" },
        { {  86,  28,  10 }, { 222, 100,  30 }, { 255, 214, 120 }, { 255, 160,  60 } }
    }
};

static void paint_item_tile(unsigned char *pixels, int index) {
    const ItemArt *art = &kItemArt[index];
    const int tile = ITEM_TILE_BASE + index;
    const int origin_x = (tile % ATLAS_COLS) * ATLAS_TILE;
    const int origin_y = (tile / ATLAS_COLS) * ATLAS_TILE;
    const int scale = ATLAS_TILE / 8;

    for (int py = 0; py < ATLAS_TILE; ++py) {
        const char *row = art->rows[py / scale];
        for (int px = 0; px < ATLAS_TILE; ++px) {
            const char code = row[px / scale];
            unsigned char *p =
                &pixels[((origin_y + py) * ATLAS_WIDTH + origin_x + px) * 4];
            if (code < '1' || code > '4') {
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
                continue;
            }
            const unsigned char *rgb = art->palette[code - '1'];
            const float shade = 0.94f + pixel_noise(tile, px / 2, py / 2) * 0.12f;
            p[0] = clamp255((float)rgb[0] * shade);
            p[1] = clamp255((float)rgb[1] * shade);
            p[2] = clamp255((float)rgb[2] * shade);
            p[3] = 255;
        }
    }
}

static GLuint build_atlas(void) {
    unsigned char *pixels = g_atlas_pixels;

    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; ++i) {
        pixels[i * 4 + 0] = 0;
        pixels[i * 4 + 1] = 0;
        pixels[i * 4 + 2] = 0;
        pixels[i * 4 + 3] = 255;
    }

    const int tile_count = (int)(sizeof(kTiles) / sizeof(kTiles[0]));
    for (int tile = 0; tile < tile_count; ++tile) {
        /* 32..40 belong to the item sprites painted below. */
        if (tile >= ITEM_TILE_BASE && tile < ITEM_TILE_BASE + ITEM_TILE_COUNT) {
            continue;
        }
        const TileDef *def = &kTiles[tile];
        if (def->pattern == PAT_UNUSED) {
            continue;
        }
        const int origin_x = (tile % ATLAS_COLS) * ATLAS_TILE;
        const int origin_y = (tile / ATLAS_COLS) * ATLAS_TILE;
        for (int py = 0; py < ATLAS_TILE; ++py) {
            for (int px = 0; px < ATLAS_TILE; ++px) {
                unsigned char rgb[3];
                paint_pixel(def, tile, px, py, rgb);
                unsigned char *p =
                    &pixels[((origin_y + py) * ATLAS_WIDTH + (origin_x + px)) * 4];
                p[0] = rgb[0];
                p[1] = rgb[1];
                p[2] = rgb[2];
                p[3] = 255;
            }
        }
    }

    for (int item = 0; item < ITEM_TILE_COUNT; ++item) {
        paint_item_tile(pixels, item);
    }

    /* Supplied art lives in textures/ and simply paints over the code-made
     * tiles; a missing folder keeps the painted atlas. */
    const int overrides = texture_apply_overrides(pixels, ATLAS_WIDTH, ATLAS_HEIGHT, ATLAS_TILE);
    if (overrides > 0) {
        fprintf(stderr, "BlockForge: applied %d texture override(s) from textures/\n", overrides);
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, ATLAS_MAX_LEVEL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}

/* ------------------------------------------------------------- renderer */

/* Driver messages are rare but valuable: a shader link warning or a driver
 * reset right before a crash explains the fault better than a position does.
 * The ring lives in gltrace.c so it can be tested without a GPU. */
static void GLAPIENTRY on_gl_message(GLenum source, GLenum type, GLuint id, GLenum severity,
                                     GLsizei length, const GLchar *message,
                                     const void *user) {
    (void)source;
    (void)type;
    (void)id;
    (void)length;
    (void)user;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }
    gltrace_add(message);
}

bool renderer_init(BgRenderer *renderer) {
    memset(renderer, 0, sizeof(*renderer));
    gltrace_clear();

    glewExperimental = GL_TRUE;
    const GLenum status = glewInit();
    if (status != GLEW_OK) {
        fprintf(stderr, "GLEW init failed: %s\n", (const char *)glewGetErrorString(status));
        return false;
    }
    while (glGetError() != GL_NO_ERROR) {
    }

    const char *version = (const char *)glGetString(GL_VERSION);
    const char *device = (const char *)glGetString(GL_RENDERER);
    fprintf(stderr, "OpenGL %s | %s\n", version ? version : "unknown",
            device ? device : "unknown renderer");

    if (!GLEW_VERSION_3_3) {
        fprintf(stderr, "OpenGL 3.3 is required but the driver does not expose it\n");
        return false;
    }

    /* Name the driver in the crash report even if it never says anything. */
    char banner[BG_GLTRACE_TEXT];
    snprintf(banner, sizeof(banner), "driver %s | %s", version ? version : "unknown",
             device ? device : "unknown");
    gltrace_add(banner);

    /* Debug output needs GL 4.3 or KHR_debug; a 3.3-only driver just has no
     * messages to give, so this stays optional. */
    if (GLEW_VERSION_4_3 || GLEW_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(on_gl_message, NULL);
    }

    renderer->program = link_program(shader_world_vertex(), shader_world_fragment());
    renderer->ui_program = link_program(shader_ui_vertex(), shader_ui_fragment());
    if (!renderer->program || !renderer->ui_program) {
        return false;
    }

    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ibo);
    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ibo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, ao));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, light));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, tile));
    glBindVertexArray(0);

    glGenVertexArrays(1, &renderer->mob_vao);
    glGenBuffers(1, &renderer->mob_vbo);
    glGenBuffers(1, &renderer->mob_ibo);
    glBindVertexArray(renderer->mob_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->mob_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->mob_ibo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, ao));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, light));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, tile));
    glBindVertexArray(0);

    glGenVertexArrays(1, &renderer->ui_vao);
    glGenBuffers(1, &renderer->ui_vbo);
    glBindVertexArray(renderer->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    renderer->atlas = build_atlas();
    renderer_set_environment(renderer, BG_UPPERWORLD);
    return true;
}

void renderer_shutdown(BgRenderer *renderer) {
    renderer_clear_chunks(renderer);
    free(renderer->chunks);
    renderer->chunks = NULL;
    if (renderer->vbo) glDeleteBuffers(1, &renderer->vbo);
    if (renderer->ibo) glDeleteBuffers(1, &renderer->ibo);
    if (renderer->vao) glDeleteVertexArrays(1, &renderer->vao);
    if (renderer->ui_vbo) glDeleteBuffers(1, &renderer->ui_vbo);
    if (renderer->ui_vao) glDeleteVertexArrays(1, &renderer->ui_vao);
    mesh_free(&renderer->mob_mesh);
    if (renderer->mob_vbo) glDeleteBuffers(1, &renderer->mob_vbo);
    if (renderer->mob_ibo) glDeleteBuffers(1, &renderer->mob_ibo);
    if (renderer->mob_vao) glDeleteVertexArrays(1, &renderer->mob_vao);
    if (renderer->atlas) glDeleteTextures(1, &renderer->atlas);
    if (renderer->program) glDeleteProgram(renderer->program);
    if (renderer->ui_program) glDeleteProgram(renderer->ui_program);
    memset(renderer, 0, sizeof(*renderer));
}

void renderer_set_environment(BgRenderer *renderer, Dimension dimension) {
    renderer->dimension = dimension;
    if (dimension == BG_UNDERWORLD) {
        const float sky[3] = { 0.10f, 0.03f, 0.03f };
        const float fog[3] = { 0.20f, 0.05f, 0.04f };
        const float ambient[3] = { 0.34f, 0.16f, 0.14f };
        const float light[3] = { 1.00f, 0.52f, 0.22f };
        memcpy(renderer->sky, sky, sizeof(sky));
        memcpy(renderer->fog_color, fog, sizeof(fog));
        memcpy(renderer->ambient, ambient, sizeof(ambient));
        memcpy(renderer->light_color, light, sizeof(light));
        renderer->fog_start = 22.0f;
        renderer->fog_density = 0.0075f;
    } else if (dimension == BG_CINDERHOLD) {
        /* A sealed furnace: the only light is what the rock gives off. */
        const float sky[3] = { 0.05f, 0.02f, 0.03f };
        const float fog[3] = { 0.14f, 0.03f, 0.02f };
        const float ambient[3] = { 0.30f, 0.13f, 0.10f };
        const float light[3] = { 1.00f, 0.45f, 0.18f };
        memcpy(renderer->sky, sky, sizeof(sky));
        memcpy(renderer->fog_color, fog, sizeof(fog));
        memcpy(renderer->ambient, ambient, sizeof(ambient));
        memcpy(renderer->light_color, light, sizeof(light));
        renderer->fog_start = 18.0f;
        renderer->fog_density = 0.0085f;
    } else {
        const float sky[3] = { 0.52f, 0.72f, 0.95f };
        const float fog[3] = { 0.62f, 0.78f, 0.96f };
        const float ambient[3] = { 0.44f, 0.44f, 0.46f };
        const float light[3] = { 1.00f, 0.98f, 0.90f };
        memcpy(renderer->sky, sky, sizeof(sky));
        memcpy(renderer->fog_color, fog, sizeof(fog));
        memcpy(renderer->ambient, ambient, sizeof(ambient));
        memcpy(renderer->light_color, light, sizeof(light));
        renderer->fog_start = 80.0f;
        renderer->fog_density = 0.0018f;
    }
}

static float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

/* Blends the day and night palettes and remembers where the sun is. */
void renderer_set_time(BgRenderer *renderer, float phase) {
    phase -= floorf(phase);
    renderer->sun_phase = phase;
    const float elevation = sinf(phase * 6.2831853f);
    float day = (elevation + 0.16f) / 0.46f;
    if (day < 0.0f) day = 0.0f;
    if (day > 1.0f) day = 1.0f;
    renderer->sun_strength = day;

    if (renderer->dimension != BG_UPPERWORLD) {
        return; /* The two deep realms glow the same no matter the hour. */
    }

    const float dusk = expf(-elevation * elevation * 16.0f);
    const float day_sky[3] = { 0.52f, 0.72f, 0.95f };
    const float night_sky[3] = { 0.015f, 0.025f, 0.07f };
    const float dusk_sky[3] = { 0.48f, 0.26f, 0.22f };
    const float day_fog[3] = { 0.62f, 0.78f, 0.96f };
    const float night_fog[3] = { 0.035f, 0.045f, 0.10f };
    const float dusk_fog[3] = { 0.62f, 0.34f, 0.24f };
    const float day_amb[3] = { 0.44f, 0.44f, 0.46f };
    const float night_amb[3] = { 0.09f, 0.11f, 0.18f };
    const float day_light[3] = { 1.0f, 0.98f, 0.90f };
    const float night_light[3] = { 0.26f, 0.30f, 0.44f };

    for (int i = 0; i < 3; ++i) {
        const float sky = lerp_f(night_sky[i], day_sky[i], day);
        const float fog = lerp_f(night_fog[i], day_fog[i], day);
        renderer->sky[i] = lerp_f(sky, dusk_sky[i], dusk * 0.55f);
        renderer->fog_color[i] = lerp_f(fog, dusk_fog[i], dusk * 0.55f);
        renderer->ambient[i] = lerp_f(night_amb[i], day_amb[i], day);
        renderer->light_color[i] = lerp_f(night_light[i], day_light[i], day);
    }
    renderer->fog_start = lerp_f(46.0f, 80.0f, day);
    renderer->fog_density = lerp_f(0.0036f, 0.0018f, day);
}

static BgChunkDraw *find_chunk_draw(BgRenderer *renderer, uint64_t key) {
    for (size_t i = 0; i < renderer->chunk_count; ++i) {
        if (renderer->chunks[i].key == key) {
            return &renderer->chunks[i];
        }
    }
    return NULL;
}

void renderer_upload_chunk(BgRenderer *renderer, uint64_t key, const BgMesh *mesh) {
    BgChunkDraw *draw = find_chunk_draw(renderer, key);
    if (mesh->index_count == 0) {
        if (draw) {
            draw->index_count = 0;
        }
        return;
    }

    if (!draw) {
        if (renderer->chunk_count == renderer->chunk_capacity) {
            const size_t next = renderer->chunk_capacity ? renderer->chunk_capacity * 2 : 256;
            BgChunkDraw *grown = (BgChunkDraw *)realloc(renderer->chunks, next * sizeof(BgChunkDraw));
            if (!grown) {
                return;
            }
            renderer->chunks = grown;
            renderer->chunk_capacity = next;
        }
        draw = &renderer->chunks[renderer->chunk_count++];
        memset(draw, 0, sizeof(*draw));
        draw->key = key;
        glGenVertexArrays(1, &draw->vao);
        glGenBuffers(1, &draw->vbo);
        glGenBuffers(1, &draw->ibo);

        glBindVertexArray(draw->vao);
        glBindBuffer(GL_ARRAY_BUFFER, draw->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw->ibo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, ao));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, light));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, tile));
        glBindVertexArray(0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, draw->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * sizeof(BgVertex)),
                 mesh->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)),
                 mesh->indices, GL_STATIC_DRAW);
    draw->index_count = (GLsizei)mesh->index_count;
}

void renderer_clear_chunks(BgRenderer *renderer) {
    for (size_t i = 0; i < renderer->chunk_count; ++i) {
        BgChunkDraw *draw = &renderer->chunks[i];
        if (draw->vbo) glDeleteBuffers(1, &draw->vbo);
        if (draw->ibo) glDeleteBuffers(1, &draw->ibo);
        if (draw->vao) glDeleteVertexArrays(1, &draw->vao);
    }
    renderer->chunk_count = 0;
}

void renderer_get_size(BgRenderer *renderer, int *width, int *height) {
    GLint viewport[4] = { 0, 0, 1280, 720 };
    glGetIntegerv(GL_VIEWPORT, viewport);
    renderer->width = viewport[2] > 0 ? viewport[2] : 1280;
    renderer->height = viewport[3] > 0 ? viewport[3] : 720;
    if (width) *width = renderer->width;
    if (height) *height = renderer->height;
}

static void draw_sky(BgRenderer *renderer, const BgCamera *camera) {
    int width = 1280;
    int height = 720;
    renderer_get_size(renderer, &width, &height);

    const float top[3] = { renderer->sky[0] * 0.88f, renderer->sky[1] * 0.88f,
                           renderer->sky[2] * 0.88f };
    const float bottom[3] = { renderer->fog_color[0], renderer->fog_color[1],
                              renderer->fog_color[2] };
    const float w = (float)width;
    const float h = (float)height;
    const float verts[6][6] = {
        { 0.0f, 0.0f, top[0], top[1], top[2], 1.0f },
        { w, 0.0f, top[0], top[1], top[2], 1.0f },
        { w, h, bottom[0], bottom[1], bottom[2], 1.0f },
        { 0.0f, 0.0f, top[0], top[1], top[2], 1.0f },
        { w, h, bottom[0], bottom[1], bottom[2], 1.0f },
        { 0.0f, h, bottom[0], bottom[1], bottom[2], 1.0f }
    };

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(renderer->ui_program);
    glUniform2f(glGetUniformLocation(renderer->ui_program, "screenSize"), w, h);
    glBindVertexArray(renderer->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(verts), verts, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (renderer->dimension == BG_UPPERWORLD) {
        const float phase = renderer->sun_phase;
        const float tan_half = tanf(camera->fov * 0.5f);
        const float aspect = camera->aspect > 0.0001f ? camera->aspect : 16.0f / 9.0f;
        const float cp = cosf(camera->pitch);
        const float forward[3] = { cp * cosf(camera->yaw), sinf(camera->pitch),
                                   cp * sinf(camera->yaw) };
        const float right[3] = { -sinf(camera->yaw), 0.0f, cosf(camera->yaw) };
        const float up[3] = { right[1] * forward[2] - right[2] * forward[1],
                              right[2] * forward[0] - right[0] * forward[2],
                              right[0] * forward[1] - right[1] * forward[0] };

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        /* Stars fade in as the sun sets. */
        const float night = clamp01(-sinf(phase * 6.2831853f) * 2.2f);
        if (night > 0.05f) {
            float star_verts[6 * 6 * 48];
            int used = 0;
            for (int i = 0; i < 48; ++i) {
                const float sx = (float)((i * 92821 + 17) % 1000) / 1000.0f * w;
                const float sy = (float)((i * 68917 + 53) % 620) / 620.0f * h * 0.7f;
                const float size = 1.5f + (float)(i % 3);
                const float a = night * (0.35f + (float)(i % 5) * 0.12f);
                const float quad[6][6] = {
                    { sx, sy, 1, 1, 1, a }, { sx + size, sy, 1, 1, 1, a },
                    { sx + size, sy + size, 1, 1, 1, a },
                    { sx, sy, 1, 1, 1, a }, { sx + size, sy + size, 1, 1, 1, a },
                    { sx, sy + size, 1, 1, 1, a }
                };
                for (int v = 0; v < 6; ++v) {
                    for (int k = 0; k < 6; ++k) {
                        star_verts[used++] = quad[v][k];
                    }
                }
            }
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * (size_t)used),
                         star_verts, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, used / 6);
        }

        /* The sun and moon ride the same arc, half a cycle apart. */
        for (int body = 0; body < 2; ++body) {
            const float body_phase = phase + (body ? 0.5f : 0.0f);
            const float elevation = sinf(body_phase * 6.2831853f);
            if (elevation < -0.06f) {
                continue;
            }
            const float azimuth = body_phase * 6.2831853f;
            const float ce = cosf(elevation * 1.2f);
            const float dir[3] = { cosf(azimuth) * ce, elevation, sinf(azimuth) * ce };
            const float vz = dir[0] * forward[0] + dir[1] * forward[1] + dir[2] * forward[2];
            if (vz < 0.18f) {
                continue;
            }
            const float vx = dir[0] * right[0] + dir[1] * right[1] + dir[2] * right[2];
            const float vy = dir[0] * up[0] + dir[1] * up[1] + dir[2] * up[2];
            const float sx = w * 0.5f + (vx / vz) * (w * 0.5f / tan_half) / aspect;
            const float sy = h * 0.5f - (vy / vz) * (h * 0.5f / tan_half);
            const float size = body ? 62.0f : 96.0f;
            const float alpha = clamp01(elevation * 3.0f + 0.4f);
            const float tint[3] = { body ? 0.82f : 1.0f, body ? 0.86f : 0.94f,
                                    body ? 0.92f : 0.68f };
            const float quad[6][6] = {
                { sx - size, sy - size, tint[0], tint[1], tint[2], alpha },
                { sx + size, sy - size, tint[0], tint[1], tint[2], alpha },
                { sx + size, sy + size, tint[0], tint[1], tint[2], alpha },
                { sx - size, sy - size, tint[0], tint[1], tint[2], alpha },
                { sx + size, sy + size, tint[0], tint[1], tint[2], alpha },
                { sx - size, sy + size, tint[0], tint[1], tint[2], alpha }
            };
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quad), quad, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glDisable(GL_BLEND);
    }

    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void renderer_draw(BgRenderer *renderer, const BgCamera *camera) {
    renderer->draw_calls = 0;

    glClearColor(renderer->sky[0], renderer->sky[1], renderer->sky[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_sky(renderer, camera);

    float view[16];
    float projection[16];
    camera_view_matrix(camera, view);
    camera_projection_matrix(camera, projection);

    glUseProgram(renderer->program);
    const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "model"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, GL_FALSE, projection);
    glUniform3f(glGetUniformLocation(renderer->program, "viewPos"), camera->x, camera->y, camera->z);
    glUniform3fv(glGetUniformLocation(renderer->program, "ambientColor"), 1, renderer->ambient);
    glUniform3fv(glGetUniformLocation(renderer->program, "lightColor"), 1, renderer->light_color);
    glUniform3fv(glGetUniformLocation(renderer->program, "fogColor"), 1, renderer->fog_color);
    glUniform1f(glGetUniformLocation(renderer->program, "fogStart"), renderer->fog_start);
    glUniform1f(glGetUniformLocation(renderer->program, "fogDensity"), renderer->fog_density);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->atlas);
    glUniform1i(glGetUniformLocation(renderer->program, "texAtlas"), 0);

    for (size_t i = 0; i < renderer->chunk_count; ++i) {
        const BgChunkDraw *draw = &renderer->chunks[i];
        if (draw->index_count == 0) {
            continue;
        }
        glBindVertexArray(draw->vao);
        glDrawElements(GL_TRIANGLES, draw->index_count, GL_UNSIGNED_INT, NULL);
        renderer->draw_calls++;
    }
    glBindVertexArray(0);
}

/* ------------------------------------------------------------------- UI */

static bool ui_reserve(BgUi *ui, size_t floats_needed) {
    if (ui->capacity >= floats_needed) {
        return true;
    }
    size_t next = ui->capacity ? ui->capacity : 4096;
    while (next < floats_needed) {
        next *= 2;
    }
    float *grown = (float *)realloc(ui->verts, next * sizeof(float));
    if (!grown) {
        return false;
    }
    ui->verts = grown;
    ui->capacity = next;
    return true;
}

void ui_clear(BgUi *ui) {
    ui->floats = 0;
}

void ui_free(BgUi *ui) {
    free(ui->verts);
    ui->verts = NULL;
    ui->floats = 0;
    ui->capacity = 0;
}

void ui_rect(BgUi *ui, float x, float y, float w, float h, const float color[4]) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    if (!ui_reserve(ui, ui->floats + 6 * 6)) {
        return;
    }
    const float x1 = x + w;
    const float y1 = y + h;
    const float verts[6][6] = {
        { x, y, color[0], color[1], color[2], color[3] },
        { x1, y, color[0], color[1], color[2], color[3] },
        { x1, y1, color[0], color[1], color[2], color[3] },
        { x, y, color[0], color[1], color[2], color[3] },
        { x1, y1, color[0], color[1], color[2], color[3] },
        { x, y1, color[0], color[1], color[2], color[3] }
    };
    memcpy(&ui->verts[ui->floats], verts, sizeof(verts));
    ui->floats += 6 * 6;
}

/* Tiles an atlas texture across a rectangle with 4x4 colour samples per cell.
 * Used for the dirt menu backdrop, so menus share the world's real pixels. */
void ui_tile_texture(BgUi *ui, float x, float y, float width, float height, int tile,
                     float cell, float shade, float alpha) {
    const int origin_x = (tile % ATLAS_COLS) * ATLAS_TILE;
    const int origin_y = (tile / ATLAS_COLS) * ATLAS_TILE;
    const int cells = 4;
    const int span = ATLAS_TILE / cells;
    const float step = cell / (float)cells;
    const int cols = (int)ceilf(width / cell);
    const int rows = (int)ceilf(height / cell);
    const float inv = shade / (float)(span * span * 255);

    for (int ty = 0; ty < rows; ++ty) {
        for (int tx = 0; tx < cols; ++tx) {
            for (int iy = 0; iy < cells; ++iy) {
                for (int ix = 0; ix < cells; ++ix) {
                    int sum[3] = { 0, 0, 0 };
                    for (int py = 0; py < span; ++py) {
                        for (int px = 0; px < span; ++px) {
                            const unsigned char *src = &g_atlas_pixels[
                                ((origin_y + iy * span + py) * ATLAS_WIDTH + origin_x +
                                 ix * span + px) * 4];
                            sum[0] += src[0];
                            sum[1] += src[1];
                            sum[2] += src[2];
                        }
                    }
                    const float px0 = x + (float)tx * cell + (float)ix * step;
                    const float py0 = y + (float)ty * cell + (float)iy * step;
                    float pw = step + 0.6f;
                    float ph = step + 0.6f;
                    if (px0 + pw > x + width) pw = x + width - px0;
                    if (py0 + ph > y + height) ph = y + height - py0;
                    if (pw <= 0.0f || ph <= 0.0f) {
                        continue;
                    }
                    const float color[4] = { (float)sum[0] * inv, (float)sum[1] * inv,
                                             (float)sum[2] * inv, alpha };
                    ui_rect(ui, px0, py0, pw, ph, color);
                }
            }
        }
    }
}

/* 5x7 bitmap font; bit n of a column is the pixel on row n (0 = top). */
typedef struct {
    char code;
    unsigned char columns[5];
} Glyph;

static const Glyph kGlyphs[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '!', { 0x00, 0x00, 0x5F, 0x00, 0x00 } },
    { '"', { 0x00, 0x07, 0x00, 0x07, 0x00 } },
    { '#', { 0x14, 0x7F, 0x14, 0x7F, 0x14 } },
    { '%', { 0x23, 0x13, 0x08, 0x64, 0x62 } },
    { '&', { 0x36, 0x49, 0x55, 0x22, 0x50 } },
    { '\'', { 0x00, 0x05, 0x03, 0x00, 0x00 } },
    { '(', { 0x00, 0x1C, 0x22, 0x41, 0x00 } },
    { ')', { 0x00, 0x41, 0x22, 0x1C, 0x00 } },
    { '*', { 0x14, 0x08, 0x3E, 0x08, 0x14 } },
    { '+', { 0x08, 0x08, 0x3E, 0x08, 0x08 } },
    { ',', { 0x00, 0x50, 0x30, 0x00, 0x00 } },
    { '-', { 0x08, 0x08, 0x08, 0x08, 0x08 } },
    { '.', { 0x00, 0x60, 0x60, 0x00, 0x00 } },
    { '/', { 0x20, 0x10, 0x08, 0x04, 0x02 } },
    { '0', { 0x3E, 0x51, 0x49, 0x45, 0x3E } },
    { '1', { 0x00, 0x42, 0x7F, 0x40, 0x00 } },
    { '2', { 0x42, 0x61, 0x51, 0x49, 0x46 } },
    { '3', { 0x21, 0x41, 0x45, 0x4B, 0x31 } },
    { '4', { 0x18, 0x14, 0x12, 0x7F, 0x10 } },
    { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
    { '6', { 0x3C, 0x4A, 0x49, 0x49, 0x30 } },
    { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
    { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
    { '9', { 0x06, 0x49, 0x49, 0x29, 0x1E } },
    { ':', { 0x00, 0x36, 0x36, 0x00, 0x00 } },
    { ';', { 0x00, 0x56, 0x36, 0x00, 0x00 } },
    { '<', { 0x08, 0x14, 0x22, 0x41, 0x00 } },
    { '=', { 0x14, 0x14, 0x14, 0x14, 0x14 } },
    { '>', { 0x00, 0x41, 0x22, 0x14, 0x08 } },
    { '?', { 0x02, 0x01, 0x51, 0x09, 0x06 } },
    { '@', { 0x32, 0x49, 0x79, 0x41, 0x3E } },
    { 'A', { 0x7E, 0x11, 0x11, 0x11, 0x7E } },
    { 'B', { 0x7F, 0x49, 0x49, 0x49, 0x36 } },
    { 'C', { 0x3E, 0x41, 0x41, 0x41, 0x22 } },
    { 'D', { 0x7F, 0x41, 0x41, 0x22, 0x1C } },
    { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
    { 'F', { 0x7F, 0x09, 0x09, 0x09, 0x01 } },
    { 'G', { 0x3E, 0x41, 0x49, 0x49, 0x7A } },
    { 'H', { 0x7F, 0x08, 0x08, 0x08, 0x7F } },
    { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
    { 'J', { 0x20, 0x40, 0x41, 0x3F, 0x01 } },
    { 'K', { 0x7F, 0x08, 0x14, 0x22, 0x41 } },
    { 'L', { 0x7F, 0x40, 0x40, 0x40, 0x40 } },
    { 'M', { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
    { 'N', { 0x7F, 0x04, 0x08, 0x10, 0x7F } },
    { 'O', { 0x3E, 0x41, 0x41, 0x41, 0x3E } },
    { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
    { 'Q', { 0x3E, 0x41, 0x51, 0x21, 0x5E } },
    { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
    { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
    { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
    { 'U', { 0x3F, 0x40, 0x40, 0x40, 0x3F } },
    { 'V', { 0x1F, 0x20, 0x40, 0x20, 0x1F } },
    { 'W', { 0x3F, 0x40, 0x38, 0x40, 0x3F } },
    { 'X', { 0x63, 0x14, 0x08, 0x14, 0x63 } },
    { 'Y', { 0x07, 0x08, 0x70, 0x08, 0x07 } },
    { 'Z', { 0x61, 0x51, 0x49, 0x45, 0x43 } }
};

static const Glyph *find_glyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    for (size_t i = 0; i < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++i) {
        if (kGlyphs[i].code == c) {
            return &kGlyphs[i];
        }
    }
    return NULL;
}

void ui_text(BgUi *ui, float x, float y, float scale, const char *text, const float color[4]) {
    float pen = x;
    for (const char *p = text; *p; ++p) {
        const Glyph *glyph = find_glyph(*p);
        if (glyph) {
            for (int col = 0; col < 5; ++col) {
                const unsigned char bits = glyph->columns[col];
                if (!bits) {
                    continue;
                }
                for (int row = 0; row < 7; ++row) {
                    if (bits & (1u << row)) {
                        ui_rect(ui, pen + (float)col * scale, y + (float)row * scale,
                                scale, scale, color);
                    }
                }
            }
        }
        pen += 6.0f * scale;
    }
}

float ui_text_width(const char *text, float scale) {
    const size_t length = strlen(text);
    return length ? (float)length * 6.0f * scale - scale : 0.0f;
}

void ui_text_centered(BgUi *ui, float center_x, float y, float scale, const char *text,
                      const float color[4]) {
    ui_text(ui, center_x - ui_text_width(text, scale) * 0.5f, y, scale, text, color);
}

void renderer_draw_ui(BgRenderer *renderer, const BgUi *ui) {
    if (!renderer->ui_program || ui->floats == 0) {
        return;
    }
    int width = 1280;
    int height = 720;
    renderer_get_size(renderer, &width, &height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(renderer->ui_program);
    glUniform2f(glGetUniformLocation(renderer->ui_program, "screenSize"),
                (float)width, (float)height);
    glBindVertexArray(renderer->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui->floats * sizeof(float)), ui->verts,
                 GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ui->floats / 6));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void block_color(BlockId block, float rgb[3]) {
    const int tile = block_tile(block, 2);
    const TileDef *def = &kTiles[tile];
    rgb[0] = (float)def->r / 255.0f;
    rgb[1] = (float)def->g / 255.0f;
    rgb[2] = (float)def->b / 255.0f;
}

void item_color(uint16_t item, float rgb[3]) {
    if (item < BG_BLOCK_COUNT) {
        block_color((BlockId)item, rgb);
        return;
    }
    switch (item) {
        case BG_ITEM_STICK:
            rgb[0] = 0.55f; rgb[1] = 0.40f; rgb[2] = 0.22f;
            break;
        case BG_ITEM_EMBER_SHARD:
            rgb[0] = 0.97f; rgb[1] = 0.53f; rgb[2] = 0.16f;
            break;
        case BG_ITEM_ASH_DUST:
            rgb[0] = 0.62f; rgb[1] = 0.60f; rgb[2] = 0.58f;
            break;
        case BG_ITEM_MEAT:
            rgb[0] = 0.78f; rgb[1] = 0.26f; rgb[2] = 0.26f;
            break;
        default:
            rgb[0] = 0.30f; rgb[1] = 0.30f; rgb[2] = 0.32f;
            break;
    }
}

void renderer_draw_mobs(BgRenderer *renderer, const BgCamera *camera,
                        const BgMobWorld *mobs) {
    if (!renderer->mob_vao || !renderer->program || !mobs) {
        return;
    }

    BgMesh *mesh = &renderer->mob_mesh;
    mesh_reset(mesh);

    for (int i = 0; i < MAX_MOBS; ++i) {
        const BgMob *mob = &mobs->mobs[i];
        if (!mob->alive) {
            continue;
        }
        const MobType type = (MobType)mob->type;
        const float half = mob_half_width(type);
        const float height = mob_height(type);

        BlockId skin = BG_PALE_QUARTZ;
        if (type == MOB_CINDER_HOUND) {
            skin = BG_EMBERSTONE;
        } else if (type == MOB_ASH_WISP) {
            skin = BG_LUMENSTONE;
        } else if (type == MOB_CINDER_ALPHA) {
            skin = BG_CINDERBRICK;
        } else if (type == MOB_CINDER_SOVEREIGN) {
            skin = BG_CINDERSTONE;
        }
        if (mob->hurt_timer > 0.0f) {
            skin = BG_PALE_QUARTZ;
        }

        /* Draw between the last two fixed steps so a fast display stays smooth. */
        const float mx = bg_lerp(mob->prev_x, mob->x, camera->alpha);
        const float my = bg_lerp(mob->prev_y, mob->y, camera->alpha);
        const float mz = bg_lerp(mob->prev_z, mob->z, camera->alpha);

        const float body_half[3] = { half, height * 0.31f, half * 0.78f };
        const float body_center[3] = { mx, my + height * 0.46f, mz };
        mesh_box(mesh, body_center, body_half, skin);

        /* The Sovereign roots itself before a slam. A ring of magma lights up
         * around the blast radius, filling one segment at a time, so the
         * player can read both where the danger ends and how long is left. */
        if (type == MOB_CINDER_SOVEREIGN && mob->windup > 0.0f) {
            const int segments = bg_charge_segments(mob->windup, 0.8f, 48);
            const float radius = 13.0f;
            for (int s = 0; s < segments; ++s) {
                const float angle = (float)s * (6.2831853f / 48.0f);
                const float seg_half[3] = { 0.9f, 0.02f, 0.9f };
                const float seg_center[3] = {
                    mx + cosf(angle) * radius,
                    my + 0.05f,
                    mz + sinf(angle) * radius
                };
                mesh_box(mesh, seg_center, seg_half, BG_MAGMA);
            }
        }

        const float head_half[3] = { half * 0.52f, height * 0.18f, half * 0.52f };
        const float forward = half * 0.78f;
        const float head_center[3] = {
            mx + cosf(mob->yaw) * forward,
            my + height * 0.86f,
            mz + sinf(mob->yaw) * forward
        };
        mesh_box(mesh, head_center, head_half,
                 (type == MOB_ASH_WISP || type == MOB_CINDER_SOVEREIGN) ? BG_MAGMA : skin);
    }

    if (mesh->index_count == 0) {
        return;
    }

    float view[16];
    float projection[16];
    camera_view_matrix(camera, view);
    camera_projection_matrix(camera, projection);

    const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    glUseProgram(renderer->program);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "model"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, GL_FALSE,
                       projection);
    glUniform3f(glGetUniformLocation(renderer->program, "viewPos"), camera->x, camera->y,
                camera->z);
    glUniform3fv(glGetUniformLocation(renderer->program, "ambientColor"), 1, renderer->ambient);
    glUniform3fv(glGetUniformLocation(renderer->program, "lightColor"), 1, renderer->light_color);
    glUniform3fv(glGetUniformLocation(renderer->program, "fogColor"), 1, renderer->fog_color);
    glUniform1f(glGetUniformLocation(renderer->program, "fogStart"), renderer->fog_start);
    glUniform1f(glGetUniformLocation(renderer->program, "fogDensity"), renderer->fog_density);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->atlas);
    glUniform1i(glGetUniformLocation(renderer->program, "texAtlas"), 0);

    glBindVertexArray(renderer->mob_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->mob_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->vertex_count * sizeof(BgVertex)), mesh->vertices,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->mob_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)), mesh->indices,
                 GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->index_count, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);
    renderer->draw_calls++;
}

void renderer_draw_drops(BgRenderer *renderer, const BgCamera *camera, const BgDrop *drops,
                         int drop_count) {
    if (!renderer->mob_vao || !renderer->program || !drops) {
        return;
    }

    BgMesh *mesh = &renderer->mob_mesh;
    mesh_reset(mesh);
    int drawn = 0;
    for (int i = 0; i < MAX_DROPS && i < drop_count; ++i) {
        const BgDrop *drop = &drops[i];
        if (!drop->alive) {
            continue;
        }
        BlockId skin = BG_PALE_QUARTZ;
        if (drop->item < BG_BLOCK_COUNT) {
            skin = (BlockId)drop->item;
        } else if (drop->item == BG_ITEM_EMBER_SHARD || drop->item == BG_ITEM_EMBERHEART) {
            skin = BG_LUMENSTONE;
        }
        const float bob = sinf(drop->age * 2.6f) * 0.06f;
        const float half[3] = { 0.13f, 0.13f, 0.13f };
        const float center[3] = {
            bg_lerp(drop->prev_x, drop->x, camera->alpha),
            bg_lerp(drop->prev_y, drop->y, camera->alpha) + 0.2f + bob,
            bg_lerp(drop->prev_z, drop->z, camera->alpha)
        };
        mesh_box(mesh, center, half, skin);
        drawn++;
    }
    if (drawn == 0 || mesh->index_count == 0) {
        return;
    }

    float view[16];
    float projection[16];
    camera_view_matrix(camera, view);
    camera_projection_matrix(camera, projection);
    const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    glUseProgram(renderer->program);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "model"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, GL_FALSE,
                       projection);
    glUniform3f(glGetUniformLocation(renderer->program, "viewPos"), camera->x, camera->y,
                camera->z);
    glUniform3fv(glGetUniformLocation(renderer->program, "ambientColor"), 1, renderer->ambient);
    glUniform3fv(glGetUniformLocation(renderer->program, "lightColor"), 1, renderer->light_color);
    glUniform3fv(glGetUniformLocation(renderer->program, "fogColor"), 1, renderer->fog_color);
    glUniform1f(glGetUniformLocation(renderer->program, "fogStart"), renderer->fog_start);
    glUniform1f(glGetUniformLocation(renderer->program, "fogDensity"), renderer->fog_density);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->atlas);
    glUniform1i(glGetUniformLocation(renderer->program, "texAtlas"), 0);

    glBindVertexArray(renderer->mob_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->mob_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->vertex_count * sizeof(BgVertex)), mesh->vertices,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->mob_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)), mesh->indices,
                 GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->index_count, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);
    renderer->draw_calls++;
}

/* The atlas tile a hotbar slot shows: the block's top face, or an item sprite. */
static int item_icon_tile(uint16_t item) {
    if (item < BG_BLOCK_COUNT) {
        return block_tile((BlockId)item, 2);
    }
    if (item >= BG_ITEM_STICK && item < BG_ITEM_COUNT) {
        return ITEM_TILE_BASE + (int)(item - BG_ITEM_STICK);
    }
    return -1;
}

/* Samples the atlas (8x8 cells) so the hotbar shows the same pixels the world
 * does, with the transparent corners of item sprites left alone. */
void ui_draw_item_icon(BgUi *ui, float x, float y, float size, uint16_t item,
                       float alpha) {
    const int tile = item_icon_tile(item);
    if (tile < 0) {
        float rgb[3] = { 0.30f, 0.30f, 0.32f };
        item_color(item, rgb);
        const float flat[4] = { rgb[0], rgb[1], rgb[2], alpha };
        ui_rect(ui, x, y, size, size, flat);
        return;
    }

    const int origin_x = (tile % ATLAS_COLS) * ATLAS_TILE;
    const int origin_y = (tile / ATLAS_COLS) * ATLAS_TILE;
    const int cells = 8;
    const int cell = ATLAS_TILE / cells;
    const float step = size / (float)cells;
    for (int iy = 0; iy < cells; ++iy) {
        for (int ix = 0; ix < cells; ++ix) {
            int sum[4] = { 0, 0, 0, 0 };
            for (int py = 0; py < cell; ++py) {
                for (int px = 0; px < cell; ++px) {
                    const unsigned char *src =
                        &g_atlas_pixels[((origin_y + iy * cell + py) * ATLAS_WIDTH +
                                         origin_x + ix * cell + px) * 4];
                    sum[0] += src[0];
                    sum[1] += src[1];
                    sum[2] += src[2];
                    sum[3] += src[3];
                }
            }
            const float cover = (float)sum[3] / (float)(cell * cell * 255);
            if (cover < 0.4f) {
                continue;
            }
            const float weight = (float)sum[3] / 255.0f;
            const float color[4] = { (float)sum[0] / weight / 255.0f,
                                     (float)sum[1] / weight / 255.0f,
                                     (float)sum[2] / weight / 255.0f,
                                     alpha * cover };
            ui_rect(ui, x + (float)ix * step, y + (float)iy * step, step + 0.5f,
                    step + 0.5f, color);
        }
    }
}

static const char *const kHeartArt[7] = {
    ".11.11.",
    "1111111",
    "1111111",
    ".11111.",
    "..111..",
    "...1...",
    "......."
};

static void draw_heart(BgUi *ui, float x, float y, float pixel, bool full) {
    const float shadow[4] = { 0.10f, 0.02f, 0.02f, 0.95f };
    const float color[4] = { full ? 0.90f : 0.26f, full ? 0.16f : 0.12f,
                             full ? 0.18f : 0.14f, 1.0f };
    for (int pass = 0; pass < 2; ++pass) {
        const float offset = pass == 0 ? pixel : 0.0f;
        const float *tint = pass == 0 ? shadow : color;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 7; ++col) {
                if (kHeartArt[row][col] != '1') {
                    continue;
                }
                ui_rect(ui, x + (float)col * pixel + offset,
                        y + (float)row * pixel + offset, pixel, pixel, tint);
            }
        }
    }
}

void ui_draw_hearts(BgUi *ui, float x, float y, float pixel, int health, int hearts) {
    for (int i = 0; i < hearts; ++i) {
        draw_heart(ui, x + (float)i * pixel * 10.0f, y, pixel, health > i * 2);
    }
}

void renderer_draw_hud(BgRenderer *renderer, const BgPlayer *player) {
    int width = 1280;
    int height = 720;
    renderer_get_size(renderer, &width, &height);
    const float w = (float)width;
    const float h = (float)height;

    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float cross_shadow[4] = { 0.05f, 0.05f, 0.06f, 0.55f };
    ui_rect(&ui, w * 0.5f - 11.0f, h * 0.5f, 22.0f, 2.0f, cross_shadow);
    ui_rect(&ui, w * 0.5f, h * 0.5f - 11.0f, 2.0f, 22.0f, cross_shadow);
    const float white[4] = { 1.0f, 1.0f, 1.0f, 0.92f };
    ui_rect(&ui, w * 0.5f - 10.0f, h * 0.5f - 1.0f, 20.0f, 2.0f, white);
    ui_rect(&ui, w * 0.5f - 1.0f, h * 0.5f - 10.0f, 2.0f, 20.0f, white);

    const float slot = 42.0f;
    const float gap = 2.0f;
    const float total = 9.0f * slot + 8.0f * gap;
    const float bar_x = (w - total) * 0.5f;
    const float bar_y = h - slot - 18.0f;

    const float edge[4] = { 0.04f, 0.04f, 0.05f, 0.90f };
    ui_rect(&ui, bar_x - 8.0f, bar_y - 8.0f, total + 16.0f, slot + 16.0f, edge);
    const float panel[4] = { 0.30f, 0.30f, 0.31f, 0.94f };
    ui_rect(&ui, bar_x - 6.0f, bar_y - 6.0f, total + 12.0f, slot + 12.0f, panel);
    const float inner[4] = { 0.09f, 0.09f, 0.10f, 0.96f };
    ui_rect(&ui, bar_x - 4.0f, bar_y - 4.0f, total + 8.0f, slot + 8.0f, inner);

    for (int i = 0; i < 9; ++i) {
        const float x = bar_x + (float)i * (slot + gap);
        const bool selected = (i == player->selected);
        const float cell_color[4] = { 0.14f, 0.14f, 0.15f, 0.95f };
        ui_rect(&ui, x, bar_y, slot, slot, cell_color);

        const BgStack *stack = &player->inventory[i];
        if (stack->count > 0) {
            ui_draw_item_icon(&ui, x + 4.0f, bar_y + 4.0f, slot - 8.0f, stack->item, 0.98f);
        }
        if (selected) {
            const float frame[4] = { 0.98f, 0.98f, 0.94f, 0.95f };
            ui_rect(&ui, x - 2.0f, bar_y - 2.0f, slot + 4.0f, 2.0f, frame);
            ui_rect(&ui, x - 2.0f, bar_y + slot, slot + 4.0f, 2.0f, frame);
            ui_rect(&ui, x - 2.0f, bar_y - 2.0f, 2.0f, slot + 4.0f, frame);
            ui_rect(&ui, x + slot, bar_y - 2.0f, 2.0f, slot + 4.0f, frame);
        }
        if (stack->count > 1) {
            char text[8];
            snprintf(text, sizeof(text), "%u", (unsigned)stack->count);
            const float shadow[4] = { 0.05f, 0.05f, 0.06f, 0.90f };
            const float count_color[4] = { 1.0f, 1.0f, 1.0f, 0.98f };
            ui_text(&ui, x + slot - 1.0f - ui_text_width(text, 1.6f),
                    bar_y + slot - 11.0f, 1.6f, text, shadow);
            ui_text(&ui, x + slot - 2.0f - ui_text_width(text, 1.6f),
                    bar_y + slot - 12.0f, 1.6f, text, count_color);
        }
        const int wear = item_durability_max(stack->item);
        if (wear > 0 && stack->damage > 0) {
            const float left = 1.0f - (float)stack->damage / (float)wear;
            const float bar_w = (slot - 8.0f) * clamp01(left);
            const float wear_color[4] = { clamp01(1.4f - left * 1.4f),
                                          clamp01(0.5f + left * 0.9f), 0.18f, 0.98f };
            ui_rect(&ui, x + 4.0f, bar_y + slot - 6.0f, slot - 8.0f, 2.0f,
                    (float[4]){ 0.05f, 0.05f, 0.06f, 0.9f });
            ui_rect(&ui, x + 4.0f, bar_y + slot - 6.0f, bar_w, 2.0f, wear_color);
        }
    }

    ui_draw_hearts(&ui, bar_x, bar_y - 30.0f, 2.0f, player->health, 10);

    const BgStack *held = &player->inventory[player->selected];
    if (held->count > 0) {
        const float name_color[4] = { 0.96f, 0.96f, 0.92f, 0.92f };
        const float name_shadow[4] = { 0.06f, 0.06f, 0.07f, 0.85f };
        ui_text_centered(&ui, w * 0.5f + 1.5f, bar_y - 54.5f, 1.8f, item_name(held->item),
                         name_shadow);
        ui_text_centered(&ui, w * 0.5f, bar_y - 56.0f, 1.8f, item_name(held->item),
                         name_color);

        /* Last quarter of a tool's life: say how many blocks are left before
         * it snaps, so a pick never breaks mid-wall without warning. */
        const int maximum = item_durability_max(held->item);
        const int left = maximum - (int)held->damage;
        if (maximum > 0 && held->damage > 0 && left * 4 <= maximum) {
            char warn[96];
            snprintf(warn, sizeof(warn), "%s wearing thin - %d left",
                     item_name(held->item), left > 0 ? left : 0);
            const float warn_shadow[4] = { 0.12f, 0.03f, 0.02f, 0.92f };
            const float warn_color[4] = { 1.0f, 0.44f, 0.24f, 0.98f };
            ui_text_centered(&ui, w * 0.5f + 1.5f, bar_y - 74.5f, 1.7f, warn, warn_shadow);
            ui_text_centered(&ui, w * 0.5f, bar_y - 76.0f, 1.7f, warn, warn_color);
        }
    }

    renderer_draw_ui(renderer, &ui);
    ui_free(&ui);
}
