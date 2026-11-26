#include "bg.h"
#include "render.h"

#include <GLFW/glfw3.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define WORLD_DIR "worlds"
#define WORLD_INDEX "worlds/worlds.idx"
#define LEGACY_SAVE "blockforge.sav"
#define MAX_WORLDS 8
#define WORLD_NAME_MAX 24
#define RENDER_RADIUS 5
#define MAX_TRACKED_COLUMNS 2048
#define EYE_HEIGHT 1.62f
#define MOUSE_SENSITIVITY 0.0022f
#define FIELD_OF_VIEW 1.2217305f

typedef enum {
    STATE_TITLE,
    STATE_WORLDS,
    STATE_CREATE,
    STATE_LOADING,
    STATE_PLAYING,
    STATE_PAUSED
} AppState;

/* One saved world, as listed on the world-select screen. */
typedef struct WorldEntry {
    int id;
    char name[WORLD_NAME_MAX];
    uint64_t seed;
} WorldEntry;

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
    char name_text[WORLD_NAME_MAX];
    Dimension create_dimension;
    bool loading;
    Dimension loading_dimension;
    int loading_center_x, loading_center_z;
    int loading_radius;
    int loading_cursor, loading_total;
    uint64_t tracked[MAX_TRACKED_COLUMNS];
    size_t tracked_count;
    int last_cx, last_cz;
    bool travel_armed;
    BgMobWorld mobs;
    bool show_inventory;
    int craft_choice;
    WorldEntry worlds[MAX_WORLDS];
    int world_count;
    int world_choice;
    int active_world;
    char save_path[96];
    float mouse_x, mouse_y;
    int mine_x, mine_y, mine_z;
    float mine_progress;
    float mine_hint_timer;
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
    if (g_app.state != STATE_CREATE) {
        return;
    }
    if (g_app.create_choice == 0) {
        if (!(isalnum((int)codepoint) || codepoint == ' ' || codepoint == '-' ||
              codepoint == '_')) {
            return;
        }
        const size_t length = strlen(g_app.name_text);
        if (length < sizeof(g_app.name_text) - 1) {
            g_app.name_text[length] = (char)codepoint;
            g_app.name_text[length + 1] = '\0';
        }
    } else if (g_app.create_choice == 1) {
        if (codepoint < '0' || codepoint > '9') {
            return;
        }
        const size_t length = strlen(g_app.seed_text);
        if (length < sizeof(g_app.seed_text) - 1) {
            g_app.seed_text[length] = (char)codepoint;
            g_app.seed_text[length + 1] = '\0';
        }
    }
}

static void on_mouse_button(GLFWwindow *window, int button, int action, int mods) {
    (void)window;
    (void)mods;
    if (button < 0 || button >= 5) {
        return;
    }
    g_app.input.mouse[button] = (action == GLFW_PRESS);
    g_app.input.mouse_held[button] = (action != GLFW_RELEASE);
}

