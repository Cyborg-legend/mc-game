#ifndef BLOCKFORGE_C_RENDER_H
#define BLOCKFORGE_C_RENDER_H

#include "bg.h"

#include <GL/glew.h>

typedef struct BgCamera {
    float x, y, z;
    float yaw, pitch;
    float fov;
    float aspect;
    float near_plane, far_plane;
} BgCamera;

typedef struct BgChunkDraw {
    uint64_t key;
    GLuint vao, vbo, ibo;
    GLsizei index_count;
} BgChunkDraw;

typedef struct BgRenderer {
    GLuint program, vao, vbo, ibo, atlas;
    GLuint ui_program, ui_vao, ui_vbo;
    GLuint mob_vao, mob_vbo, mob_ibo;
    BgMesh mob_mesh;
    BgChunkDraw *chunks;
    size_t chunk_count, chunk_capacity;
    float sky[3], fog_color[3], ambient[3], light_color[3];
    float fog_start, fog_density;
    int draw_calls;
    int width, height;
} BgRenderer;

/* UI batch: six floats per vertex (x, y, r, g, b, a). */
typedef struct BgUi {
    float *verts;
    size_t floats;
    size_t capacity;
} BgUi;

bool renderer_init(BgRenderer *renderer);
void renderer_shutdown(BgRenderer *renderer);
void renderer_set_environment(BgRenderer *renderer, Dimension dimension);
void renderer_upload_chunk(BgRenderer *renderer, uint64_t key, const BgMesh *mesh);
void renderer_clear_chunks(BgRenderer *renderer);
void renderer_draw(BgRenderer *renderer, const BgCamera *camera);
void renderer_draw_mobs(BgRenderer *renderer, const BgCamera *camera, const BgMobWorld *mobs);
void renderer_draw_hud(BgRenderer *renderer, const BgPlayer *player);
void renderer_get_size(BgRenderer *renderer, int *width, int *height);

void ui_clear(BgUi *ui);
void ui_free(BgUi *ui);
void ui_rect(BgUi *ui, float x, float y, float w, float h, const float color[4]);
void ui_tile_texture(BgUi *ui, float x, float y, float width, float height, int tile,
                     float cell, float shade, float alpha);
void ui_text(BgUi *ui, float x, float y, float scale, const char *text, const float color[4]);
void ui_text_centered(BgUi *ui, float center_x, float y, float scale, const char *text,
                      const float color[4]);
float ui_text_width(const char *text, float scale);
void renderer_draw_ui(BgRenderer *renderer, const BgUi *ui);

/* Draws a block face or item sprite sampled from the atlas, skipping the
 * transparent cells of item sprites. */
void ui_draw_item_icon(BgUi *ui, float x, float y, float size, uint16_t item, float alpha);

/* Hearts in a row: filled ones track the player's health out of 20. */
void ui_draw_hearts(BgUi *ui, float x, float y, float pixel, int health, int hearts);

/* Representative colour of a block or item, for the hotbar icons. */
void block_color(BlockId block, float rgb[3]);
void item_color(uint16_t item, float rgb[3]);

#endif /* BLOCKFORGE_C_RENDER_H */
