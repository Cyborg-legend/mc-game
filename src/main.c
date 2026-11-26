#include "bg.h"
#include "render.h"

#include <GLFW/glfw3.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_PATH "blockforge.sav"
#define RENDER_RADIUS 5
#define MAX_TRACKED_COLUMNS 2048
#define EYE_HEIGHT 1.62f
#define MOUSE_SENSITIVITY 0.0022f
#define FIELD_OF_VIEW 1.2217305f

typedef enum {
    STATE_TITLE,
    STATE_CREATE,
    STATE_LOADING,
    STATE_PLAYING,
    STATE_PAUSED
} AppState;

typedef struct App {
    World world;
    BgPlayer player;
    BgRenderer renderer;
    BgInput input;
    bool prev_keys[INPUT_KEY_COUNT];
    AppState state;
    int menu_choice;
    int pause_choice;
    int create_choice;
    char seed_text[20];
    Dimension create_dimension;
    bool loading;
    Dimension loading_dimension;
    int loading_center_x, loading_center_z;
    int loading_radius;
    int loading_cursor, loading_total;
    uint64_t tracked[MAX_TRACKED_COLUMNS];
    size_t tracked_count;
    int last_cx, last_cz;
    bool has_save;
    bool travel_armed;
    BgMobWorld mobs;
    bool show_crafting;
    int craft_choice;
    char toast[96];
    float toast_timer;
    int width, height;
    float time;
} App;

static App g_app;

static void stream_around_player(App *app);

/* ------------------------------------------------------------- callbacks */

static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (key < 0 || key >= INPUT_KEY_COUNT) {
        return;
    }
    if (action == GLFW_PRESS) {
        g_app.input.keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        g_app.input.keys[key] = false;
    }
}

static void on_char(GLFWwindow *window, unsigned int codepoint) {
    (void)window;
    if (g_app.state != STATE_CREATE || g_app.create_choice != 0) {
        return;
    }
    if (codepoint < '0' || codepoint > '9') {
        return;
    }
    const size_t length = strlen(g_app.seed_text);
    if (length < sizeof(g_app.seed_text) - 1) {
        g_app.seed_text[length] = (char)codepoint;
        g_app.seed_text[length + 1] = '\0';
    }
}

static void on_mouse_button(GLFWwindow *window, int button, int action, int mods) {
    (void)window;
    (void)mods;
    if (button < 0 || button >= 5) {
        return;
    }
    g_app.input.mouse[button] = (action == GLFW_PRESS);
}

static void on_cursor(GLFWwindow *window, double x, double y) {
    (void)window;
    static double last_x = 0.0;
    static double last_y = 0.0;
    static bool initialised = false;
    if (!initialised) {
        last_x = x;
        last_y = y;
        initialised = true;
        return;
    }
    if (g_app.state == STATE_PLAYING) {
        g_app.input.mouse_dx = (int16_t)(x - last_x);
        g_app.input.mouse_dy = (int16_t)(y - last_y);
    }
    last_x = x;
    last_y = y;
}

static void on_framebuffer_size(GLFWwindow *window, int width, int height) {
    (void)window;
    g_app.width = width;
    g_app.height = height;
    glViewport(0, 0, width > 0 ? width : 1, height > 0 ? height : 1);
}

/* --------------------------------------------------------------- helpers */

static bool key_pressed(const App *app, int key) {
    if (key < 0 || key >= INPUT_KEY_COUNT) {
        return false;
    }
    return app->input.keys[key] && !app->prev_keys[key];
}

static uint64_t column_key(int cx, int cz) {
    return ((uint64_t)(uint32_t)cx << 32) | (uint64_t)(uint32_t)cz;
}

static bool column_tracked(const App *app, uint64_t key) {
    for (size_t i = 0; i < app->tracked_count; ++i) {
        if (app->tracked[i] == key) {
            return true;
        }
    }
    return false;
}

static void track_column(App *app, uint64_t key) {
    if (app->tracked_count < MAX_TRACKED_COLUMNS) {
        app->tracked[app->tracked_count++] = key;
    }
}

static void set_toast(App *app, const char *text) {
    snprintf(app->toast, sizeof(app->toast), "%s", text);
    app->toast_timer = 2.6f;
}

static void reset_streaming(App *app) {
    app->tracked_count = 0;
    renderer_clear_chunks(&app->renderer);
    app->last_cx = 0x7FFFFFF;
    app->last_cz = 0x7FFFFFF;
}

/* Stands the player just outside a rift's opening, facing the frame. */
static void place_at_rift(App *app, int cx, int cy, int cz, int axis) {
    app->player.vx = app->player.vy = app->player.vz = 0.0f;
    if (axis == 0) {
        app->player.x = (float)cx + 1.5f;
        app->player.z = (float)cz - 2.5f;
        app->player.yaw = 1.5707963f;
    } else {
        app->player.x = (float)cx - 2.5f;
        app->player.z = (float)cz + 1.5f;
        app->player.yaw = 0.0f;
    }
    app->player.y = (float)cy;
    app->player.pitch = 0.0f;
}