static void on_cursor(GLFWwindow *window, double x, double y) {
    static double last_x = 0.0;
    static double last_y = 0.0;
    static bool initialised = false;

    /* Menus are laid out in framebuffer pixels, so the cursor is scaled to
     * match on displays where the window and the framebuffer differ. */
    int window_w = 0;
    int window_h = 0;
    int frame_w = 0;
    int frame_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    glfwGetFramebufferSize(window, &frame_w, &frame_h);
    if (window_w > 0 && window_h > 0) {
        g_app.mouse_x = (float)(x * (double)frame_w / (double)window_w);
        g_app.mouse_y = (float)(y * (double)frame_h / (double)window_h);
    }

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

/* ---------------------------------------------------------------- layout */

/* Menus share these rectangles between drawing and hit testing so a click
 * always lands on the control it looks like it should. */
typedef struct UiRect {
    float x, y, w, h;
} UiRect;

static bool ui_hit(UiRect r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

static UiRect button_rect(float center_x, float start_y, float w, float h, float gap,
                          int index) {
    UiRect r;
    r.x = center_x - w * 0.5f;
    r.y = start_y + (float)index * (h + gap);
    r.w = w;
    r.h = h;
    return r;
}

static UiRect title_button_rect(const App *app, int index) {
    return button_rect((float)app->width * 0.5f, (float)app->height * 0.44f, 420.0f, 46.0f,
                       12.0f, index);
}

static UiRect pause_button_rect(const App *app, int index) {
    return button_rect((float)app->width * 0.5f, (float)app->height * 0.36f, 470.0f, 46.0f,
                       12.0f, index);
}

static UiRect create_field_rect(const App *app, int index) {
    UiRect r;
    r.w = 560.0f;
    r.h = 50.0f;
    r.x = (float)app->width * 0.5f - r.w * 0.5f;
    r.y = (float)app->height * (0.28f + 0.105f * (float)index);
    return r;
}

static UiRect create_button_rect(const App *app) {
    UiRect r;
    r.x = (float)app->width * 0.5f - 200.0f;
    r.y = (float)app->height * 0.65f;
    r.w = 400.0f;
    r.h = 50.0f;
    return r;
}

static UiRect worlds_row_rect(const App *app, int index) {
    UiRect r;
    r.x = (float)app->width * 0.5f - 330.0f;
    r.y = (float)app->height * 0.235f + (float)index * 50.0f;
    r.w = 660.0f;
    r.h = 42.0f;
    return r;
}

static UiRect worlds_button_rect(const App *app, int index) {
    const float w = 240.0f;
    const float gap = 24.0f;
    const float total = w * 2.0f + gap;
    UiRect r;
    r.x = (float)app->width * 0.5f - total * 0.5f + (float)index * (w + gap);
    r.y = (float)app->height * 0.86f;
    r.w = w;
    r.h = 46.0f;
    return r;
}

static UiRect inventory_panel_rect(const App *app) {
    UiRect r;
    r.w = (float)app->width - 80.0f;
    if (r.w > 900.0f) {
        r.w = 900.0f;
    }
    r.h = (float)app->height - 80.0f;
    if (r.h > 600.0f) {
        r.h = 600.0f;
    }
    r.x = ((float)app->width - r.w) * 0.5f;
    r.y = ((float)app->height - r.h) * 0.5f;
    return r;
}

static UiRect inventory_slot_rect(const App *app, int index) {
    const float slot = 46.0f;
    const float gap = 6.0f;
    const float total = (float)HOTBAR_SLOTS * slot + (float)(HOTBAR_SLOTS - 1) * gap;
    const UiRect panel = inventory_panel_rect(app);
    UiRect r;
    r.x = (float)app->width * 0.5f - total * 0.5f + (float)index * (slot + gap);
    r.y = panel.y + 70.0f;
    r.w = slot;
    r.h = slot;
    return r;
}

static UiRect inventory_row_rect(const App *app, int index) {
    const UiRect panel = inventory_panel_rect(app);
    UiRect r;
    r.x = panel.x + 18.0f;
    r.y = panel.y + 190.0f + (float)index * 24.0f;
    r.w = panel.w - 36.0f;
    r.h = 22.0f;
    return r;
}

/* --------------------------------------------------------------- worlds */

static void world_save_path(int id, char *out, size_t size) {
    snprintf(out, size, "%s/slot_%d.sav", WORLD_DIR, id);
}

static void ensure_world_dir(void) {
    FILE *probe = fopen(WORLD_INDEX, "rb");
    if (probe) {
        fclose(probe);
        return;
    }
#ifdef _WIN32
    _mkdir(WORLD_DIR);
#else
    mkdir(WORLD_DIR, 0755);
#endif
}

static void worlds_write_index(const App *app) {
    FILE *file = fopen(WORLD_INDEX, "wb");
    if (!file) {
        return;
    }
    fprintf(file, "BLOCKFORGE-WORLDS 1\n");
    for (int i = 0; i < app->world_count; ++i) {
        fprintf(file, "%d\t%s\t%llu\n", app->worlds[i].id, app->worlds[i].name,
                (unsigned long long)app->worlds[i].seed);
    }
    fclose(file);
}

static void worlds_read_index(App *app) {
    app->world_count = 0;
    FILE *file = fopen(WORLD_INDEX, "rb");
    if (!file) {
        return;
    }

    char line[160];
    bool header = true;
    while (fgets(line, sizeof(line), file) && app->world_count < MAX_WORLDS) {
        if (header) {
            header = false;
            if (strncmp(line, "BLOCKFORGE-WORLDS", 17) == 0) {
                continue;
            }
        }
        int id = 0;
        char name[WORLD_NAME_MAX] = { 0 };
        unsigned long long seed = 0;
        if (sscanf(line, "%d\t%23[^\t]\t%llu", &id, name, &seed) < 2) {
            continue;
        }
        /* A world whose save went missing is not offered any more. */
        char path[96];
        world_save_path(id, path, sizeof(path));
        FILE *probe = fopen(path, "rb");
        if (!probe) {
            continue;
        }
        fclose(probe);

        WorldEntry *entry = &app->worlds[app->world_count++];
        entry->id = id;
        snprintf(entry->name, sizeof(entry->name), "%s", name);
        entry->seed = (uint64_t)seed;
    }
    fclose(file);
}

static int worlds_next_id(const App *app) {
    int id = 1;
    for (int i = 0; i < app->world_count; ++i) {
        if (app->worlds[i].id >= id) {
            id = app->worlds[i].id + 1;
        }
    }
    return id;
}

/* Builds before this one kept a single save beside the executable, so adopt
 * it as the first world instead of losing somebody's build. */
static void worlds_import_legacy(App *app) {
    if (app->world_count > 0) {
        return;
    }
    FILE *legacy = fopen(LEGACY_SAVE, "rb");
    if (!legacy) {
        return;
    }
    fclose(legacy);

    char path[96];
    world_save_path(1, path, sizeof(path));
    FILE *existing = fopen(path, "rb");
    if (existing) {
        fclose(existing);
        return;
    }

    if (rename(LEGACY_SAVE, path) != 0) {
        FILE *src = fopen(LEGACY_SAVE, "rb");
        FILE *dst = fopen(path, "wb");
        if (!src || !dst) {
            if (src) {
                fclose(src);
            }
            if (dst) {
                fclose(dst);
            }
            return;
        }
        char buffer[8192];
        size_t read = 0;
        while ((read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, read, dst);
        }
        fclose(src);
        fclose(dst);
        remove(LEGACY_SAVE);
    }

    WorldEntry *entry = &app->worlds[app->world_count++];
    entry->id = 1;
    snprintf(entry->name, sizeof(entry->name), "Old World");
    entry->seed = 0;
    worlds_write_index(app);
}

/* Fills the create screen with sensible defaults for the next world. */
static void begin_create(App *app) {
    app->state = STATE_CREATE;
    app->create_choice = 0;
    app->create_dimension = BG_UPPERWORLD;
    snprintf(app->name_text, sizeof(app->name_text), "World %d", app->world_count + 1);
    snprintf(app->seed_text, sizeof(app->seed_text), "%d", 1337 + app->world_count * 101);
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
    /* A world exists on disk from the moment it is playable. */
    if (app->save_path[0] != '\0' && app->active_world >= 0) {
        save_world(app->save_path, &app->world, &app->player);
        worlds_write_index(app);
    }
    app->state = STATE_PLAYING;
    set_cursor_captured(window, true);
}

static void start_new_world(App *app, GLFWwindow *window) {
    (void)window;
    if (app->world_count >= MAX_WORLDS) {
        set_toast(app, "The world list is full.");
        app->state = STATE_WORLDS;
        return;
    }

    const uint64_t typed = (uint64_t)strtoull(app->seed_text, NULL, 10);
    const uint64_t seed = typed ? typed : 1337u;

    WorldEntry *entry = &app->worlds[app->world_count];
    entry->id = worlds_next_id(app);
    snprintf(entry->name, sizeof(entry->name), "%s",
             app->name_text[0] != '\0' ? app->name_text : "New World");
    entry->seed = seed;
    app->active_world = app->world_count;
    app->world_count++;
    worlds_write_index(app);
    world_save_path(entry->id, app->save_path, sizeof(app->save_path));

    world_set_seed(&app->world, seed);
    begin_loading(app, app->create_dimension);
}

static bool load_game(App *app, GLFWwindow *window) {
    if (app->world_choice < 0 || app->world_choice >= app->world_count) {
        return false;
    }

    const int index = app->world_choice;
    char path[96];
    world_save_path(app->worlds[index].id, path, sizeof(path));
    if (!load_world(path, &app->world, &app->player)) {
        return false;
    }

    snprintf(app->save_path, sizeof(app->save_path), "%s", path);
    app->active_world = index;
    app->worlds[index].seed = app->world.seed;
    worlds_write_index(app);

    renderer_set_environment(&app->renderer, app->world.dimension);
    reset_streaming(app);
    stream_around_player(app);
    app->travel_armed = false;
    app->show_inventory = false;
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
    if (app->active_world >= 0 && app->active_world < app->world_count &&
        app->save_path[0] != '\0') {
        save_world(app->save_path, &app->world, &app->player);
        app->worlds[app->active_world].seed = app->world.seed;
        worlds_write_index(app);
    }
    app->show_inventory = false;
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

    const char *labels[3] = { "PLAY", "NEW WORLD", "QUIT GAME" };
    for (int i = 0; i < 3; ++i) {
        const UiRect r = title_button_rect(app, i);
        const bool enabled = !(i == 0 && app->world_count == 0);
        draw_button(&ui, r.x, r.y, r.w, r.h, 2.4f, labels[i],
                    app->menu_choice == i && enabled, enabled);
    }

    if (app->world_count > 0) {
        char line[64];
        snprintf(line, sizeof(line), "%d WORLD%s SAVED", app->world_count,
                 app->world_count == 1 ? "" : "S");
        const float count_color[4] = { 0.80f, 0.84f, 0.90f, 0.85f };
        ui_text_centered(&ui, width * 0.5f, height * 0.44f + 3.0f * 58.0f + 8.0f, 1.5f, line,
                         count_color);
    }

    ui_text_centered(&ui, width * 0.5f, height - 40.0f, 1.5f,
                     "WASD MOVE   SPACE JUMP   LMB MINE   RMB BUILD   E INVENTORY   F EAT",
                     soft);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_worlds(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    menu_background(&ui, width, height);

    draw_logo(&ui, width * 0.5f, height * 0.055f, 5.0f);
    const float title[4] = { 0.98f, 0.90f, 0.62f, 1.0f };
    const float text_color[4] = { 0.92f, 0.94f, 0.98f, 1.0f };
    const float muted[4] = { 0.62f, 0.66f, 0.74f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.16f, 3.0f, "SELECT WORLD", title);

    if (app->world_count == 0) {
        draw_shadowed_centered(&ui, width * 0.5f, height * 0.36f, 2.2f, "NO WORLDS YET",
                               text_color);
        ui_text_centered(&ui, width * 0.5f, height * 0.44f, 1.8f,
                         "MAKE ONE AND START BUILDING.", muted);
    }

    for (int i = 0; i < app->world_count; ++i) {
        const UiRect r = worlds_row_rect(app, i);
        const bool selected = (i == app->world_choice);

        const float border[4] = { 0.05f, 0.05f, 0.06f, 0.95f };
        ui_rect(&ui, r.x - 3.0f, r.y - 3.0f, r.w + 6.0f, r.h + 6.0f, border);
        const float fill[4] = { selected ? 0.44f : 0.30f, selected ? 0.45f : 0.31f,
                                selected ? 0.48f : 0.33f, 1.0f };
        ui_rect(&ui, r.x, r.y, r.w, r.h, fill);
        const float light[4] = { 0.64f, 0.65f, 0.68f, selected ? 1.0f : 0.55f };
        ui_rect(&ui, r.x, r.y, r.w, 2.0f, light);
        ui_rect(&ui, r.x, r.y, 2.0f, r.h, light);
        const float dark[4] = { 0.11f, 0.11f, 0.13f, 0.90f };
        ui_rect(&ui, r.x, r.y + r.h - 2.0f, r.w, 2.0f, dark);
        ui_rect(&ui, r.x + r.w - 2.0f, r.y, 2.0f, r.h, dark);

        char seed[48];
        if (app->worlds[i].seed != 0) {
            snprintf(seed, sizeof(seed), "SEED %llu",
                     (unsigned long long)app->worlds[i].seed);
        } else {
            snprintf(seed, sizeof(seed), "SEED UNKNOWN");
        }
        const float name_shadow[4] = { 0.06f, 0.06f, 0.07f, 0.92f };
        ui_text(&ui, r.x + 20.0f, r.y + 13.0f, 2.0f, app->worlds[i].name, name_shadow);
        ui_text(&ui, r.x + 18.0f, r.y + 11.0f, 2.0f, app->worlds[i].name,
                selected ? title : text_color);
        ui_text(&ui, r.x + r.w - 18.0f - ui_text_width(seed, 1.5f), r.y + 16.0f, 1.5f, seed,
                muted);
    }

    for (int i = 0; i < 2; ++i) {
        const UiRect r = worlds_button_rect(app, i);
        draw_button(&ui, r.x, r.y, r.w, r.h, 2.2f, i == 0 ? "NEW WORLD" : "BACK", false, true);
    }

    ui_text_centered(&ui, width * 0.5f, height - 34.0f, 1.5f,
                     "CLICK A WORLD TO PLAY IT   OR PICK ONE AND PRESS ENTER", muted);

    if (app->toast_timer > 0.0f) {
        ui_text_centered(&ui, width * 0.5f, height - 62.0f, 1.7f, app->toast, title);
    }

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_create(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    menu_background(&ui, width, height);

    draw_logo(&ui, width * 0.5f, height * 0.045f, 4.5f);
    const float title[4] = { 0.98f, 0.90f, 0.62f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.165f, 2.8f, "NEW WORLD", title);

    char name_line[64];
    snprintf(name_line, sizeof(name_line), "NAME  %s%s", app->name_text,
             app->create_choice == 0 ? "_" : "");
    const UiRect name_field = create_field_rect(app, 0);
    draw_field(&ui, name_field.x, name_field.y, name_field.w, name_field.h, 2.0f, name_line,
               app->create_choice == 0);

    char seed_line[64];
    snprintf(seed_line, sizeof(seed_line), "SEED  %s%s", app->seed_text,
             app->create_choice == 1 ? "_" : "");
    const UiRect seed_field = create_field_rect(app, 1);
    draw_field(&ui, seed_field.x, seed_field.y, seed_field.w, seed_field.h, 2.0f, seed_line,
               app->create_choice == 1);

    char realm_line[64];
    snprintf(realm_line, sizeof(realm_line), "REALM  %s%s%s",
             app->create_choice == 2 ? "< " : "",
             app->create_dimension == BG_UPPERWORLD ? "UPPERWORLD" : "UNDERWORLD",
             app->create_choice == 2 ? " >" : "");
    const UiRect realm_field = create_field_rect(app, 2);
    draw_field(&ui, realm_field.x, realm_field.y, realm_field.w, realm_field.h, 2.0f,
               realm_line, app->create_choice == 2);

    const UiRect button = create_button_rect(app);
    draw_button(&ui, button.x, button.y, button.w, button.h, 2.4f, "CREATE WORLD",
                app->create_choice == 3, true);

    const float hint[4] = { 0.82f, 0.84f, 0.88f, 0.85f };
    ui_text_centered(&ui, width * 0.5f, height * 0.80f, 1.6f,
                     "TYPE A NAME AND SEED   LEFT/RIGHT CHANGES REALM   ENTER CREATES   ESC BACK",
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

    const char *labels[3] = { "BACK TO GAME", "SAVE AND QUIT TO TITLE", "QUIT TO DESKTOP" };
    for (int i = 0; i < 3; ++i) {
        const UiRect r = pause_button_rect(app, i);
        draw_button(&ui, r.x, r.y, r.w, r.h, 2.4f, labels[i], i == app->pause_choice, true);
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

/* The pack and the recipe list share one screen: nine real slots across the
 * top, hearts under them, then every recipe with its output icon. */
static void render_inventory(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    const float dim[4] = { 0.02f, 0.02f, 0.03f, 0.78f };
    ui_rect(&ui, 0.0f, 0.0f, width, height, dim);

    const UiRect panel = inventory_panel_rect(app);
    const float title[4] = { 0.98f, 0.92f, 0.72f, 1.0f };
    const float text_color[4] = { 0.90f, 0.92f, 0.96f, 1.0f };
    const float muted[4] = { 0.60f, 0.64f, 0.72f, 1.0f };
    const bool near_table = player_near_table(&app->world, &app->player);

    const float border[4] = { 0.04f, 0.04f, 0.05f, 0.95f };
    ui_rect(&ui, panel.x - 4.0f, panel.y - 4.0f, panel.w + 8.0f, panel.h + 8.0f, border);
    const float plate[4] = { 0.16f, 0.16f, 0.18f, 0.97f };
    ui_rect(&ui, panel.x, panel.y, panel.w, panel.h, plate);
    const float light[4] = { 0.42f, 0.43f, 0.47f, 0.90f };
    ui_rect(&ui, panel.x, panel.y, panel.w, 3.0f, light);
    ui_rect(&ui, panel.x, panel.y, 3.0f, panel.h, light);
    const float dark[4] = { 0.07f, 0.07f, 0.08f, 0.95f };
    ui_rect(&ui, panel.x, panel.y + panel.h - 3.0f, panel.w, 3.0f, dark);
    ui_rect(&ui, panel.x + panel.w - 3.0f, panel.y, 3.0f, panel.h, dark);

    draw_shadowed_centered(&ui, width * 0.5f, panel.y + 12.0f, 3.0f, "INVENTORY", title);
    ui_text_centered(&ui, width * 0.5f, panel.y + 46.0f, 1.5f,
                     near_table ? "CRAFTING TABLE IN REACH" : "NO TABLE: BASIC RECIPES ONLY",
                     near_table ? title : muted);

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        const UiRect r = inventory_slot_rect(app, i);
        const bool selected = (i == app->player.selected);
        const float cell[4] = { 0.10f, 0.10f, 0.11f, 0.96f };
        ui_rect(&ui, r.x, r.y, r.w, r.h, cell);
        const float bevel[4] = { 0.30f, 0.31f, 0.34f, 0.95f };
        ui_rect(&ui, r.x, r.y, r.w, 2.0f, bevel);
        ui_rect(&ui, r.x, r.y, 2.0f, r.h, bevel);

        const BgStack *stack = &app->player.inventory[i];
        if (stack->count > 0) {
            ui_draw_item_icon(&ui, r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, stack->item, 1.0f);
        }

        char key[4];
        snprintf(key, sizeof(key), "%d", i + 1);
        ui_text(&ui, r.x + 4.0f, r.y + 2.0f, 1.2f, key, muted);

        if (stack->count > 1) {
            char count[8];
            snprintf(count, sizeof(count), "%u", (unsigned)stack->count);
            const float count_shadow[4] = { 0.05f, 0.05f, 0.06f, 0.90f };
            ui_text(&ui, r.x + r.w - 3.0f - ui_text_width(count, 1.5f), r.y + r.h - 9.0f,
                    1.5f, count, count_shadow);
            ui_text(&ui, r.x + r.w - 4.0f - ui_text_width(count, 1.5f), r.y + r.h - 10.0f,
                    1.5f, count, text_color);
        }

        if (selected) {
            const float frame[4] = { 0.98f, 0.98f, 0.94f, 0.95f };
            ui_rect(&ui, r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, 2.0f, frame);
            ui_rect(&ui, r.x - 2.0f, r.y + r.h, r.w + 4.0f, 2.0f, frame);
            ui_rect(&ui, r.x - 2.0f, r.y - 2.0f, 2.0f, r.h + 4.0f, frame);
            ui_rect(&ui, r.x + r.w, r.y - 2.0f, 2.0f, r.h + 4.0f, frame);
        }
    }

    ui_draw_hearts(&ui, width * 0.5f - 100.0f, panel.y + 126.0f, 2.0f, app->player.health, 10);

    const BgStack *held = &app->player.inventory[app->player.selected];
    if (held->count > 0) {
        ui_text_centered(&ui, width * 0.5f, panel.y + 148.0f, 1.5f, item_name(held->item),
                         muted);
    }

    const float sub[4] = { 0.86f, 0.82f, 0.60f, 0.95f };
    ui_text(&ui, panel.x + 20.0f, panel.y + 166.0f, 1.6f, "CRAFTING", sub);

    for (int i = 0; i < recipe_count(); ++i) {
        const BgRecipe *recipe = recipe_at(i);
        if (!recipe) {
            continue;
        }
        const UiRect r = inventory_row_rect(app, i);
        const bool can = recipe_can_craft(app->player.inventory, recipe, near_table);
        const bool selected = (i == app->craft_choice);

        if (selected) {
            const float row_plate[4] = { 0.24f, 0.26f, 0.32f, 0.95f };
            ui_rect(&ui, r.x, r.y, r.w, r.h, row_plate);
            const float edge[4] = { 0.90f, 0.78f, 0.40f, 1.0f };
            ui_rect(&ui, r.x, r.y, 3.0f, r.h, edge);
        }

        ui_draw_item_icon(&ui, r.x + 6.0f, r.y + 3.0f, 16.0f, recipe->output.item,
                          can ? 1.0f : 0.55f);

        char line[128];
        recipe_line(recipe, line, sizeof(line));
        const float color[4] = { can ? 0.86f : 0.46f, can ? 0.92f : 0.48f,
                                 can ? 0.98f : 0.54f, 1.0f };
        ui_text(&ui, r.x + 30.0f, r.y + 5.0f, 1.7f, line, color);
    }

    ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 42.0f, 1.5f,
                     "CLICK A RECIPE TO CRAFT IT   UP/DOWN + ENTER ALSO WORKS", text_color);
    ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 18.0f, 1.5f,
                     "CLICK A SLOT OR PRESS 1-9 TO HOLD IT   E OR ESC CLOSES", muted);

    if (app->toast_timer > 0.0f) {
        ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 66.0f, 1.6f, app->toast, title);
    }

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void craft_selected(App *app) {
    const bool near_table = player_near_table(&app->world, &app->player);
    const BgRecipe *recipe = recipe_at(app->craft_choice);
    if (!recipe) {
        return;
    }
    if (recipe->needs_table && !near_table) {
        set_toast(app, "You need a crafting table nearby.");
        return;
    }
    if (!recipe_can_craft(app->player.inventory, recipe, near_table)) {
        set_toast(app, "Not enough materials.");
        return;
    }
    recipe_craft(app->player.inventory, recipe, near_table);
    char message[96];
    snprintf(message, sizeof(message), "Crafted %s X%d", item_name(recipe->output.item),
             (int)recipe->output.count);
    set_toast(app, message);
}

static void handle_inventory_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_E) || key_pressed(app, GLFW_KEY_C) ||
        key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->show_inventory = false;
        set_cursor_captured(window, true);
        return;
    }

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        if (key_pressed(app, GLFW_KEY_1 + i)) {
            app->player.selected = i;
        }
    }

    if (recipe_count() > 0) {
        if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
            app->craft_choice = (app->craft_choice + recipe_count() - 1) % recipe_count();
        }
        if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
            app->craft_choice = (app->craft_choice + 1) % recipe_count();
        }
        if (key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER)) {
            craft_selected(app);
        }
    }

    if (!app->input.mouse[0]) {
        return;
    }
    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        if (ui_hit(inventory_slot_rect(app, i), app->mouse_x, app->mouse_y)) {
            app->player.selected = i;
            return;
        }
    }
    for (int i = 0; i < recipe_count(); ++i) {
        if (ui_hit(inventory_row_rect(app, i), app->mouse_x, app->mouse_y)) {
            app->craft_choice = i;
            craft_selected(app);
            return;
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
    const bool has_world = app->world_count > 0;

    for (int i = 0; i < 3; ++i) {
        if (i == 0 && !has_world) {
            continue;
        }
        if (ui_hit(title_button_rect(app, i), app->mouse_x, app->mouse_y)) {
            app->menu_choice = i;
        }
    }

    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->menu_choice = (app->menu_choice + 2) % 3;
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->menu_choice = (app->menu_choice + 1) % 3;
    }
    if (app->menu_choice == 0 && !has_world) {
        app->menu_choice = 1;
    }

    bool activate = key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER);
    if (app->input.mouse[0]) {
        for (int i = 0; i < 3; ++i) {
            if (i == 0 && !has_world) {
                continue;
            }
            if (ui_hit(title_button_rect(app, i), app->mouse_x, app->mouse_y)) {
                app->menu_choice = i;
                activate = true;
            }
        }
    }

    if (activate) {
        if (app->menu_choice == 0) {
            app->world_choice = (app->active_world >= 0 && app->active_world < app->world_count)
                                    ? app->active_world
                                    : 0;
            app->state = STATE_WORLDS;
        } else if (app->menu_choice == 1) {
            begin_create(app);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void handle_worlds_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_TITLE;
        app->menu_choice = 0;
        return;
    }

    if (app->world_count > 0) {
        if (app->world_choice < 0 || app->world_choice >= app->world_count) {
            app->world_choice = 0;
        }
        for (int i = 0; i < app->world_count; ++i) {
            if (ui_hit(worlds_row_rect(app, i), app->mouse_x, app->mouse_y)) {
                app->world_choice = i;
            }
        }
        if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
            app->world_choice = (app->world_choice + app->world_count - 1) % app->world_count;
        }
        if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
            app->world_choice = (app->world_choice + 1) % app->world_count;
        }
    }

    if (app->input.mouse[0]) {
        if (ui_hit(worlds_button_rect(app, 0), app->mouse_x, app->mouse_y)) {
            begin_create(app);
            return;
        }
        if (ui_hit(worlds_button_rect(app, 1), app->mouse_x, app->mouse_y)) {
            app->state = STATE_TITLE;
            app->menu_choice = 0;
            return;
        }
    }

    bool play = key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER);
    if (app->input.mouse[0]) {
        for (int i = 0; i < app->world_count; ++i) {
            if (ui_hit(worlds_row_rect(app, i), app->mouse_x, app->mouse_y)) {
                app->world_choice = i;
                play = true;
            }
        }
    }
    if (play && app->world_count > 0 && !load_game(app, window)) {
        set_toast(app, "That world could not be opened.");
    }
}

