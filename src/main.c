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
#define GATE_X 0
#define GATE_Z 0
#define EYE_HEIGHT 1.62f
#define MOUSE_SENSITIVITY 0.0022f
#define FIELD_OF_VIEW 1.2217305f

typedef enum {
    STATE_TITLE,
    STATE_CREATE,
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
    uint64_t tracked[MAX_TRACKED_COLUMNS];
    size_t tracked_count;
    int last_cx, last_cz;
    bool has_save;
    bool travel_armed;
    int width, height;
    float time;
} App;

static App g_app;

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

static void reset_streaming(App *app) {
    app->tracked_count = 0;
    renderer_clear_chunks(&app->renderer);
    app->last_cx = 0x7FFFFFF;
    app->last_cz = 0x7FFFFFF;
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

static void spawn_in_world(App *app, Dimension dimension, GLFWwindow *window) {
    world_clear(&app->world);
    world_set_dimension(&app->world, dimension);
    renderer_set_environment(&app->renderer, dimension);
    reset_streaming(app);

    world_generate_around(&app->world, GATE_X / CHUNK_SIZE, GATE_Z / CHUNK_SIZE,
                          RENDER_RADIUS + 1);
    build_rift_gate(&app->world, GATE_X, GATE_Z);

    player_init(&app->player);
    app->player.x = (float)GATE_X + 0.5f;
    app->player.z = (float)GATE_Z - 6.0f;
    app->player.y = (float)world_surface_y(&app->world, (int)app->player.x, (int)app->player.z);
    if (app->player.y < 2.0f) {
        app->player.y = 40.0f;
    }
    app->player.yaw = 1.5707963f;
    app->travel_armed = false;

    stream_around_player(app);
    app->state = STATE_PLAYING;
    set_cursor_captured(window, true);
}

static void start_new_world(App *app, GLFWwindow *window) {
    const uint64_t seed = (uint64_t)strtoull(app->seed_text, NULL, 10);
    world_set_seed(&app->world, seed ? seed : 1337u);
    spawn_in_world(app, app->create_dimension, window);
}

static bool load_game(App *app, GLFWwindow *window) {
    if (!load_world(SAVE_PATH, &app->world, &app->player)) {
        return false;
    }
    renderer_set_environment(&app->renderer, app->world.dimension);
    reset_streaming(app);
    stream_around_player(app);
    app->travel_armed = false;
    app->state = STATE_PLAYING;
    set_cursor_captured(window, true);
    return true;
}

static void travel(App *app) {
    const Dimension next = app->world.dimension == BG_UPPERWORLD ? BG_UNDERWORLD : BG_UPPERWORLD;
    world_clear(&app->world);
    world_set_dimension(&app->world, next);
    renderer_set_environment(&app->renderer, next);
    reset_streaming(app);

    world_generate_around(&app->world, GATE_X / CHUNK_SIZE, GATE_Z / CHUNK_SIZE,
                          RENDER_RADIUS + 1);
    int gate_y = build_rift_gate(&app->world, GATE_X, GATE_Z);
    if (gate_y < 2) {
        gate_y = world_surface_y(&app->world, GATE_X, GATE_Z);
    }

    app->player.vx = app->player.vy = app->player.vz = 0.0f;
    app->player.x = (float)GATE_X + 0.5f;
    app->player.z = (float)GATE_Z - 6.0f;
    app->player.y = (float)gate_y;
    app->player.yaw = 1.5707963f;
    app->player.pitch = 0.0f;
    app->travel_armed = false;

    stream_around_player(app);
}

static void save_and_title(App *app, GLFWwindow *window) {
    save_world(SAVE_PATH, &app->world, &app->player);
    app->has_save = true;
    app->state = STATE_TITLE;
    app->menu_choice = 0;
    set_cursor_captured(window, false);
}

/* ----------------------------------------------------------------- menus */

static void draw_panel(BgUi *ui, float width, float height) {
    const float dim[4] = { 0.04f, 0.05f, 0.08f, 0.78f };
    ui_rect(ui, 0.0f, 0.0f, width, height, dim);
}

static void draw_option(BgUi *ui, float center_x, float y, float scale, const char *label,
                        bool selected) {
    const float plate[4] = { 0.10f, 0.12f, 0.18f, selected ? 0.92f : 0.55f };
    const float width = ui_text_width(label, scale) + 48.0f;
    ui_rect(ui, center_x - width * 0.5f, y - scale * 3.0f, width, scale * 13.0f, plate);
    if (selected) {
        const float edge[4] = { 0.98f, 0.78f, 0.32f, 0.95f };
        ui_rect(ui, center_x - width * 0.5f, y - scale * 3.0f, 4.0f, scale * 13.0f, edge);
    }
    const float color[4] = { selected ? 1.0f : 0.78f, selected ? 1.0f : 0.80f,
                             selected ? 0.92f : 0.82f, 1.0f };
    ui_text_centered(ui, center_x, y, scale, label, color);
}

static void render_title(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    draw_panel(&ui, width, height);

    const float glow[4] = { 0.98f, 0.72f, 0.28f, 0.95f };
    const float soft[4] = { 0.72f, 0.80f, 0.94f, 0.85f };
    ui_text_centered(&ui, width * 0.5f, height * 0.18f, 9.0f, "BLOCKFORGE", glow);
    ui_text_centered(&ui, width * 0.5f, height * 0.30f, 2.0f, "TWO REALMS. ONE RIFT.", soft);

    const float option_scale = 3.0f;
    const float start_y = height * 0.46f;
    const char *labels[3] = { "CONTINUE", "NEW WORLD", "QUIT" };
    for (int i = 0; i < 3; ++i) {
        const bool selected = (i == app->menu_choice);
        if (i == 0 && !app->has_save) {
            const float faded[4] = { 0.45f, 0.48f, 0.55f, 1.0f };
            ui_text_centered(&ui, width * 0.5f, start_y + (float)i * 64.0f, option_scale,
                             "CONTINUE (NO SAVE)", faded);
            continue;
        }
        draw_option(&ui, width * 0.5f, start_y + (float)i * 64.0f, option_scale, labels[i],
                    selected);
    }

    ui_text_centered(&ui, width * 0.5f, height - 48.0f, 1.6f,
                     "WASD MOVE   SPACE JUMP   SHIFT SPRINT   LMB MINE   RMB BUILD   ESC PAUSE",
                     soft);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_create(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    draw_panel(&ui, width, height);

    const float title_color[4] = { 0.98f, 0.78f, 0.36f, 1.0f };
    const float text_color[4] = { 0.90f, 0.92f, 0.96f, 1.0f };
    ui_text_centered(&ui, width * 0.5f, height * 0.16f, 5.0f, "NEW WORLD", title_color);
    ui_text_centered(&ui, width * 0.5f, height * 0.26f, 1.8f, "TAB SWITCHES FIELD", text_color);

    char seed_line[64];
    snprintf(seed_line, sizeof(seed_line), "SEED: %s%s", app->seed_text,
             app->create_choice == 0 ? "_" : "");
    draw_option(&ui, width * 0.5f, height * 0.40f, 2.6f, seed_line, app->create_choice == 0);

    char realm_line[64];
    snprintf(realm_line, sizeof(realm_line), "REALM: %s",
             app->create_dimension == BG_UPPERWORLD ? "UPPERWORLD" : "UNDERWORLD");
    draw_option(&ui, width * 0.5f, height * 0.52f, 2.6f, realm_line, app->create_choice == 1);

    draw_option(&ui, width * 0.5f, height * 0.68f, 2.6f, "CREATE", app->create_choice == 2);
    ui_text_centered(&ui, width * 0.5f, height * 0.80f, 1.8f, "ESC GOES BACK", text_color);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_pause(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    draw_panel(&ui, width, height);

    const float title_color[4] = { 0.98f, 0.78f, 0.36f, 1.0f };
    const float text_color[4] = { 0.80f, 0.86f, 0.96f, 0.9f };
    ui_text_centered(&ui, width * 0.5f, height * 0.20f, 6.0f, "PAUSED", title_color);

    const char *labels[3] = { "RESUME", "SAVE AND TITLE", "QUIT TO DESKTOP" };
    for (int i = 0; i < 3; ++i) {
        draw_option(&ui, width * 0.5f, height * 0.40f + (float)i * 64.0f, 2.8f, labels[i],
                    i == app->pause_choice);
    }

    char where[96];
    snprintf(where, sizeof(where), "%s   X %.1f  Y %.1f  Z %.1f",
             dimension_name(app->world.dimension), app->player.x, app->player.y, app->player.z);
    ui_text_centered(&ui, width * 0.5f, height - 56.0f, 1.8f, where, text_color);

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
         key_pressed(app, GLFW_KEY_SPACE) || key_pressed(app, GLFW_KEY_ENTER))) {
        app->create_dimension = app->create_dimension == BG_UPPERWORLD ? BG_UNDERWORLD
                                                                      : BG_UPPERWORLD;
        if (key_pressed(app, GLFW_KEY_ENTER)) {
            start_new_world(app, window);
        }
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
        int bx = 0, by = 0, bz = 0;
        BlockId type = BG_AIR;
        int normal[3] = { 0, 0, 0 };
        if (player_raycast(&app->player, &app->world, 6.0f, &bx, &by, &bz, &type, normal) &&
            block_breakable(type)) {
            world_set_block(&app->world, bx, by, bz, BG_AIR);
            remesh_near(app, bx, by, bz);
        }
    }
    if (app->input.mouse[1]) {
        const BlockId block = app->player.hotbar[app->player.selected];
        int px = 0, py = 0, pz = 0;
        if (player_place(&app->player, &app->world, block, &px, &py, &pz)) {
            remesh_near(app, px, py, pz);
        }
    }

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
        } else {
            draw_world(&g_app);
            renderer_draw_hud(&g_app.renderer, &g_app.player);
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