static void respawn(App *app) {
    app->player.health = 20;
    app->player.invuln = 1.5f;
    app->player.vx = app->player.vy = app->player.vz = 0.0f;
    if (app->world.dimension == BG_UNDERWORLD) {
        /* There is no camp in the rift realm, so the player comes to on the spot
         * where they fell instead of being pushed inside solid rock. */
        set_toast(app, "You come to where you fell.");
        mobs_clear(&app->mobs);
        return;
    }
    int spawn_x = 0;
    int spawn_z = 0;
    world_spawn_site(&app->world, &spawn_x, &spawn_z);
    if (!world_chunk(&app->world, spawn_x / CHUNK_SIZE, 0, spawn_z / CHUNK_SIZE, false)) {
        world_generate_around(&app->world, spawn_x / CHUNK_SIZE, spawn_z / CHUNK_SIZE, 0);
    }
    app->player.x = (float)spawn_x + 0.5f;
    app->player.z = (float)spawn_z + 0.5f;
    app->player.y = (float)world_surface_y(&app->world, spawn_x, spawn_z);
    if (app->player.y < 2.0f) {
        app->player.y = 40.0f;
    }
    app->player.yaw = 1.5707963f;
    app->player.pitch = 0.0f;
    set_toast(app, "You wake at the spawn.");
    mobs_clear(&app->mobs);
    reset_streaming(app);
    stream_around_player(app);
}

static void set_cursor_captured(GLFWwindow *window, bool captured) {
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    g_app.input.mouse_dx = 0;
    g_app.input.mouse_dy = 0;
}

static void ensure_column(App *app, int cx, int cz) {
    if (!world_chunk(&app->world, cx, 0, cz, false)) {
        world_generate_around(&app->world, cx, cz, 0);
    }
}

static void mesh_column(App *app, int cx, int cz) {
    const int rows = (app->world.dimension == BG_UPPERWORLD ? UPPERWORLD_TOP : UNDERWORLD_TOP)
                     / CHUNK_SIZE;
    for (int cy = 0; cy < rows; ++cy) {
        Chunk *chunk = world_chunk(&app->world, cx, cy, cz, false);
        if (!chunk) {
            continue;
        }
        BgMesh mesh;
        mesh_chunk(&app->world, chunk, &mesh);
        renderer_upload_chunk(&app->renderer, chunk_key_of(cx, cy, cz), &mesh);
        mesh_free(&mesh);
    }
}

static void stream_around_player(App *app) {
    const int cx = (int)floorf(app->player.x) >> 4;
    const int cz = (int)floorf(app->player.z) >> 4;
    if (app->tracked_count > 0 && cx == app->last_cx && cz == app->last_cz) {
        return;
    }
    app->last_cx = cx;
    app->last_cz = cz;

    if (app->tracked_count + 256 > MAX_TRACKED_COLUMNS) {
        reset_streaming(app);
        app->last_cx = cx;
        app->last_cz = cz;
    }

    for (int dz = -RENDER_RADIUS - 1; dz <= RENDER_RADIUS + 1; ++dz) {
        for (int dx = -RENDER_RADIUS - 1; dx <= RENDER_RADIUS + 1; ++dx) {
            ensure_column(app, cx + dx, cz + dz);
        }
    }

    for (int dz = -RENDER_RADIUS; dz <= RENDER_RADIUS; ++dz) {
        for (int dx = -RENDER_RADIUS; dx <= RENDER_RADIUS; ++dx) {
            const uint64_t key = column_key(cx + dx, cz + dz);
            if (column_tracked(app, key)) {
                continue;
            }
            track_column(app, key);
            mesh_column(app, cx + dx, cz + dz);
        }
    }
}

static void remesh_near(App *app, int x, int y, int z) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int by = y + dy;
                if (by < 0 || by >= UNDERWORLD_TOP) {
                    continue;
                }
                const int cx = (x + dx) >> 4;
                const int cy = by >> 4;
                const int cz = (z + dz) >> 4;
                Chunk *chunk = world_chunk(&app->world, cx, cy, cz, false);
                if (!chunk) {
                    continue;
                }
                BgMesh mesh;
                mesh_chunk(&app->world, chunk, &mesh);
                renderer_upload_chunk(&app->renderer, chunk_key_of(cx, cy, cz), &mesh);
                mesh_free(&mesh);
            }
        }
    }
}

/* Starts a brand-new world but leaves the generation to update_loading, which
 * spreads it over frames behind a loading screen. */
static void begin_loading(App *app, Dimension dimension) {
    world_clear(&app->world);
    world_set_dimension(&app->world, dimension);
    renderer_set_environment(&app->renderer, dimension);
    player_init(&app->player);
    reset_streaming(app);

    if (dimension == BG_UNDERWORLD) {
        app->loading_center_x = 0;
        app->loading_center_z = 0;
    } else {
        world_spawn_site(&app->world, &app->loading_center_x, &app->loading_center_z);
    }
    app->loading_dimension = dimension;
    app->loading_radius = RENDER_RADIUS + 1;
    const int span = app->loading_radius * 2 + 1;
    app->loading_total = span * span;
    app->loading_cursor = 0;
    app->loading = true;
    app->state = STATE_LOADING;
}