static void handle_create_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_TITLE;
        app->menu_choice = 0;
        return;
    }

    if (app->input.mouse[0]) {
        for (int i = 0; i < 3; ++i) {
            if (!ui_hit(create_field_rect(app, i), app->mouse_x, app->mouse_y)) {
                continue;
            }
            app->create_choice = i;
            if (i == 2) {
                app->create_dimension = app->create_dimension == BG_UPPERWORLD
                                            ? BG_UNDERWORLD
                                            : BG_UPPERWORLD;
            }
        }
        const UiRect button = create_button_rect(app);
        if (ui_hit(button, app->mouse_x, app->mouse_y)) {
            app->create_choice = 3;
            start_new_world(app, window);
            return;
        }
    }

    if (key_pressed(app, GLFW_KEY_TAB) || key_pressed(app, GLFW_KEY_UP) ||
        key_pressed(app, GLFW_KEY_DOWN)) {
        app->create_choice = (app->create_choice + 1) % 4;
    }
    if (app->create_choice == 2 &&
        (key_pressed(app, GLFW_KEY_LEFT) || key_pressed(app, GLFW_KEY_RIGHT) ||
         key_pressed(app, GLFW_KEY_SPACE))) {
        app->create_dimension = app->create_dimension == BG_UPPERWORLD ? BG_UNDERWORLD
                                                                      : BG_UPPERWORLD;
        return;
    }
    if (key_pressed(app, GLFW_KEY_BACKSPACE)) {
        if (app->create_choice == 0) {
            const size_t length = strlen(app->name_text);
            if (length > 0) {
                app->name_text[length - 1] = '\0';
            }
        } else if (app->create_choice == 1) {
            const size_t length = strlen(app->seed_text);
            if (length > 0) {
                app->seed_text[length - 1] = '\0';
            }
        }
    }
    if (key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER)) {
        start_new_world(app, window);
    }
}