static void update_loading(App *app, GLFWwindow *window) {
    if (!app->loading) {
        return;
    }

    const int span = app->loading_radius * 2 + 1;
    const int cx0 = app->loading_center_x / CHUNK_SIZE - app->loading_radius;
    const int cz0 = app->loading_center_z / CHUNK_SIZE - app->loading_radius;
    int budget = 4;
    while (budget-- > 0 && app->loading_cursor < app->loading_total) {
        const int cx = cx0 + app->loading_cursor % span;
        const int cz = cz0 + app->loading_cursor / span;
        world_generate_around(&app->world, cx, cz, 0);
        app->loading_cursor++;
    }
    if (app->loading_cursor < app->loading_total) {
        return;
    }

    app->loading = false;
    if (app->loading_dimension == BG_UNDERWORLD) {
        /* Arriving in the rift realm means the twin gate came along; it is also
         * the only way back out of a world that starts here. */
        int surface = world_surface_y(&app->world, 0, 0);
        if (surface < 2) {
            surface = 60;
        }
        rift_build(&app->world, 0, surface, 0, 0);
        place_at_rift(app, 0, surface, 0, 0);
    } else {
        app->player.x = (float)app->loading_center_x + 0.5f;
        app->player.z = (float)app->loading_center_z + 0.5f;
        app->player.y = (float)world_surface_y(&app->world, app->loading_center_x,
                                               app->loading_center_z);
        if (app->player.y < 2.0f) {
            app->player.y = 40.0f;
        }
        app->player.yaw = 1.5707963f;
        app->player.pitch = 0.0f;
    }
    app->travel_armed = false;
    mobs_clear(&app->mobs);

    stream_around_player(app);
    app->state = STATE_PLAYING;
    set_cursor_captured(window, true);
}

static void start_new_world(App *app, GLFWwindow *window) {
    (void)window;
    const uint64_t seed = (uint64_t)strtoull(app->seed_text, NULL, 10);
    world_set_seed(&app->world, seed ? seed : 1337u);
    begin_loading(app, app->create_dimension);
}

static bool load_game(App *app, GLFWwindow *window) {
    if (!load_world(SAVE_PATH, &app->world, &app->player)) {
        return false;
    }
    renderer_set_environment(&app->renderer, app->world.dimension);
    reset_streaming(app);
    stream_around_player(app);
    app->travel_armed = false;
    mobs_clear(&app->mobs);
    app->state = STATE_PLAYING;
    set_cursor_captured(window, true);
    return true;
}

static void travel(App *app) {
    const Dimension next = app->world.dimension == BG_UPPERWORLD ? BG_UNDERWORLD : BG_UPPERWORLD;
    const int px = (int)floorf(app->player.x);
    const int py = (int)floorf(app->player.y);
    const int pz = (int)floorf(app->player.z);

    /* The frame is wherever the player is standing in the portal; rebuilding the
     * twin from it keeps both sides of a hand-built rift in the same place. */
    int cx = 0;
    int cy = 0;
    int cz = 0;
    int axis = 0;
    if (!rift_frame_at(&app->world, px, py, pz, &cx, &cy, &cz, &axis) &&
        !rift_frame_at(&app->world, px, py + 1, pz, &cx, &cy, &cz, &axis)) {
        return;
    }

    world_clear(&app->world);
    world_set_dimension(&app->world, next);
    renderer_set_environment(&app->renderer, next);
    reset_streaming(app);

    world_generate_around(&app->world, cx / CHUNK_SIZE, cz / CHUNK_SIZE, RENDER_RADIUS + 1);
    rift_build(&app->world, cx, cy, cz, axis);

    place_at_rift(app, cx, cy, cz, axis);
    app->travel_armed = false;
    mobs_clear(&app->mobs);

    stream_around_player(app);
    set_toast(app, next == BG_UNDERWORLD ? "The Underworld swallows you."
                                        : "You step back into the open.");
}

static void save_and_title(App *app, GLFWwindow *window) {
    save_world(SAVE_PATH, &app->world, &app->player);
    app->has_save = true;
    app->state = STATE_TITLE;
    app->menu_choice = 0;
    set_cursor_captured(window, false);
}

/* ----------------------------------------------------------------- menus */

/* The dirt backdrop the classic menus always use, tinted down a little. */
static void menu_background(BgUi *ui, float width, float height) {
    ui_tile_texture(ui, 0.0f, 0.0f, width, height, 2, 64.0f, 0.74f, 1.0f);
    const float dim[4] = { 0.02f, 0.02f, 0.03f, 0.30f };
    ui_rect(ui, 0.0f, 0.0f, width, height, dim);
}

static void draw_shadowed_centered(BgUi *ui, float cx, float y, float scale, const char *text,
                                   const float color[4]) {
    const float shadow[4] = { 0.05f, 0.05f, 0.06f, 0.92f };
    ui_text_centered(ui, cx + 2.0f, y + 2.0f, scale, text, shadow);
    ui_text_centered(ui, cx, y, scale, text, color);
}

static void draw_logo(BgUi *ui, float cx, float y, float scale) {
    const float shadow[4] = { 0.09f, 0.09f, 0.11f, 1.0f };
    const float bevel[4] = { 0.30f, 0.26f, 0.20f, 1.0f };
    const float face[4] = { 0.95f, 0.93f, 0.88f, 1.0f };
    ui_text_centered(ui, cx + 6.0f, y + 6.0f, scale, "BLOCKFORGE", shadow);
    ui_text_centered(ui, cx + 3.0f, y + 3.0f, scale, "BLOCKFORGE", bevel);
    ui_text_centered(ui, cx, y, scale, "BLOCKFORGE", face);
}

/* Stone-grey button with a bevel, a black outline and a shadowed label. */
static void draw_button(BgUi *ui, float x, float y, float w, float h, float scale,
                        const char *label, bool selected, bool enabled) {
    const float border[4] = { 0.05f, 0.05f, 0.06f, 0.95f };
    ui_rect(ui, x - 3.0f, y - 3.0f, w + 6.0f, h + 6.0f, border);

    const float fill[4] = {
        enabled ? (selected ? 0.48f : 0.33f) : 0.22f,
        enabled ? (selected ? 0.49f : 0.34f) : 0.22f,
        enabled ? (selected ? 0.52f : 0.37f) : 0.24f,
        1.0f
    };
    ui_rect(ui, x, y, w, h, fill);

    const float light[4] = { 0.66f, 0.67f, 0.70f, selected ? 1.0f : 0.60f };
    ui_rect(ui, x, y, w, 3.0f, light);
    ui_rect(ui, x, y, 3.0f, h, light);
    const float dark[4] = { 0.11f, 0.11f, 0.13f, 0.92f };
    ui_rect(ui, x, y + h - 3.0f, w, 3.0f, dark);
    ui_rect(ui, x + w - 3.0f, y, 3.0f, h, dark);

    const float text_y = y + (h - 7.0f * scale) * 0.5f;
    const float text_shadow[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
    ui_text_centered(ui, x + w * 0.5f + 2.0f, text_y + 2.0f, scale, label, text_shadow);
    const float color[4] = {
        enabled ? (selected ? 1.0f : 0.90f) : 0.42f,
        enabled ? (selected ? 0.95f : 0.91f) : 0.43f,
        enabled ? (selected ? 0.60f : 0.92f) : 0.47f,
        1.0f
    };
    ui_text_centered(ui, x + w * 0.5f, text_y, scale, label, color);
}

/* Sunken black field with a light border, as used for text input. */
static void draw_field(BgUi *ui, float x, float y, float w, float h, float scale,
                       const char *label, bool selected) {
    const float back[4] = { 0.0f, 0.0f, 0.0f, 0.78f };
    ui_rect(ui, x, y, w, h, back);
    const float edge[4] = { selected ? 0.88f : 0.36f, selected ? 0.89f : 0.37f,
                            selected ? 0.86f : 0.40f, 1.0f };
    ui_rect(ui, x, y, w, 2.0f, edge);
    ui_rect(ui, x, y + h - 2.0f, w, 2.0f, edge);
    ui_rect(ui, x, y, 2.0f, h, edge);
    ui_rect(ui, x + w - 2.0f, y, 2.0f, h, edge);

    const float text_y = y + (h - 7.0f * scale) * 0.5f;
    const float shadow[4] = { 0.05f, 0.05f, 0.06f, 1.0f };
    ui_text(ui, x + 16.0f, text_y + 2.0f, scale, label, shadow);
    const float color[4] = { 0.93f, 0.93f, 0.90f, 1.0f };
    ui_text(ui, x + 14.0f, text_y, scale, label, color);
}

static void draw_splash(BgUi *ui, float cx, float y, float time) {
    const float bob = sinf(time * 2.8f) * 5.0f;
    const float shadow[4] = { 0.16f, 0.13f, 0.02f, 0.95f };
    const float color[4] = { 1.0f, 0.98f, 0.22f, 1.0f };
    ui_text_centered(ui, cx + 2.0f, y + bob + 2.0f, 2.0f, "Now with rifts!", shadow);
    ui_text_centered(ui, cx, y + bob, 2.0f, "Now with rifts!", color);
}

static void render_title(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    menu_background(&ui, width, height);

    draw_logo(&ui, width * 0.5f, height * 0.10f, 8.0f);
    const float soft[4] = { 0.88f, 0.90f, 0.94f, 0.92f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.255f, 2.0f,
                           "TWO REALMS. ONE RIFT.", soft);
    draw_splash(&ui, width * 0.5f + 300.0f, height * 0.185f, app->time);

    const float scale = 2.4f;
    const float bw = 420.0f;
    const float bh = 46.0f;
    const float start_y = height * 0.44f;
    const char *labels[3] = { "CONTINUE", "NEW WORLD", "QUIT GAME" };
    for (int i = 0; i < 3; ++i) {
        const bool enabled = !(i == 0 && !app->has_save);
        draw_button(&ui, width * 0.5f - bw * 0.5f, start_y + (float)i * (bh + 12.0f), bw, bh,
                    scale, labels[i], app->menu_choice == i && enabled, enabled);
    }

    ui_text_centered(&ui, width * 0.5f, height - 40.0f, 1.5f,
                     "WASD MOVE   SPACE JUMP   LMB MINE/ATTACK   RMB BUILD/LIGHT   C CRAFT   F EAT",
                     soft);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_create(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    menu_background(&ui, width, height);

    draw_logo(&ui, width * 0.5f, height * 0.06f, 5.0f);
    const float title[4] = { 0.98f, 0.90f, 0.62f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.19f, 3.0f, "NEW WORLD", title);

    const float field_w = 560.0f;
    const float field_h = 54.0f;
    const float field_x = width * 0.5f - field_w * 0.5f;

    char seed_line[64];
    snprintf(seed_line, sizeof(seed_line), "SEED  %s%s", app->seed_text,
             app->create_choice == 0 ? "_" : "");
    draw_field(&ui, field_x, height * 0.33f, field_w, field_h, 2.2f, seed_line,
               app->create_choice == 0);

    char realm_line[64];
    snprintf(realm_line, sizeof(realm_line), "REALM  %s%s%s",
             app->create_choice == 1 ? "< " : "",
             app->create_dimension == BG_UPPERWORLD ? "UPPERWORLD" : "UNDERWORLD",
             app->create_choice == 1 ? " >" : "");
    draw_field(&ui, field_x, height * 0.45f, field_w, field_h, 2.2f, realm_line,
               app->create_choice == 1);

    draw_button(&ui, width * 0.5f - 200.0f, height * 0.62f, 400.0f, 50.0f, 2.4f,
                "CREATE WORLD", app->create_choice == 2, true);

    const float hint[4] = { 0.82f, 0.84f, 0.88f, 0.85f };
    ui_text_centered(&ui, width * 0.5f, height * 0.78f, 1.6f,
                     "UP/DOWN MOVES   LEFT/RIGHT CHANGES REALM   ENTER CONFIRMS   ESC BACK",
                     hint);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_pause(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    const float dim[4] = { 0.02f, 0.02f, 0.03f, 0.62f };
    ui_rect(&ui, 0.0f, 0.0f, width, height, dim);

    const float title[4] = { 0.98f, 0.92f, 0.72f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.14f, 5.0f, "GAME PAUSED", title);

    const float scale = 2.4f;
    const float bw = 470.0f;
    const float bh = 46.0f;
    const char *labels[3] = { "BACK TO GAME", "SAVE AND QUIT TO TITLE", "QUIT TO DESKTOP" };
    for (int i = 0; i < 3; ++i) {
        draw_button(&ui, width * 0.5f - bw * 0.5f, height * 0.36f + (float)i * (bh + 12.0f),
                    bw, bh, scale, labels[i], i == app->pause_choice, true);
    }

    char where[128];
    snprintf(where, sizeof(where), "%s   X %.1f   Y %.1f   Z %.1f",
             dimension_name(app->world.dimension), app->player.x, app->player.y,
             app->player.z);
    const float soft[4] = { 0.78f, 0.82f, 0.90f, 0.85f };
    ui_text_centered(&ui, width * 0.5f, height - 52.0f, 1.6f, where, soft);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_toast(App *app) {
    if (app->toast_timer <= 0.0f || app->toast[0] == '\0') {
        return;
    }
    BgUi ui;
    memset(&ui, 0, sizeof(ui));
    const float color[4] = { 1.0f, 0.94f, 0.74f, 0.96f };
    const float shadow[4] = { 0.06f, 0.06f, 0.07f, 0.90f };
    ui_text_centered(&ui, (float)app->width * 0.5f + 1.5f, (float)app->height - 118.0f + 1.5f,
                     1.8f, app->toast, shadow);
    ui_text_centered(&ui, (float)app->width * 0.5f, (float)app->height - 118.0f, 1.8f,
                     app->toast, color);
    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void recipe_line(const BgRecipe *recipe, char *out, size_t size) {
    size_t used = (size_t)snprintf(out, size, "%s X%d  <-", item_name(recipe->output.item),
                                   (int)recipe->output.count);
    for (int i = 0; i < recipe->input_count && used < size; ++i) {
        used += (size_t)snprintf(out + used, size - used, " %d %s",
                                 (int)recipe->inputs[i].count,
                                 item_name(recipe->inputs[i].item));
    }
    if (recipe->needs_table && used + 8 < size) {
        snprintf(out + used, size - used, "  [TABLE]");
    }
}

static void render_crafting(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    const float dim[4] = { 0.02f, 0.02f, 0.03f, 0.74f };
    ui_rect(&ui, 0.0f, 0.0f, width, height, dim);

    const float title[4] = { 0.98f, 0.92f, 0.72f, 1.0f };
    const float text_color[4] = { 0.90f, 0.92f, 0.96f, 1.0f };
    const float muted[4] = { 0.56f, 0.60f, 0.68f, 1.0f };
    const bool near_table = player_near_table(&app->world, &app->player);

    draw_shadowed_centered(&ui, width * 0.5f, height * 0.055f, 4.0f, "CRAFTING", title);
    ui_text_centered(&ui, width * 0.5f, height * 0.13f, 1.6f,
                     near_table ? "CRAFTING TABLE IN REACH" : "NO TABLE: BASIC RECIPES ONLY",
                     near_table ? title : muted);

    float y = height * 0.21f;
    for (int i = 0; i < recipe_count(); ++i) {
        const BgRecipe *recipe = recipe_at(i);
        const bool can = recipe_can_craft(app->player.inventory, recipe, near_table);
        const bool selected = (i == app->craft_choice);

        char line[128];
        recipe_line(recipe, line, sizeof(line));

        const float row_w = 660.0f;
        const float row_x = width * 0.5f - row_w * 0.5f;
        if (selected) {
            const float plate[4] = { 0.10f, 0.12f, 0.17f, 0.95f };
            ui_rect(&ui, row_x, y - 8.0f, row_w, 30.0f, plate);
            const float edge[4] = { 0.90f, 0.78f, 0.40f, 1.0f };
            ui_rect(&ui, row_x, y - 8.0f, 3.0f, 30.0f, edge);
        }
        const float color[4] = { can ? 0.84f : 0.44f, can ? 0.90f : 0.46f,
                                 can ? 0.98f : 0.52f, 1.0f };
        ui_text_centered(&ui, width * 0.5f, y, 2.0f, line, color);
        y += 31.0f;
    }

    float inv_y = height * 0.73f;
    ui_text_centered(&ui, width * 0.5f, inv_y, 1.6f, "INVENTORY", muted);
    inv_y += 26.0f;

    char line[160] = { 0 };
    size_t used = 0;
    for (int item = 1; item < BG_ITEM_COUNT; ++item) {
        const int count = inventory_count(app->player.inventory, (uint16_t)item);
        if (count <= 0) {
            continue;
        }
        char part[48];
        snprintf(part, sizeof(part), "%s X%d   ", item_name((uint16_t)item), count);
        if (used + strlen(part) >= sizeof(line) - 1 || used > 68) {
            ui_text_centered(&ui, width * 0.5f, inv_y, 1.5f, line, text_color);
            inv_y += 20.0f;
            line[0] = '\0';
            used = 0;
        }
        const size_t length = strlen(part);
        memcpy(line + used, part, length + 1);
        used += length;
    }
    if (used > 0) {
        ui_text_centered(&ui, width * 0.5f, inv_y, 1.5f, line, text_color);
    }

    const float tip[4] = { 0.86f, 0.80f, 0.56f, 0.90f };
    ui_text_centered(&ui, width * 0.5f, height - 92.0f, 1.5f,
                     "4X5 VOIDSTONE FRAME + EMBER SHARD OPENS A RIFT", tip);
    ui_text_centered(&ui, width * 0.5f, height - 62.0f, 1.7f,
                     "UP/DOWN SELECT   ENTER CRAFT   C CLOSE", text_color);

    if (app->toast_timer > 0.0f) {
        ui_text_centered(&ui, width * 0.5f, height - 120.0f, 1.7f, app->toast, title);
    }

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void handle_crafting_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_C) || key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->show_crafting = false;
        set_cursor_captured(window, true);
        return;
    }
    if (recipe_count() <= 0) {
        return;
    }
    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->craft_choice = (app->craft_choice + recipe_count() - 1) % recipe_count();
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->craft_choice = (app->craft_choice + 1) % recipe_count();
    }
    if (key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER)) {
        const bool near_table = player_near_table(&app->world, &app->player);
        const BgRecipe *recipe = recipe_at(app->craft_choice);
        if (!recipe) {
            return;
        }
        if (recipe->needs_table && !near_table) {
            set_toast(app, "You need a crafting table nearby.");
        } else if (!recipe_can_craft(app->player.inventory, recipe, near_table)) {
            set_toast(app, "Not enough materials.");
        } else {
            recipe_craft(app->player.inventory, recipe, near_table);
            char message[96];
            snprintf(message, sizeof(message), "Crafted %s X%d",
                     item_name(recipe->output.item), (int)recipe->output.count);
            set_toast(app, message);
        }
    }
}

/* The classic world-build screen: dirt backdrop, logo and a progress bar. */
static void render_loading(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    menu_background(&ui, width, height);

    draw_logo(&ui, width * 0.5f, height * 0.18f, 6.0f);

    const float title[4] = { 0.96f, 0.94f, 0.88f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.40f, 2.4f, "BUILDING WORLD", title);

    const float bw = 520.0f;
    const float bh = 22.0f;
    const float bx = width * 0.5f - bw * 0.5f;
    const float by = height * 0.49f;
    float progress = app->loading_total > 0
                         ? (float)app->loading_cursor / (float)app->loading_total
                         : 1.0f;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    const float frame[4] = { 0.05f, 0.05f, 0.06f, 1.0f };
    ui_rect(&ui, bx - 3.0f, by - 3.0f, bw + 6.0f, bh + 6.0f, frame);
    const float track[4] = { 0.20f, 0.20f, 0.21f, 1.0f };
    ui_rect(&ui, bx, by, bw, bh, track);
    const float fill[4] = { 0.36f, 0.75f, 0.20f, 1.0f };
    ui_rect(&ui, bx, by, bw * progress, bh, fill);
    const float gloss[4] = { 0.62f, 0.92f, 0.42f, 0.85f };
    ui_rect(&ui, bx, by, bw * progress, 5.0f, gloss);

    char line[96];
    snprintf(line, sizeof(line), "%d%%   %s", (int)(progress * 100.0f + 0.5f),
             app->loading_dimension == BG_UNDERWORLD ? "UNDERWORLD" : "UPPERWORLD");
    const float soft[4] = { 0.86f, 0.88f, 0.92f, 0.90f };
    ui_text_centered(&ui, width * 0.5f, by + bh + 16.0f, 1.8f, line, soft);

    ui_text_centered(&ui, width * 0.5f, height - 56.0f, 1.5f,
                     "TIP: TEN VOIDSTONE BLOCKS IN A 4X5 RING, LIT WITH AN EMBER SHARD, OPEN A RIFT.",
                     soft);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

/* ------------------------------------------------------------- game loop */

static void handle_title_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->menu_choice = (app->menu_choice + 2) % 3;
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->menu_choice = (app->menu_choice + 1) % 3;
    }
    if (app->menu_choice == 0 && !app->has_save) {
        app->menu_choice = 1;
    }

    if (key_pressed(app, GLFW_KEY_ENTER)) {
        if (app->menu_choice == 0 && app->has_save) {
            if (!load_game(app, window)) {
                app->menu_choice = 1;
            }
        } else if (app->menu_choice == 1) {
            app->state = STATE_CREATE;
            app->create_choice = 0;
        } else if (app->menu_choice == 2) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void handle_create_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_TITLE;
        return;
    }
    if (key_pressed(app, GLFW_KEY_TAB)) {
        app->create_choice = (app->create_choice + 1) % 3;
    }
    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_DOWN)) {
        app->create_choice = (app->create_choice + 1) % 3;
    }
    if (app->create_choice == 1 &&
        (key_pressed(app, GLFW_KEY_LEFT) || key_pressed(app, GLFW_KEY_RIGHT) ||
         key_pressed(app, GLFW_KEY_SPACE))) {
        app->create_dimension = app->create_dimension == BG_UPPERWORLD ? BG_UNDERWORLD
                                                                      : BG_UPPERWORLD;
        return;
    }
    if (app->create_choice == 0 && key_pressed(app, GLFW_KEY_BACKSPACE)) {
        const size_t length = strlen(app->seed_text);
        if (length > 0) {
            app->seed_text[length - 1] = '\0';
        }
    }
    if (key_pressed(app, GLFW_KEY_ENTER)) {
        start_new_world(app, window);
    }
}