static void handle_pause_input(App *app, GLFWwindow *window) {
    for (int i = 0; i < 3; ++i) {
        if (ui_hit(pause_button_rect(app, i), app->mouse_x, app->mouse_y)) {
            app->pause_choice = i;
        }
    }
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

    bool activate = key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER);
    if (app->input.mouse[0]) {
        for (int i = 0; i < 3; ++i) {
            if (ui_hit(pause_button_rect(app, i), app->mouse_x, app->mouse_y)) {
                app->pause_choice = i;
                activate = true;
            }
        }
    }
    if (!activate) {
        return;
    }
    if (app->pause_choice == 0) {
        app->state = STATE_PLAYING;
        set_cursor_captured(window, true);
    } else if (app->pause_choice == 1) {
        save_and_title(app, window);
    } else {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void mine_tick(App *app, float dt) {
    int bx = 0;
    int by = 0;
    int bz = 0;
    BlockId type = BG_AIR;
    int normal[3] = { 0, 0, 0 };
    if (!player_raycast(&app->player, &app->world, 6.0f, &bx, &by, &bz, &type, normal) ||
        !block_breakable(type)) {
        app->mine_progress = 0.0f;
        return;
    }

    const BgStack *held = &app->player.inventory[app->player.selected];
    const uint16_t item = held->count > 0 ? held->item : BG_AIR;
    const int need = block_pick_tier(type);
    const int have = item_pick_tier(item);
    if (need > have) {
        app->mine_progress = 0.0f;
        if (app->mine_hint_timer <= 0.0f) {
            char message[96];
            snprintf(message, sizeof(message), "%s needs a %s.",
                     item_name((uint16_t)type),
                     need >= 2 ? "stone pickaxe" : "pickaxe");
            set_toast(app, message);
            app->mine_hint_timer = 3.0f;
        }
        return;
    }

    if (bx != app->mine_x || by != app->mine_y || bz != app->mine_z) {
        app->mine_x = bx;
        app->mine_y = by;
        app->mine_z = bz;
        app->mine_progress = 0.0f;
    }

    const float hardness = block_hardness(type);
    if (hardness <= 0.0f) {
        app->mine_progress = 0.0f;
        return;
    }
    const float speed = need > 0 ? item_pick_speed(item) : 1.0f;
    app->mine_progress += dt * speed / hardness;
    if (app->mine_progress < 1.0f) {
        return;
    }

    app->mine_progress = 0.0f;
    world_set_block(&app->world, bx, by, bz, BG_AIR);
    remesh_near(app, bx, by, bz);
    uint16_t drop_item = BG_AIR;
    int drop_amount = 0;
    if (block_drop(type, &drop_item, &drop_amount)) {
        inventory_add(app->player.inventory, drop_item, drop_amount);
    }
}

static void render_mining(App *app) {
    if (app->mine_progress <= 0.0f || app->mine_progress >= 1.0f) {
        return;
    }
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float w = (float)app->width;
    const float h = (float)app->height;
    const float bw = 64.0f;
    const float bh = 6.0f;
    const float x = w * 0.5f - bw * 0.5f;
    const float y = h * 0.5f + 24.0f;

    const float border[4] = { 0.04f, 0.04f, 0.05f, 0.90f };
    ui_rect(&ui, x - 2.0f, y - 2.0f, bw + 4.0f, bh + 4.0f, border);
    const float track[4] = { 0.16f, 0.16f, 0.17f, 0.95f };
    ui_rect(&ui, x, y, bw, bh, track);
    const float fill[4] = { 0.94f, 0.86f, 0.44f, 0.97f };
    ui_rect(&ui, x, y, bw * app->mine_progress, bh, fill);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void update_playing(App *app, float dt, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_PAUSED;
        app->pause_choice = 0;
        app->show_inventory = false;
        app->mine_progress = 0.0f;
        set_cursor_captured(window, false);
        return;
    }

    if (app->mine_hint_timer > 0.0f) {
        app->mine_hint_timer -= dt;
    }

    if (key_pressed(app, GLFW_KEY_E) || key_pressed(app, GLFW_KEY_C)) {
        app->show_inventory = true;
        app->craft_choice = 0;
        app->mine_progress = 0.0f;
        set_cursor_captured(window, false);
        return;
    }
    if (app->show_inventory) {
        handle_inventory_input(app, window);
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

    if (app->input.mouse_held[0]) {
        const int mob_index = mobs_pick(&app->mobs, &app->world, &app->player, 4.0f);
        if (mob_index >= 0) {
            app->mine_progress = 0.0f;
            if (app->input.mouse[0]) {
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
            }
        } else {
            mine_tick(app, dt);
        }
    } else {
        app->mine_progress = 0.0f;
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
    g_app.world_choice = 0;
    g_app.active_world = -1;
    g_app.create_dimension = BG_UPPERWORLD;
    snprintf(g_app.name_text, sizeof(g_app.name_text), "My World");
    snprintf(g_app.seed_text, sizeof(g_app.seed_text), "1337");
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

    ensure_world_dir();
    worlds_read_index(&g_app);
    worlds_import_legacy(&g_app);
    g_app.menu_choice = g_app.world_count > 0 ? 0 : 1;

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
        if (g_app.toast_timer > 0.0f) {
            g_app.toast_timer -= dt;
        }

        if (g_app.state == STATE_TITLE) {
            handle_title_input(&g_app, window);
        } else if (g_app.state == STATE_WORLDS) {
            handle_worlds_input(&g_app, window);
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
        } else if (g_app.state == STATE_WORLDS) {
            render_worlds(&g_app);
        } else if (g_app.state == STATE_CREATE) {
            render_create(&g_app);
        } else if (g_app.state == STATE_LOADING) {
            render_loading(&g_app);
        } else {
            draw_world(&g_app);
            renderer_draw_hud(&g_app.renderer, &g_app.player);
            render_mining(&g_app);
            render_toast(&g_app);
            if (g_app.show_inventory) {
                render_inventory(&g_app);
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