static void handle_pause_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->pause_choice = (app->pause_choice + 2) % 3;
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->pause_choice = (app->pause_choice + 1) % 3;
    }
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_PLAYING;
        set_cursor_captured(window, true);
        return;
    }
    if (key_pressed(app, GLFW_KEY_ENTER)) {
        if (app->pause_choice == 0) {
            app->state = STATE_PLAYING;
            set_cursor_captured(window, true);
        } else if (app->pause_choice == 1) {
            save_and_title(app, window);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

static void update_playing(App *app, float dt, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_PAUSED;
        app->pause_choice = 0;
        set_cursor_captured(window, false);
        return;
    }

    if (app->toast_timer > 0.0f) {
        app->toast_timer -= dt;
    }

    if (key_pressed(app, GLFW_KEY_C)) {
        app->show_crafting = true;
        app->craft_choice = 0;
        set_cursor_captured(window, false);
        return;
    }
    if (app->show_crafting) {
        handle_crafting_input(app, window);
        mobs_update(&app->mobs, &app->world, &app->player, dt);
        if (app->player.health <= 0) {
            respawn(app);
        }
        return;
    }

    app->player.yaw += (float)app->input.mouse_dx * MOUSE_SENSITIVITY;
    app->player.pitch -= (float)app->input.mouse_dy * MOUSE_SENSITIVITY;
    if (app->player.pitch > 1.5533f) app->player.pitch = 1.5533f;
    if (app->player.pitch < -1.5533f) app->player.pitch = -1.5533f;
    app->input.mouse_dx = 0;
    app->input.mouse_dy = 0;

    for (int i = 0; i < 9; ++i) {
        if (key_pressed(app, GLFW_KEY_1 + i)) {
            app->player.selected = i;
        }
    }

    player_update(&app->player, dt, &app->input, &app->world);

    if (app->input.mouse[0]) {
        const int mob_index = mobs_pick(&app->mobs, &app->world, &app->player, 4.0f);
        if (mob_index >= 0) {
            MobType killed = MOB_TYPE_COUNT;
            const float dir_x = cosf(app->player.yaw);
            const float dir_z = sinf(app->player.yaw);
            if (mobs_damage(&app->mobs, mob_index, 6, dir_x, dir_z, &killed)) {
                BgStack drops[2];
                const int drop_count = mob_drops(killed, drops);
                for (int i = 0; i < drop_count; ++i) {
                    inventory_add(app->player.inventory, drops[i].item, drops[i].count);
                }
                char message[96];
                snprintf(message, sizeof(message), "%s defeated", mob_name(killed));
                set_toast(app, message);
            }
        } else {
            int bx = 0, by = 0, bz = 0;
            BlockId type = BG_AIR;
            int normal[3] = { 0, 0, 0 };
            if (player_raycast(&app->player, &app->world, 6.0f, &bx, &by, &bz, &type,
                               normal) &&
                block_breakable(type)) {
                world_set_block(&app->world, bx, by, bz, BG_AIR);
                remesh_near(app, bx, by, bz);
                uint16_t drop_item = BG_AIR;
                int drop_amount = 0;
                if (block_drop(type, &drop_item, &drop_amount)) {
                    inventory_add(app->player.inventory, drop_item, drop_amount);
                }
            }
        }
    }
    if (app->input.mouse[1]) {
        BgStack *held = &app->player.inventory[app->player.selected];
        BlockId block = BG_AIR;
        if (held->count > 0 && item_block(held->item, &block)) {
            int px = 0, py = 0, pz = 0;
            if (player_place(&app->player, &app->world, block, &px, &py, &pz)) {
                remesh_near(app, px, py, pz);
                held->count--;
                if (held->count == 0) {
                    held->item = BG_AIR;
                }
            }
        } else if (held->count > 0 && held->item == BG_ITEM_EMBER_SHARD) {
            /* An ember shard struck against a voidstone frame lights the rift. */
            int bx = 0, by = 0, bz = 0;
            BlockId type = BG_AIR;
            int normal[3] = { 0, 0, 0 };
            if (player_raycast(&app->player, &app->world, 6.0f, &bx, &by, &bz, &type,
                               normal) &&
                type == BG_VOIDSTONE) {
                int cx = 0, cy = 0, cz = 0, axis = 0;
                const RiftFire fire = rift_ignite(&app->world, bx, by, bz, &cx, &cy, &cz, &axis);
                if (fire == RIFT_OPENED) {
                    inventory_remove(app->player.inventory, BG_ITEM_EMBER_SHARD, 1);
                    remesh_near(app, axis == 0 ? cx + 1 : cx, cy + 2,
                                axis == 0 ? cz : cz + 1);
                    set_toast(app, "The rift tears open.");
                } else if (fire == RIFT_ALREADY_OPEN) {
                    set_toast(app, "The rift is already open.");
                } else {
                    set_toast(app, "A rift needs a 4x5 voidstone frame, hollow in the middle.");
                }
            }
        }
    }

    if (key_pressed(app, GLFW_KEY_F)) {
        if (app->player.health >= 20) {
            set_toast(app, "Not hungry.");
        } else if (inventory_count(app->player.inventory, BG_ITEM_MEAT) > 0) {
            inventory_remove(app->player.inventory, BG_ITEM_MEAT, 1);
            app->player.health += 4;
            if (app->player.health > 20) {
                app->player.health = 20;
            }
            set_toast(app, "Ate meat.");
        } else {
            set_toast(app, "Nothing to eat.");
        }
    }

    mobs_update(&app->mobs, &app->world, &app->player, dt);

    stream_around_player(app);

    const BlockId at_feet = world_get_block(&app->world, (int)floorf(app->player.x),
                                            (int)floorf(app->player.y + 0.5f),
                                            (int)floorf(app->player.z));
    const BlockId at_eye = world_get_block(&app->world, (int)floorf(app->player.x),
                                           (int)floorf(app->player.y + EYE_HEIGHT),
                                           (int)floorf(app->player.z));
    if (!app->travel_armed && (block_portal(at_feet) || block_portal(at_eye))) {
        app->travel_armed = true;
        travel(app);
    } else if (!block_portal(at_feet) && !block_portal(at_eye)) {
        app->travel_armed = false;
    }

    if (app->player.health <= 0) {
        respawn(app);
    }
}

static void draw_world(App *app) {
    BgCamera camera;
    camera.x = app->player.x;
    camera.y = app->player.y + EYE_HEIGHT;
    camera.z = app->player.z;
    camera.yaw = app->player.yaw;
    camera.pitch = app->player.pitch;
    camera.fov = FIELD_OF_VIEW;
    camera.aspect = app->height > 0 ? (float)app->width / (float)app->height : 16.0f / 9.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 400.0f;

    renderer_draw(&app->renderer, &camera);
    renderer_draw_mobs(&app->renderer, &camera, &app->mobs);
}

int main(void) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialise GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "BlockForge", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create a window with OpenGL 3.3\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    memset(&g_app, 0, sizeof(g_app));
    g_app.state = STATE_TITLE;
    g_app.menu_choice = 1;
    g_app.create_dimension = BG_UPPERWORLD;
    strcpy(g_app.seed_text, "1337");
    g_app.width = 1280;
    g_app.height = 720;
    g_app.last_cx = 0x7FFFFFF;
    g_app.last_cz = 0x7FFFFFF;

    world_init(&g_app.world);
    world_set_seed(&g_app.world, 1337u);
    player_init(&g_app.player);
    mobs_init(&g_app.mobs);

    if (!renderer_init(&g_app.renderer)) {
        fprintf(stderr, "Renderer initialisation failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    FILE *existing = fopen(SAVE_PATH, "rb");
    if (existing) {
        fclose(existing);
        g_app.has_save = true;
        g_app.menu_choice = 0;
    }

    glfwSetKeyCallback(window, on_key);
    glfwSetCharCallback(window, on_char);
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);

    glViewport(0, 0, g_app.width, g_app.height);

    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        float dt = (float)(now - previous);
        previous = now;
        if (dt > 0.1f) {
            dt = 0.1f;
        }
        g_app.time += dt;

        if (g_app.state == STATE_TITLE) {
            handle_title_input(&g_app, window);
        } else if (g_app.state == STATE_CREATE) {
            handle_create_input(&g_app, window);
        } else if (g_app.state == STATE_LOADING) {
            update_loading(&g_app, window);
        } else if (g_app.state == STATE_PAUSED) {
            handle_pause_input(&g_app, window);
        } else {
            update_playing(&g_app, dt, window);
        }

        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (g_app.state == STATE_TITLE) {
            render_title(&g_app);
        } else if (g_app.state == STATE_CREATE) {
            render_create(&g_app);
        } else if (g_app.state == STATE_LOADING) {
            render_loading(&g_app);
        } else {
            draw_world(&g_app);
            renderer_draw_hud(&g_app.renderer, &g_app.player);
            render_toast(&g_app);
            if (g_app.show_crafting) {
                render_crafting(&g_app);
            }
            if (g_app.state == STATE_PAUSED) {
                render_pause(&g_app);
            }
        }

        glfwSwapBuffers(window);
        memcpy(g_app.prev_keys, g_app.input.keys, sizeof(g_app.prev_keys));
        g_app.input.mouse[0] = false;
        g_app.input.mouse[1] = false;
    }

    renderer_shutdown(&g_app.renderer);
    world_free(&g_app.world);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
