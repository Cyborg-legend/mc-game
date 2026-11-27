#include "bg.h"
#include "render.h"
#include "audio.h"

#include <GLFW/glfw3.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define WORLD_DIR "worlds"
#define WORLD_INDEX "worlds/worlds.idx"
#define LEGACY_SAVE "blockforge.sav"
#define MAX_WORLDS 8
#define WORLD_NAME_MAX 24
#define DEFAULT_RENDER_RADIUS 5
#define MAX_TRACKED_COLUMNS 2048
#define EYE_HEIGHT 1.62f
#define DEFAULT_SENSITIVITY 0.0022f
#define DEFAULT_FOV 1.2217305f
#define DAY_LENGTH 600.0f
#define AUTOSAVE_SECONDS 120.0f
#define SETTINGS_PATH "worlds/settings.cfg"

typedef enum {
    STATE_TITLE,
    STATE_WORLDS,
    STATE_CREATE,
    STATE_SETTINGS,
    STATE_LOADING,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_ENDING
} AppState;

/* One saved world, as listed on the world-select screen. */
typedef struct WorldEntry {
    int id;
    char name[WORLD_NAME_MAX];
    uint64_t seed;
    Dimension dimension; /* realm the player last stood in */
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
    int world_delete_confirm; /* row whose DELETE was clicked once */
    float mouse_x, mouse_y;
    int mine_x, mine_y, mine_z;
    float mine_progress;
    float mine_hint_timer;
    char toast[96];
    float toast_timer;
    int width, height;
    float time;

    /* Settings, persisted beside the worlds. */
    float sensitivity;
    float fov;
    int render_dist;
    bool invert_y;
    bool fullscreen;
    bool show_debug;
    float fps;
    float autosave_timer;
    int settings_choice;

    /* Loose items, a stack held on the inventory cursor, and the open chest. */
    BgDrop drops[MAX_DROPS];
    int drop_count;
    BgStack cursor;
    int open_chest;

    bool renaming;
    char rename_text[WORLD_NAME_MAX];
    float ending_timer;
    AppState settings_return;
} App;

static App g_app;

static void stream_around_player(App *app);
static void quest_objective(const App *app, char *out, size_t size);

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
    if (g_app.state == STATE_WORLDS && g_app.renaming) {
        if (!(isalnum((int)codepoint) || codepoint == ' ' || codepoint == '-' ||
              codepoint == '_')) {
            return;
        }
        const size_t length = strlen(g_app.rename_text);
        if (length < sizeof(g_app.rename_text) - 1) {
            g_app.rename_text[length] = (char)codepoint;
            g_app.rename_text[length + 1] = '\0';
        }
        return;
    }
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

static float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static void set_toast(App *app, const char *text) {
    snprintf(app->toast, sizeof(app->toast), "%s", text);
    app->toast_timer = 2.6f;
}

/* -------------------------------------------------------------- settings */

static void settings_defaults(App *app) {
    app->sensitivity = DEFAULT_SENSITIVITY;
    app->fov = DEFAULT_FOV;
    app->render_dist = DEFAULT_RENDER_RADIUS;
    app->invert_y = false;
    app->fullscreen = false;
    app->show_debug = false;
    app->settings_choice = 0;
}

static void settings_load(App *app) {
    settings_defaults(app);
    FILE *file = fopen(SETTINGS_PATH, "rb");
    if (!file) {
        return;
    }
    char key[32];
    float value = 0.0f;
    while (fscanf(file, "%31s %f", key, &value) == 2) {
        if (strcmp(key, "sensitivity") == 0 && value > 0.0003f && value < 0.02f) {
            app->sensitivity = value;
        } else if (strcmp(key, "fov") == 0 && value > 0.6f && value < 1.9f) {
            app->fov = value;
        } else if (strcmp(key, "render_dist") == 0 && value >= 2.0f && value <= 12.0f) {
            app->render_dist = (int)(value + 0.5f);
        } else if (strcmp(key, "invert_y") == 0) {
            app->invert_y = value != 0.0f;
        } else if (strcmp(key, "fullscreen") == 0) {
            app->fullscreen = value != 0.0f;
        }
    }
    fclose(file);
}

static void settings_save(const App *app) {
    FILE *file = fopen(SETTINGS_PATH, "wb");
    if (!file) {
        return;
    }
    fprintf(file, "sensitivity %.6f\nfov %.5f\nrender_dist %d\ninvert_y %d\nfullscreen %d\n",
            (double)app->sensitivity, (double)app->fov, app->render_dist,
            app->invert_y ? 1 : 0, app->fullscreen ? 1 : 0);
    fclose(file);
}

static void settings_apply_fullscreen(App *app, GLFWwindow *window) {
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        return;
    }
    if (app->fullscreen) {
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        if (mode) {
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                                 mode->refreshRate);
        }
    } else {
        glfwSetWindowMonitor(window, NULL, 80, 60, 1280, 720, GLFW_DONT_CARE);
    }
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
    return button_rect((float)app->width * 0.5f, (float)app->height * 0.41f, 420.0f, 44.0f,
                       10.0f, index);
}

static UiRect pause_button_rect(const App *app, int index) {
    return button_rect((float)app->width * 0.5f, (float)app->height * 0.28f, 470.0f, 44.0f,
                       10.0f, index);
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

static UiRect worlds_delete_rect(const App *app, int index) {
    const UiRect row = worlds_row_rect(app, index);
    UiRect r;
    r.w = 64.0f;
    r.h = 26.0f;
    r.x = row.x + row.w - r.w - 9.0f;
    r.y = row.y + (row.h - r.h) * 0.5f;
    return r;
}

static UiRect worlds_button_rect(const App *app, int index) {
    const float w = 210.0f;
    const float gap = 18.0f;
    const float total = w * 3.0f + gap * 2.0f;
    UiRect r;
    r.x = (float)app->width * 0.5f - total * 0.5f + (float)index * (w + gap);
    r.y = (float)app->height * 0.865f;
    r.w = w;
    r.h = 44.0f;
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
    const int row = index / HOTBAR_SLOTS;
    const int column = index % HOTBAR_SLOTS;
    UiRect r;
    r.x = (float)app->width * 0.5f - total * 0.5f + (float)column * (slot + gap);
    r.y = panel.y + 64.0f + (float)row * (slot + gap);
    r.w = slot;
    r.h = slot;
    return r;
}

/* Recipes share the space left under the pack, so a longer list only gets
 * tighter instead of running into the hint lines. */
static float inventory_row_pitch(const App *app) {
    const float space = inventory_panel_rect(app).h - 252.0f - 60.0f;
    const int count = recipe_count();
    if (count <= 0) {
        return 21.0f;
    }
    float pitch = space / (float)count;
    if (pitch > 21.0f) {
        pitch = 21.0f;
    }
    if (pitch < 15.0f) {
        pitch = 15.0f;
    }
    return pitch;
}

static UiRect inventory_row_rect(const App *app, int index) {
    const UiRect panel = inventory_panel_rect(app);
    const float pitch = inventory_row_pitch(app);
    UiRect r;
    r.x = panel.x + 18.0f;
    r.y = panel.y + 252.0f + (float)index * pitch;
    r.w = panel.w - 36.0f;
    r.h = pitch - 2.0f;
    return r;
}

/* The chest screen: six rows of chest storage above the player's hotbar. */
static UiRect chest_panel_rect(const App *app) {
    UiRect r;
    r.w = 620.0f;
    r.h = 520.0f;
    r.x = ((float)app->width - r.w) * 0.5f;
    r.y = ((float)app->height - r.h) * 0.5f;
    return r;
}

static UiRect chest_slot_rect(const App *app, int index) {
    const float slot = 44.0f;
    const float gap = 5.0f;
    const float total = (float)HOTBAR_SLOTS * slot + (float)(HOTBAR_SLOTS - 1) * gap;
    const UiRect panel = chest_panel_rect(app);
    const int row = index / HOTBAR_SLOTS;
    const int column = index % HOTBAR_SLOTS;
    UiRect r;
    r.x = (float)app->width * 0.5f - total * 0.5f + (float)column * (slot + gap);
    r.y = panel.y + 70.0f + (float)row * (slot + gap);
    r.w = slot;
    r.h = slot;
    return r;
}

static UiRect chest_player_slot_rect(const App *app, int index) {
    UiRect r = chest_slot_rect(app, 0);
    r.x += (float)index * (r.w + 5.0f);
    r.y = chest_panel_rect(app).y + 300.0f;
    return r;
}

static UiRect settings_row_rect(const App *app, int index) {
    UiRect r;
    r.x = (float)app->width * 0.5f - 300.0f;
    r.y = (float)app->height * 0.30f + (float)index * 54.0f;
    r.w = 600.0f;
    r.h = 44.0f;
    return r;
}

static UiRect settings_step_rect(const App *app, int index, bool plus) {
    const UiRect row = settings_row_rect(app, index);
    UiRect r;
    r.w = 38.0f;
    r.h = 30.0f;
    r.x = plus ? row.x + row.w - r.w - 10.0f : row.x + row.w - r.w * 2.0f - 28.0f;
    r.y = row.y + (row.h - r.h) * 0.5f;
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

#define REALM_COUNT 3
static const Dimension kRealms[REALM_COUNT] = { BG_UPPERWORLD, BG_UNDERWORLD, BG_CINDERHOLD };

static int dimension_top(Dimension dimension) {
    switch (dimension) {
        case BG_UPPERWORLD: return UPPERWORLD_TOP;
        case BG_CINDERHOLD: return CINDERHOLD_TOP;
        default:            return UNDERWORLD_TOP;
    }
}

static const char *dimension_title(Dimension dimension) {
    switch (dimension) {
        case BG_UNDERWORLD: return "UNDERWORLD";
        case BG_CINDERHOLD: return "CINDERHOLD";
        default:            return "UPPERWORLD";
    }
}

/* Each world keeps one save per realm, so anything built on the far side of a
 * rift is still standing when the player comes back through. */
static void world_realm_path(int id, Dimension dimension, char *out, size_t size) {
    switch (dimension) {
        case BG_UNDERWORLD:
            snprintf(out, size, "%s/slot_%d_under.sav", WORLD_DIR, id);
            break;
        case BG_CINDERHOLD:
            snprintf(out, size, "%s/slot_%d_cinder.sav", WORLD_DIR, id);
            break;
        default:
            world_save_path(id, out, size);
            break;
    }
}

static bool world_file_exists(int id, Dimension dimension) {
    char path[96];
    world_realm_path(id, dimension, path, sizeof(path));
    FILE *probe = fopen(path, "rb");
    if (!probe) {
        return false;
    }
    fclose(probe);
    return true;
}

static void worlds_write_index(const App *app) {
    FILE *file = fopen(WORLD_INDEX, "wb");
    if (!file) {
        return;
    }
    fprintf(file, "BLOCKFORGE-WORLDS 1\n");
    for (int i = 0; i < app->world_count; ++i) {
        fprintf(file, "%d\t%s\t%llu\t%d\n", app->worlds[i].id, app->worlds[i].name,
                (unsigned long long)app->worlds[i].seed, (int)app->worlds[i].dimension);
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
        int dimension = 0;
        if (sscanf(line, "%d\t%23[^\t]\t%llu\t%d", &id, name, &seed, &dimension) < 2) {
            continue;
        }
        /* A world whose realm saves all went missing is not offered any more. */
        bool any_save = false;
        for (int realm = 0; realm < REALM_COUNT; ++realm) {
            if (world_file_exists(id, kRealms[realm])) {
                any_save = true;
                break;
            }
        }
        if (!any_save) {
            continue;
        }

        WorldEntry *entry = &app->worlds[app->world_count++];
        entry->id = id;
        snprintf(entry->name, sizeof(entry->name), "%s", name);
        entry->seed = (uint64_t)seed;
        entry->dimension = (dimension >= 0 && dimension < REALM_COUNT)
                               ? (Dimension)dimension
                               : BG_UPPERWORLD;
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
    entry->dimension = BG_UPPERWORLD;
    worlds_write_index(app);
}

static int active_world_id(const App *app) {
    if (app->active_world < 0 || app->active_world >= app->world_count) {
        return -1;
    }
    return app->worlds[app->active_world].id;
}

/* Writes the realm the player is standing in; the other realm keeps whatever
 * its own file last saw. */
static void save_active_world(App *app) {
    if (app->active_world < 0 || app->active_world >= app->world_count) {
        return;
    }
    char path[96];
    world_realm_path(app->worlds[app->active_world].id, app->world.dimension, path,
                     sizeof(path));
    save_world(path, &app->world, &app->player);
    WorldEntry *entry = &app->worlds[app->active_world];
    entry->seed = app->world.seed;
    entry->dimension = app->world.dimension;
    worlds_write_index(app);
}

/* Erases a world from the list and deletes both of its realm saves. */
static void delete_world(App *app, int index) {
    if (index < 0 || index >= app->world_count) {
        return;
    }
    char name[WORLD_NAME_MAX];
    snprintf(name, sizeof(name), "%s", app->worlds[index].name);

    char path[96];
    for (int realm = 0; realm < REALM_COUNT; ++realm) {
        world_realm_path(app->worlds[index].id, kRealms[realm], path, sizeof(path));
        remove(path);
    }

    for (int i = index; i + 1 < app->world_count; ++i) {
        app->worlds[i] = app->worlds[i + 1];
    }
    app->world_count--;
    if (app->active_world == index) {
        app->active_world = -1;
    } else if (app->active_world > index) {
        app->active_world--;
    }
    if (app->world_choice >= app->world_count) {
        app->world_choice = app->world_count > 0 ? app->world_count - 1 : 0;
    }
    app->world_delete_confirm = -1;
    worlds_write_index(app);

    char message[96];
    snprintf(message, sizeof(message), "Erased %s.", name);
    set_toast(app, message);
}

/* ------------------------------------------------------------ world tools */

static bool copy_file_bytes(const char *source, const char *target) {
    FILE *in = fopen(source, "rb");
    if (!in) {
        return false;
    }
    FILE *out = fopen(target, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buffer[8192];
    size_t read = 0;
    bool ok = true;
    while ((read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read, out) != read) {
            ok = false;
            break;
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        ok = false;
    }
    return ok;
}

static void finish_rename(App *app) {
    if (!app->renaming) {
        return;
    }
    app->renaming = false;
    if (app->world_choice < 0 || app->world_choice >= app->world_count ||
        app->rename_text[0] == '\0') {
        return;
    }
    snprintf(app->worlds[app->world_choice].name, WORLD_NAME_MAX, "%s", app->rename_text);
    worlds_write_index(app);
    set_toast(app, "World renamed.");
}

static void world_copy(App *app, int index) {
    if (index < 0 || index >= app->world_count) {
        return;
    }
    if (app->world_count >= MAX_WORLDS) {
        set_toast(app, "World list full. Delete one to make room.");
        return;
    }

    const int source_id = app->worlds[index].id;
    const int new_id = worlds_next_id(app);
    int copied = 0;
    for (int realm = 0; realm < REALM_COUNT; ++realm) {
        const Dimension dimension = kRealms[realm];
        char source[96];
        char target[96];
        world_realm_path(source_id, dimension, source, sizeof(source));
        world_realm_path(new_id, dimension, target, sizeof(target));
        FILE *probe = fopen(source, "rb");
        if (!probe) {
            continue;
        }
        fclose(probe);
        if (copy_file_bytes(source, target)) {
            copied++;
        }
    }
    if (copied == 0) {
        set_toast(app, "That world has no save to copy.");
        return;
    }

    WorldEntry *entry = &app->worlds[app->world_count];
    entry->id = new_id;
    char copy_name[WORLD_NAME_MAX];
    snprintf(copy_name, sizeof(copy_name), "%.16s copy", app->worlds[index].name);
    snprintf(entry->name, WORLD_NAME_MAX, "%s", copy_name);
    entry->seed = app->worlds[index].seed;
    entry->dimension = app->worlds[index].dimension;
    app->world_count++;
    app->world_choice = app->world_count - 1;
    worlds_write_index(app);
    set_toast(app, "World copied.");
}

/* One file holding both realms, so a world can leave this machine in one
 * piece and come back as a new slot. */
static void world_export(App *app, int index) {
    if (index < 0 || index >= app->world_count) {
        return;
    }
    char path[96];
    snprintf(path, sizeof(path), "%s/export_%d.bfw", WORLD_DIR, app->worlds[index].id);
    FILE *out = fopen(path, "wb");
    if (!out) {
        set_toast(app, "The export could not be written.");
        return;
    }

    const char magic[8] = { 'B', 'F', 'B', 'U', 'N', 'D', 'L', '2' };
    char name[24] = { 0 };
    snprintf(name, sizeof(name), "%s", app->worlds[index].name);
    const uint64_t seed = app->worlds[index].seed;
    const uint8_t dimension = (uint8_t)app->worlds[index].dimension;
    fwrite(magic, 1, sizeof(magic), out);
    fwrite(name, 1, sizeof(name), out);
    fwrite(&seed, sizeof(seed), 1, out);
    fwrite(&dimension, sizeof(dimension), 1, out);

    for (int realm = 0; realm < REALM_COUNT; ++realm) {
        char realm_path[96];
        world_realm_path(app->worlds[index].id, kRealms[realm], realm_path,
                         sizeof(realm_path));
        FILE *in = fopen(realm_path, "rb");
        const uint8_t present = in ? 1u : 0u;
        fwrite(&present, 1, 1, out);
        if (!in) {
            continue;
        }
        fseek(in, 0, SEEK_END);
        long size = ftell(in);
        fseek(in, 0, SEEK_SET);
        if (size < 0) {
            size = 0;
        }
        const uint32_t length = (uint32_t)size;
        fwrite(&length, sizeof(length), 1, out);
        char buffer[8192];
        size_t read = 0;
        while ((read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            fwrite(buffer, 1, read, out);
        }
        fclose(in);
    }
    const bool ok = ferror(out) == 0;
    fclose(out);
    if (!ok) {
        remove(path);
        set_toast(app, "The export could not be written.");
        return;
    }
    char message[128];
    snprintf(message, sizeof(message), "Exported to %s", path);
    set_toast(app, message);
}

static bool world_import_bundle(App *app, const char *path) {
    if (app->world_count >= MAX_WORLDS) {
        return false;
    }
    FILE *in = fopen(path, "rb");
    if (!in) {
        return false;
    }

    char magic[8];
    char name[24];
    uint64_t seed = 0;
    uint8_t dimension = 0;
    if (fread(magic, 1, sizeof(magic), in) != sizeof(magic) ||
        (memcmp(magic, "BFBUNDL1", 8) != 0 && memcmp(magic, "BFBUNDL2", 8) != 0) ||
        fread(name, 1, sizeof(name), in) != sizeof(name) ||
        fread(&seed, sizeof(seed), 1, in) != 1 ||
        fread(&dimension, sizeof(dimension), 1, in) != 1) {
        fclose(in);
        return false;
    }
    name[sizeof(name) - 1] = '\0';
    if (dimension > (uint8_t)BG_CINDERHOLD) {
        dimension = (uint8_t)BG_UPPERWORLD;
    }
    /* Older bundles hold two realms; the current format holds all three. */
    const int realm_count = memcmp(magic, "BFBUNDL2", 8) == 0 ? 3 : 2;

    const int new_id = worlds_next_id(app);
    bool wrote_any = false;
    for (int realm = 0; realm < realm_count; ++realm) {
        uint8_t present = 0;
        if (fread(&present, 1, 1, in) != 1) {
            break;
        }
        if (!present) {
            continue;
        }
        uint32_t length = 0;
        if (fread(&length, sizeof(length), 1, in) != 1 || length > 200u * 1024u * 1024u) {
            break;
        }
        char target[96];
        world_realm_path(new_id, kRealms[realm], target, sizeof(target));
        FILE *out = fopen(target, "wb");
        if (!out) {
            break;
        }
        char buffer[8192];
        uint32_t remaining = length;
        while (remaining > 0) {
            const size_t want = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            const size_t got = fread(buffer, 1, want, in);
            if (got == 0) {
                break;
            }
            fwrite(buffer, 1, got, out);
            remaining -= (uint32_t)got;
        }
        fclose(out);
        if (remaining != 0) {
            remove(target);
            break;
        }
        wrote_any = true;
    }
    fclose(in);
    if (!wrote_any) {
        return false;
    }

    WorldEntry *entry = &app->worlds[app->world_count++];
    entry->id = new_id;
    snprintf(entry->name, WORLD_NAME_MAX, "%s", name[0] ? name : "Imported");
    entry->seed = seed;
    entry->dimension = (Dimension)dimension;
    app->world_choice = app->world_count - 1;
    worlds_write_index(app);
    return true;
}

static void world_import_scan(App *app) {
    int imported = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(WORLD_DIR "/*.bfw", &data);
    if (handle == INVALID_HANDLE_VALUE) {
        set_toast(app, "No .bfw exports found.");
        return;
    }
    do {
        char path[160];
        snprintf(path, sizeof(path), "%s/%s", WORLD_DIR, data.cFileName);
        if (world_import_bundle(app, path)) {
            char archived[192];
            snprintf(archived, sizeof(archived), "%s.imported", path);
            remove(archived);
            rename(path, archived);
            imported++;
        }
    } while (imported < MAX_WORLDS && FindNextFileA(handle, &data));
    FindClose(handle);
#else
    DIR *dir = opendir(WORLD_DIR);
    if (!dir) {
        set_toast(app, "No .bfw exports found.");
        return;
    }
    struct dirent *entry = NULL;
    while (imported < MAX_WORLDS && (entry = readdir(dir)) != NULL) {
        const size_t length = strlen(entry->d_name);
        if (length < 5 || strcmp(entry->d_name + length - 4, ".bfw") != 0) {
            continue;
        }
        char path[288];
        snprintf(path, sizeof(path), "%s/%s", WORLD_DIR, entry->d_name);
        if (world_import_bundle(app, path)) {
            char archived[304];
            snprintf(archived, sizeof(archived), "%s.imported", path);
            remove(archived);
            rename(path, archived);
            imported++;
        }
    }
    closedir(dir);
#endif
    char message[96];
    if (imported > 0) {
        snprintf(message, sizeof(message), "Imported %d world%s.", imported,
                 imported == 1 ? "" : "s");
    } else {
        snprintf(message, sizeof(message), "No .bfw exports found.");
    }
    set_toast(app, message);
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

/* Clears everything that belongs to one visit to a world rather than to the
 * world itself: loose items, the stack on the cursor, an open chest and the
 * half-dug block. */
static void reset_transient(App *app) {
    memset(app->drops, 0, sizeof(app->drops));
    app->drop_count = 0;
    app->cursor.item = BG_AIR;
    app->cursor.count = 0;
    app->cursor.damage = 0;
    app->open_chest = -1;
    app->show_inventory = false;
    app->mine_progress = 0.0f;
    app->mine_hint_timer = 0.0f;
    app->toast_timer = 0.0f;
    app->toast[0] = '\0';
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

/* The Cinderhold's gate is fixed in the arena's south wall, so the player
 * always lands just inside the arena facing the middle of it. */
static void place_at_cinder_gate(App *app) {
    app->player.vx = app->player.vy = app->player.vz = 0.0f;
    app->player.x = 0.0f;
    app->player.y = (float)CINDERHOLD_FLOOR;
    app->player.z = (float)CINDERHOLD_ARENA_HALF - 2.5f;
    app->player.yaw = -1.5707963f;
    app->player.pitch = 0.0f;
}

/* The pack, the hotbar choice and the health bar are what a realm change
 * carries; where the player stands belongs to the realm being entered. */
static void carry_pack(BgPlayer *to, const BgPlayer *from) {
    memcpy(to->inventory, from->inventory, sizeof(to->inventory));
    to->selected = from->selected;
    to->health = from->health;
}

static void respawn(App *app) {
    app->player.health = 20;
    app->player.invuln = 1.5f;
    app->player.vx = app->player.vy = app->player.vz = 0.0f;
    app->player.fall_distance = 0.0f;
    app->player.drown_timer = 0.0f;
    app->show_inventory = false;
    app->open_chest = -1;
    app->mine_progress = 0.0f;
    /* Anything carried on the cursor goes back in the pack, not into the void. */
    if (app->cursor.count > 0) {
        inventory_add(app->player.inventory, app->cursor.item, app->cursor.count);
        app->cursor.item = BG_AIR;
        app->cursor.count = 0;
        app->cursor.damage = 0;
    }
    if (app->world.dimension == BG_CINDERHOLD) {
        place_at_cinder_gate(app);
        set_toast(app, "The Cinderhold spits you back to its gate.");
        mobs_clear(&app->mobs);
        reset_streaming(app);
        stream_around_player(app);
        return;
    }
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
    const int rows = dimension_top(app->world.dimension) / CHUNK_SIZE;
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

    const int radius = app->render_dist;
    const size_t span = (size_t)(radius * 2 + 3);
    if (app->tracked_count + span * span > MAX_TRACKED_COLUMNS) {
        reset_streaming(app);
        app->last_cx = cx;
        app->last_cz = cz;
    }

    for (int dz = -radius - 1; dz <= radius + 1; ++dz) {
        for (int dx = -radius - 1; dx <= radius + 1; ++dx) {
            ensure_column(app, cx + dx, cz + dz);
        }
    }

    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const uint64_t key = column_key(cx + dx, cz + dz);
            if (column_tracked(app, key)) {
                continue;
            }
            track_column(app, key);
            mesh_column(app, cx + dx, cz + dz);
        }
    }
}

/* A block only changes faces in its own chunk and the neighbours touching it,
 * which is at most eight chunks rather than a full 3x3x3 sweep. */
static void remesh_near(App *app, int x, int y, int z) {
    int chunk_x[8], chunk_y[8], chunk_z[8];
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        const int by = y + dy;
        if (by < 0 || by >= UNDERWORLD_TOP) {
            continue;
        }
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int cx = (x + dx) >> 4;
                const int cy = by >> 4;
                const int cz = (z + dz) >> 4;
                bool seen = false;
                for (int i = 0; i < count; ++i) {
                    if (chunk_x[i] == cx && chunk_y[i] == cy && chunk_z[i] == cz) {
                        seen = true;
                        break;
                    }
                }
                if (!seen && count < 8) {
                    chunk_x[count] = cx;
                    chunk_y[count] = cy;
                    chunk_z[count] = cz;
                    count++;
                }
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        Chunk *chunk = world_chunk(&app->world, chunk_x[i], chunk_y[i], chunk_z[i], false);
        if (!chunk) {
            continue;
        }
        BgMesh mesh;
        mesh_chunk(&app->world, chunk, &mesh);
        renderer_upload_chunk(&app->renderer, chunk_key_of(chunk_x[i], chunk_y[i], chunk_z[i]),
                              &mesh);
        mesh_free(&mesh);
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
    reset_transient(app);

    if (dimension == BG_UPPERWORLD) {
        world_spawn_site(&app->world, &app->loading_center_x, &app->loading_center_z);
    } else {
        app->loading_center_x = 0;
        app->loading_center_z = 0;
    }
    app->loading_dimension = dimension;
    app->loading_radius = app->render_dist + 1;
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
    if (app->loading_dimension == BG_UPPERWORLD) {
        app->player.x = (float)app->loading_center_x + 0.5f;
        app->player.z = (float)app->loading_center_z + 0.5f;
        app->player.y = (float)world_surface_y(&app->world, app->loading_center_x,
                                               app->loading_center_z);
        if (app->player.y < 2.0f) {
            app->player.y = 40.0f;
        }
        app->player.yaw = 1.5707963f;
        app->player.pitch = 0.0f;
    } else if (app->loading_dimension == BG_CINDERHOLD) {
        cinderhold_build_arena(&app->world);
        app->world.cinder_open = true;
        place_at_cinder_gate(app);
    } else {
        /* Arriving in the rift realm means the twin gate came along; it is also
         * the only way back out of a world that starts here. */
        int surface = world_surface_y(&app->world, 0, 0);
        if (surface < 2) {
            surface = 60;
        }
        rift_build(&app->world, 0, surface, 0, 0);
        place_at_rift(app, 0, surface, 0, 0);
    }
    app->travel_armed = false;
    mobs_clear(&app->mobs);

    stream_around_player(app);
    /* A world exists on disk from the moment it is playable. */
    save_active_world(app);
    app->state = STATE_PLAYING;
    set_cursor_captured(window, true);
}

static void start_new_world(App *app, GLFWwindow *window) {
    (void)window;
    if (app->world_count >= MAX_WORLDS) {
        set_toast(app, "World list full. Delete one to make room.");
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
    entry->dimension = app->create_dimension;
    app->active_world = app->world_count;
    app->world_count++;
    worlds_write_index(app);

    world_set_seed(&app->world, seed);
    begin_loading(app, app->create_dimension);
}

static bool load_game(App *app, GLFWwindow *window) {
    if (app->world_choice < 0 || app->world_choice >= app->world_count) {
        return false;
    }

    const int index = app->world_choice;
    char path[96];
    const int realm_id = app->worlds[index].id;
    const Dimension listed = app->worlds[index].dimension;
    world_realm_path(realm_id, listed, path, sizeof(path));
    bool restored = load_world(path, &app->world, &app->player) &&
                    app->world.dimension == listed;
    /* Another realm may be the only one saved (or the listed one is gone). */
    for (int realm = 0; !restored && realm < REALM_COUNT; ++realm) {
        if (kRealms[realm] == listed) {
            continue;
        }
        world_realm_path(realm_id, kRealms[realm], path, sizeof(path));
        restored = load_world(path, &app->world, &app->player) &&
                   app->world.dimension == kRealms[realm];
    }
    if (!restored) {
        return false;
    }

    app->active_world = index;
    app->worlds[index].seed = app->world.seed;
    app->worlds[index].dimension = app->world.dimension;
    worlds_write_index(app);

    renderer_set_environment(&app->renderer, app->world.dimension);
    reset_streaming(app);
    reset_transient(app);
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

    /* Both realms keep their own save, so the far side is restored instead of
     * being generated over: bases, bridges and tunnels are all still there. */
    const BgPlayer carried = app->player;
    const uint64_t seed = app->world.seed;
    save_active_world(app);

    bool restored = false;
    const int id = active_world_id(app);
    if (id >= 0) {
        char path[96];
        world_realm_path(id, next, path, sizeof(path));
        restored = load_world(path, &app->world, &app->player) &&
                   app->world.dimension == next;
    }
    if (restored) {
        /* Terrain comes from the file, but the pack on your back is the one
         * you just carried through the gate. */
        app->player = carried;
    } else {
        /* A realm is being seen for the first time: keep the quest and the
         * clock, which belong to the world rather than to one side of it. */
        const uint8_t quest = app->world.quest_stage;
        const bool boss = app->world.boss_defeated;
        const float clock = app->world.time_of_day;
        world_clear(&app->world);
        world_set_dimension(&app->world, next);
        world_set_seed(&app->world, seed);
        app->world.quest_stage = quest;
        app->world.boss_defeated = boss;
        app->world.time_of_day = clock;
        world_generate_around(&app->world, cx / CHUNK_SIZE, cz / CHUNK_SIZE,
                              app->render_dist + 1);
    }
    renderer_set_environment(&app->renderer, next);
    reset_streaming(app);
    reset_transient(app);

    rift_build(&app->world, cx, cy, cz, axis);
    place_at_rift(app, cx, cy, cz, axis);
    app->travel_armed = false;
    mobs_clear(&app->mobs);

    stream_around_player(app);
    save_active_world(app);
    set_toast(app, next == BG_UNDERWORLD ? "The Underworld swallows you."
                                        : "You step back into the open.");
}

/* The Emberheart's gate always opens onto the arena's own gate, so a fresh
 * Cinderhold is built around it and a saved one is restored as it was. */
static void travel_to_cinderhold(App *app) {
    const BgPlayer carried = app->player;
    const uint64_t seed = app->world.seed;
    save_active_world(app);

    bool restored = false;
    const int id = active_world_id(app);
    if (id >= 0) {
        char path[96];
        world_realm_path(id, BG_CINDERHOLD, path, sizeof(path));
        restored = load_world(path, &app->world, &app->player) &&
                   app->world.dimension == BG_CINDERHOLD;
    }
    if (restored) {
        carry_pack(&app->player, &carried);
    } else {
        const uint8_t quest = app->world.quest_stage;
        const bool boss = app->world.boss_defeated;
        const float clock = app->world.time_of_day;
        world_clear(&app->world);
        world_set_dimension(&app->world, BG_CINDERHOLD);
        world_set_seed(&app->world, seed);
        app->world.quest_stage = quest;
        app->world.boss_defeated = boss;
        app->world.time_of_day = clock;
        app->world.cinder_open = true;
        world_generate_around(&app->world, 0, 0, app->render_dist + 1);
        cinderhold_build_arena(&app->world);
    }

    renderer_set_environment(&app->renderer, BG_CINDERHOLD);
    reset_streaming(app);
    reset_transient(app);
    place_at_cinder_gate(app);
    app->world.cinder_open = true;
    app->travel_armed = false;
    mobs_clear(&app->mobs);
    stream_around_player(app);
    save_active_world(app);
    set_toast(app, "The Cinderhold closes around you.");
}

/* Stepping back through the arena gate returns to the Underworld, landing
 * beside the frame the Emberheart was struck against. */
/* True when the frame in front of the player already burns with the
 * Emberheart's own fire, which must not be paved over with a rift portal. */
static bool frame_has_cinder_gate(const World *world, int cx, int cy, int cz, int axis) {
    for (int iy = 1; iy <= 3; ++iy) {
        for (int id = 1; id <= 2; ++id) {
            const int bx = axis == 0 ? cx + id : cx;
            const int bz = axis == 0 ? cz : cz + id;
            if (block_cinder_gate(world_get_block(world, bx, cy + iy, bz))) {
                return true;
            }
        }
    }
    return false;
}

static void travel_from_cinderhold(App *app) {
    const BgPlayer carried = app->player;
    const uint64_t seed = app->world.seed;
    const uint8_t quest = app->world.quest_stage;
    const bool boss = app->world.boss_defeated;
    const float clock = app->world.time_of_day;
    save_active_world(app);

    bool restored = false;
    const int id = active_world_id(app);
    if (id >= 0) {
        char path[96];
        world_realm_path(id, BG_UNDERWORLD, path, sizeof(path));
        restored = load_world(path, &app->world, &app->player) &&
                   app->world.dimension == BG_UNDERWORLD;
    }
    if (restored) {
        carry_pack(&app->player, &carried);
        /* The save holds the spot on the gate they left from. */
        int cx = 0;
        int cy = 0;
        int cz = 0;
        int axis = 0;
        const int px = (int)floorf(app->player.x);
        const int py = (int)floorf(app->player.y);
        const int pz = (int)floorf(app->player.z);
        if (rift_frame_at(&app->world, px, py, pz, &cx, &cy, &cz, &axis) ||
            rift_frame_at(&app->world, px, py + 1, pz, &cx, &cy, &cz, &axis)) {
            /* The gate they made is left burning; only an ordinary rift frame
             * is rebuilt, which also clears a landing space beside it. */
            if (!frame_has_cinder_gate(&app->world, cx, cy, cz, axis)) {
                rift_build(&app->world, cx, cy, cz, axis);
            }
            place_at_rift(app, cx, cy, cz, axis);
        }
    } else {
        world_clear(&app->world);
        world_set_dimension(&app->world, BG_UNDERWORLD);
        world_set_seed(&app->world, seed);
        app->world.quest_stage = quest;
        app->world.boss_defeated = boss;
        app->world.time_of_day = clock;
        world_generate_around(&app->world, 0, 0, app->render_dist + 1);
        int surface = world_surface_y(&app->world, 0, 0);
        if (surface < 2) {
            surface = 60;
        }
        rift_build(&app->world, 0, surface, 0, 0);
        place_at_rift(app, 0, surface, 0, 0);
    }

    renderer_set_environment(&app->renderer, BG_UNDERWORLD);
    reset_streaming(app);
    reset_transient(app);
    app->travel_armed = false;
    mobs_clear(&app->mobs);
    stream_around_player(app);
    save_active_world(app);
    set_toast(app, "You step back into the Underworld.");
}

static void save_and_title(App *app, GLFWwindow *window) {
    save_active_world(app);
    app->show_inventory = false;
    app->open_chest = -1;
    app->mine_progress = 0.0f;
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

    const char *labels[4] = { "PLAY", "NEW WORLD", "SETTINGS", "QUIT GAME" };
    for (int i = 0; i < 4; ++i) {
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
        ui_text_centered(&ui, width * 0.5f, height * 0.41f + 4.0f * 54.0f + 10.0f, 1.5f, line,
                         count_color);
    }

    ui_text_centered(&ui, width * 0.5f, height - 40.0f, 1.5f,
                     "WASD MOVE   SPACE JUMP   LMB MINE   RMB BUILD   E INVENTORY   F EAT   Q THROW",
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
        ui_text(&ui, r.x + r.w - 86.0f - ui_text_width(seed, 1.5f), r.y + 16.0f, 1.5f, seed,
                muted);

        /* First click arms the button, the second one erases the world. */
        const UiRect del = worlds_delete_rect(app, i);
        const bool armed = (app->world_delete_confirm == i);
        const float del_back[4] = { armed ? 0.62f : 0.22f, armed ? 0.15f : 0.23f,
                                    armed ? 0.16f : 0.26f, 1.0f };
        ui_rect(&ui, del.x, del.y, del.w, del.h, del_back);
        const float del_edge[4] = { armed ? 0.98f : 0.52f, armed ? 0.44f : 0.54f,
                                    armed ? 0.40f : 0.58f, 1.0f };
        ui_rect(&ui, del.x, del.y, del.w, 2.0f, del_edge);
        ui_rect(&ui, del.x, del.y + del.h - 2.0f, del.w, 2.0f, del_edge);
        const float del_text[4] = { armed ? 1.0f : 0.84f, armed ? 0.64f : 0.86f,
                                    armed ? 0.60f : 0.90f, 1.0f };
        ui_text_centered(&ui, del.x + del.w * 0.5f, del.y + (del.h - 7.0f * 1.4f) * 0.5f, 1.4f,
                         armed ? "SURE?" : "DELETE", del_text);
    }

    for (int i = 0; i < 3; ++i) {
        const UiRect r = worlds_button_rect(app, i);
        draw_button(&ui, r.x, r.y, r.w, r.h, 2.0f,
                    i == 0 ? "NEW WORLD" : (i == 1 ? "IMPORT .BFW" : "BACK"), false, true);
    }

    if (app->renaming) {
        char line[64];
        snprintf(line, sizeof(line), "NAME  %s_", app->rename_text);
        const UiRect field = create_field_rect(app, 0);
        draw_field(&ui, field.x, field.y, field.w, field.h, 2.0f, line, true);
    }

    ui_text_centered(&ui, width * 0.5f, height - 52.0f, 1.5f,
                     "CLICK A WORLD TO PLAY IT   DELETE ERASES IT (TWO CLICKS)", muted);
    ui_text_centered(&ui, width * 0.5f, height - 30.0f, 1.5f,
                     "N RENAME   C COPY   X EXPORT .BFW   I IMPORT   ESC BACK", muted);

    if (app->toast_timer > 0.0f) {
        ui_text_centered(&ui, width * 0.5f, height - 78.0f, 1.7f, app->toast, title);
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

    const char *labels[4] = { "BACK TO GAME", "SAVE AND QUIT TO TITLE", "SETTINGS",
                              "QUIT TO DESKTOP" };
    for (int i = 0; i < 4; ++i) {
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

static void render_settings(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    menu_background(&ui, width, height);

    const float title[4] = { 0.98f, 0.90f, 0.62f, 1.0f };
    const float text_color[4] = { 0.92f, 0.94f, 0.98f, 1.0f };
    const float muted[4] = { 0.62f, 0.66f, 0.74f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.13f, 4.0f, "SETTINGS", title);

    char values[5][32];
    snprintf(values[0], sizeof(values[0]), "%.4f", (double)app->sensitivity);
    snprintf(values[1], sizeof(values[1]), "%d DEGREES", (int)(app->fov * 57.2958f + 0.5f));
    snprintf(values[2], sizeof(values[2]), "%d CHUNKS", app->render_dist);
    snprintf(values[3], sizeof(values[3]), "%s", app->invert_y ? "ON" : "OFF");
    snprintf(values[4], sizeof(values[4]), "%s", app->fullscreen ? "ON" : "OFF");
    const char *labels[5] = { "MOUSE SENSITIVITY", "FIELD OF VIEW", "RENDER DISTANCE",
                              "INVERT LOOK", "FULLSCREEN" };

    for (int i = 0; i < 5; ++i) {
        const UiRect r = settings_row_rect(app, i);
        const bool selected = (i == app->settings_choice);
        const float border[4] = { 0.05f, 0.05f, 0.06f, 0.95f };
        ui_rect(&ui, r.x - 3.0f, r.y - 3.0f, r.w + 6.0f, r.h + 6.0f, border);
        const float fill[4] = { selected ? 0.40f : 0.26f, selected ? 0.41f : 0.27f,
                                selected ? 0.45f : 0.29f, 1.0f };
        ui_rect(&ui, r.x, r.y, r.w, r.h, fill);
        ui_text(&ui, r.x + 18.0f, r.y + 12.0f, 2.0f, labels[i],
                selected ? title : text_color);
        /* The value sits in the gap left of the -/+ steppers. */
        ui_text(&ui, r.x + r.w - 118.0f - ui_text_width(values[i], 1.8f), r.y + 13.0f, 1.8f,
                values[i], selected ? text_color : muted);

        const UiRect minus = settings_step_rect(app, i, false);
        const UiRect plus = settings_step_rect(app, i, true);
        draw_button(&ui, minus.x, minus.y, minus.w, minus.h, 1.8f, "-", false, true);
        draw_button(&ui, plus.x, plus.y, plus.w, plus.h, 1.8f, "+", false, true);
    }

    ui_text_centered(&ui, width * 0.5f, height - 70.0f, 1.6f,
                     "UP/DOWN SELECTS   LEFT/RIGHT CHANGES   ESC SAVES AND GOES BACK", muted);
    if (app->toast_timer > 0.0f) {
        ui_text_centered(&ui, width * 0.5f, height - 100.0f, 1.6f, app->toast, title);
    }

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_ending(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    const float dim[4] = { 0.02f, 0.01f, 0.01f, 0.86f };
    ui_rect(&ui, 0.0f, 0.0f, width, height, dim);

    const float glow[4] = { 1.0f, 0.78f, 0.34f, 0.22f };
    ui_rect(&ui, width * 0.5f - 300.0f, height * 0.26f, 600.0f, 4.0f, glow);
    ui_rect(&ui, width * 0.5f - 300.0f, height * 0.26f + 200.0f, 600.0f, 4.0f, glow);

    const float title[4] = { 1.0f, 0.86f, 0.42f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.30f, 5.0f, "THE EMBERHEART IS YOURS", title);
    const float soft[4] = { 0.90f, 0.90f, 0.94f, 0.95f };
    draw_shadowed_centered(&ui, width * 0.5f, height * 0.42f, 2.2f,
                           "THE CINDER ALPHA HAS FALLEN AND BOTH REALMS REMEMBER YOU.", soft);
    ui_text_centered(&ui, width * 0.5f, height * 0.455f, 1.5f,
                     "STRIKE THE EMBERHEART ON A RIFT FRAME BELOW TO OPEN THE CINDERHOLD.",
                     title);

    char stats[128];
    snprintf(stats, sizeof(stats), "%s   %d CHUNKS        SEED %llu",
             dimension_name(app->world.dimension), (int)app->world.count,
             (unsigned long long)app->world.seed);
    ui_text_centered(&ui, width * 0.5f, height * 0.50f, 1.8f, stats, soft);
    ui_text_centered(&ui, width * 0.5f, height * 0.58f, 1.8f,
                     "KEEP BUILDING AS LONG AS YOU LIKE. PRESS ENTER.", title);

    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void render_debug(App *app) {
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const uint8_t light = world_light_at(&app->world, (int)floorf(app->player.x),
                                         (int)floorf(app->player.y + 1.0f),
                                         (int)floorf(app->player.z));
    char lines[5][128];
    snprintf(lines[0], sizeof(lines[0]), "%.0f FPS   %d DRAW CALLS",
             app->fps > 0.0f ? 1.0f / app->fps : 0.0f, app->renderer.draw_calls);
    snprintf(lines[1], sizeof(lines[1]), "X %.1f  Y %.1f  Z %.1f  %s", app->player.x,
             app->player.y, app->player.z, dimension_name(app->world.dimension));
    snprintf(lines[2], sizeof(lines[2]), "%d CHUNKS LOADED   LIGHT %d/%d",
             (int)app->world.count, light >> 4, light & 15);
    snprintf(lines[3], sizeof(lines[3]), "%d MOBS   %d DROPS   SEED %llu", app->mobs.count,
             app->drop_count, (unsigned long long)app->world.seed);

    char objective[128];
    if (app->world.dimension == BG_CINDERHOLD) {
        snprintf(objective, sizeof(objective), "%s",
                 app->world.cinder_boss_defeated
                     ? "THE CINDERHOLD  -  THE SOVEREIGN FALLS"
                     : "THE CINDERHOLD  -  SLAY THE CINDER SOVEREIGN");
    } else {
        quest_objective(app, objective, sizeof(objective));
    }

    const float back[4] = { 0.02f, 0.02f, 0.03f, 0.62f };
    ui_rect(&ui, 10.0f, 10.0f, 470.0f, 132.0f, back);
    const float text_color[4] = { 0.86f, 0.94f, 0.86f, 1.0f };
    for (int i = 0; i < 4; ++i) {
        ui_text(&ui, 20.0f, 20.0f + (float)i * 17.0f, 1.6f, lines[i], text_color);
    }
    const float quest_color[4] = { 0.96f, 0.82f, 0.42f, 1.0f };
    ui_text(&ui, 20.0f, 92.0f, 1.5f, objective, quest_color);
    ui_text(&ui, 20.0f, 112.0f, 1.4f, "F3 HIDES THIS OVERLAY",
            (float[4]){ 0.60f, 0.66f, 0.62f, 1.0f });

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

/* ------------------------------------------------------------- quest */

static void quest_objective(const App *app, char *out, size_t size) {
    const int shards = inventory_count(app->player.inventory, BG_ITEM_EMBER_SHARD);
    switch (app->world.quest_stage) {
        case 0:
            snprintf(out, size, "THE EMBERHEART  -  GATHER EMBER SHARDS (%d/5)",
                     shards > 5 ? 5 : shards);
            break;
        case 1:
            snprintf(out, size, "THE EMBERHEART  -  CRAFT A RIFT KEY AT A TABLE");
            break;
        case 2:
            snprintf(out, size, "THE EMBERHEART  -  SLAY THE CINDER ALPHA BELOW");
            break;
        case 3:
            snprintf(out, size, "THE EMBERHEART  -  CARRY IT BACK THROUGH THE RIFT");
            break;
        default:
            if (!app->world.cinder_open) {
                snprintf(out, size, "THE EMBERHEART  -  STRIKE IT ON A RIFT FRAME BELOW");
            } else if (!app->world.cinder_boss_defeated) {
                snprintf(out, size, "THE EMBERHEART  -  THE CINDERHOLD AWAITS");
            } else {
                snprintf(out, size, "THE EMBERHEART  -  YOURS");
            }
            break;
    }
}

/* Advances the tutorial chain when its condition is met. */
static void quest_check(App *app) {
    if (app->world.quest_stage == 0 &&
        inventory_count(app->player.inventory, BG_ITEM_EMBER_SHARD) >= 5) {
        app->world.quest_stage = 1;
        set_toast(app, "The shards hum together. A key could focus them.");
        audio_play(BG_SOUND_CRAFT);
    }
}

/* The pack and the recipe list share one screen: three rows of slots, hearts
 * under them, then every recipe with its output icon. */
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

    for (int i = 0; i < INVENTORY_SLOTS; ++i) {
        const UiRect r = inventory_slot_rect(app, i);
        const bool selected = (i == app->player.selected);
        const bool hotbar = i < HOTBAR_SLOTS;
        const float cell[4] = { hotbar ? 0.12f : 0.08f, hotbar ? 0.12f : 0.08f,
                                hotbar ? 0.13f : 0.09f, 0.96f };
        ui_rect(&ui, r.x, r.y, r.w, r.h, cell);
        const float bevel[4] = { 0.30f, 0.31f, 0.34f, 0.95f };
        ui_rect(&ui, r.x, r.y, r.w, 2.0f, bevel);
        ui_rect(&ui, r.x, r.y, 2.0f, r.h, bevel);

        const BgStack *stack = &app->player.inventory[i];
        if (stack->count > 0) {
            ui_draw_item_icon(&ui, r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, stack->item, 1.0f);
        }

        char key[4];
        if (hotbar) {
            snprintf(key, sizeof(key), "%d", i + 1);
        } else {
            snprintf(key, sizeof(key), " ");
        }
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

        const int wear = item_durability_max(stack->item);
        if (wear > 0 && stack->damage > 0) {
            const float left = 1.0f - (float)stack->damage / (float)wear;
            const float wear_color[4] = { clampf(1.4f - left * 1.4f, 0.0f, 1.0f),
                                          clampf(0.5f + left * 0.9f, 0.0f, 1.0f), 0.18f,
                                          0.98f };
            ui_rect(&ui, r.x + 4.0f, r.y + r.h - 7.0f, r.w - 8.0f, 3.0f,
                    (float[4]){ 0.05f, 0.05f, 0.06f, 0.9f });
            ui_rect(&ui, r.x + 4.0f, r.y + r.h - 7.0f, (r.w - 8.0f) * left, 3.0f, wear_color);
        }

        if (selected) {
            const float frame[4] = { 0.98f, 0.98f, 0.94f, 0.95f };
            ui_rect(&ui, r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, 2.0f, frame);
            ui_rect(&ui, r.x - 2.0f, r.y + r.h, r.w + 4.0f, 2.0f, frame);
            ui_rect(&ui, r.x - 2.0f, r.y - 2.0f, 2.0f, r.h + 4.0f, frame);
            ui_rect(&ui, r.x + r.w, r.y - 2.0f, 2.0f, r.h + 4.0f, frame);
        }
    }

    ui_draw_hearts(&ui, panel.x + 24.0f, panel.y + 214.0f, 1.8f, app->player.health, 10);

    const BgStack *held = &app->player.inventory[app->player.selected];
    if (held->count > 0) {
        char held_line[64];
        snprintf(held_line, sizeof(held_line), "%s", item_name(held->item));
        ui_text_centered(&ui, width * 0.5f, panel.y + 214.0f, 1.5f, held_line, muted);
    }

    const float sub[4] = { 0.86f, 0.82f, 0.60f, 0.95f };
    ui_text(&ui, panel.x + 20.0f, panel.y + 236.0f, 1.6f, "CRAFTING", sub);

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

        ui_draw_item_icon(&ui, r.x + 6.0f, r.y + 1.0f, r.h - 2.0f, recipe->output.item,
                          can ? 1.0f : 0.55f);

        char line[128];
        recipe_line(recipe, line, sizeof(line));
        const float color[4] = { can ? 0.86f : 0.46f, can ? 0.92f : 0.48f,
                                 can ? 0.98f : 0.54f, 1.0f };
        ui_text(&ui, r.x + 30.0f, r.y + 2.0f, 1.6f, line, color);
    }

    ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 42.0f, 1.5f,
                     "CLICK A RECIPE TO CRAFT   CLICK AN ITEM TO CARRY IT, CLICK AGAIN TO DROP IT",
                     text_color);
    ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 18.0f, 1.5f,
                     "RIGHT CLICK SPLITS   Q THROWS   R REPAIRS A HELD TOOL   E CLOSES", muted);

    if (app->cursor.count > 0) {
        ui_draw_item_icon(&ui, app->mouse_x - 18.0f, app->mouse_y - 18.0f, 36.0f,
                          app->cursor.item, 1.0f);
        if (app->cursor.count > 1) {
            char count[8];
            snprintf(count, sizeof(count), "%u", (unsigned)app->cursor.count);
            ui_text(&ui, app->mouse_x + 2.0f, app->mouse_y - 2.0f, 1.5f, count, text_color);
        }
    }

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

/* Click-to-carry: pick a stack up, then click a slot to put it down. */
static void inventory_click(App *app, BgStack *slot, bool right) {
    BgStack *cursor = &app->cursor;
    if (right) {
        if (cursor->count == 0) {
            if (slot->count > 0) {
                const int half = slot->count > 1 ? slot->count / 2 : 1;
                cursor->item = slot->item;
                cursor->damage = 0;
                cursor->count = (uint8_t)half;
                slot->count = (uint8_t)(slot->count - half);
                if (slot->count == 0) {
                    slot->item = BG_AIR;
                    slot->damage = 0;
                }
            }
        } else if (slot->count == 0) {
            *slot = *cursor;
            slot->count = 1;
            cursor->count--;
            if (cursor->count == 0) {
                cursor->item = BG_AIR;
                cursor->damage = 0;
            }
        } else if (slot->item == cursor->item &&
                   slot->count < item_max_stack(slot->item)) {
            slot->count++;
            cursor->count--;
            if (cursor->count == 0) {
                cursor->item = BG_AIR;
                cursor->damage = 0;
            }
        }
        return;
    }

    if (cursor->count == 0) {
        if (slot->count > 0) {
            *cursor = *slot;
            slot->item = BG_AIR;
            slot->count = 0;
            slot->damage = 0;
        }
    } else if (slot->count == 0) {
        *slot = *cursor;
        cursor->item = BG_AIR;
        cursor->count = 0;
        cursor->damage = 0;
    } else if (slot->item == cursor->item && slot->damage == cursor->damage &&
               slot->count + cursor->count <= item_max_stack(slot->item)) {
        slot->count = (uint8_t)(slot->count + cursor->count);
        cursor->item = BG_AIR;
        cursor->count = 0;
        cursor->damage = 0;
    } else {
        const BgStack swap = *slot;
        *slot = *cursor;
        *cursor = swap;
    }
}

static void stash_cursor(App *app) {
    if (app->cursor.count == 0) {
        return;
    }
    if (!inventory_add(app->player.inventory, app->cursor.item, app->cursor.count)) {
        world_drop_item(app->drops, &app->drop_count, &app->player, app->cursor.item,
                        app->cursor.count);
    }
    app->cursor.item = BG_AIR;
    app->cursor.count = 0;
    app->cursor.damage = 0;
}

static void throw_selected(App *app) {
    BgStack *stack = &app->player.inventory[app->player.selected];
    if (stack->count == 0) {
        return;
    }
    if (world_drop_item(app->drops, &app->drop_count, &app->player, stack->item, 1)) {
        stack->count--;
        if (stack->count == 0) {
            stack->item = BG_AIR;
            stack->damage = 0;
        }
        audio_play(BG_SOUND_PICKUP);
    }
}

/* R at the inventory mends the held tool with its own material. */
static void repair_selected(App *app) {
    BgStack *stack = app->cursor.count > 0 ? &app->cursor
                                           : &app->player.inventory[app->player.selected];
    const int maximum = item_durability_max(stack->item);
    if (maximum <= 0) {
        set_toast(app, "Hold a damaged pickaxe to repair it.");
        return;
    }
    if (stack->damage == 0) {
        set_toast(app, "That tool is already sound.");
        return;
    }

    uint16_t material = BG_ITEM_EMBER_SHARD;
    if (stack->item == BG_ITEM_WOOD_PICK) {
        material = BG_PLANKS;
    } else if (stack->item == BG_ITEM_STONE_PICK) {
        material = BG_STONE;
    }
    if (inventory_count(app->player.inventory, material) <= 0) {
        char message[96];
        snprintf(message, sizeof(message), "Repairing needs %s.", item_name(material));
        set_toast(app, message);
        return;
    }
    inventory_remove(app->player.inventory, material, 1);
    const int mend = maximum / 3 + 1;
    stack->damage = stack->damage > mend ? (uint8_t)(stack->damage - mend) : 0;
    set_toast(app, "Tool repaired.");
    audio_play(BG_SOUND_CRAFT);
}

static void handle_inventory_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_E) || key_pressed(app, GLFW_KEY_C) ||
        key_pressed(app, GLFW_KEY_ESCAPE)) {
        stash_cursor(app);
        app->show_inventory = false;
        set_cursor_captured(window, true);
        return;
    }

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        if (key_pressed(app, GLFW_KEY_1 + i)) {
            app->player.selected = i;
        }
    }

    if (key_pressed(app, GLFW_KEY_Q)) {
        if (app->cursor.count > 0) {
            world_drop_item(app->drops, &app->drop_count, &app->player, app->cursor.item,
                            app->cursor.count);
            app->cursor.item = BG_AIR;
            app->cursor.count = 0;
            app->cursor.damage = 0;
        } else {
            throw_selected(app);
        }
    }
    if (key_pressed(app, GLFW_KEY_R)) {
        repair_selected(app);
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
            quest_check(app);
        }
    }

    if (app->input.mouse[0] || app->input.mouse[1]) {
        const bool right = app->input.mouse[1];
        for (int i = 0; i < INVENTORY_SLOTS; ++i) {
            if (ui_hit(inventory_slot_rect(app, i), app->mouse_x, app->mouse_y)) {
                inventory_click(app, &app->player.inventory[i], right);
                return;
            }
        }
        if (right) {
            return;
        }
        for (int i = 0; i < recipe_count(); ++i) {
            if (ui_hit(inventory_row_rect(app, i), app->mouse_x, app->mouse_y)) {
                app->craft_choice = i;
                craft_selected(app);
                quest_check(app);
                return;
            }
        }
    }
}

/* --------------------------------------------------------------- chest */

static void render_chest(App *app) {
    if (app->open_chest < 0 || (size_t)app->open_chest >= app->world.container_count) {
        return;
    }
    const BgContainer *container = &app->world.containers[app->open_chest];
    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float width = (float)app->width;
    const float height = (float)app->height;
    const float dim[4] = { 0.02f, 0.02f, 0.03f, 0.78f };
    ui_rect(&ui, 0.0f, 0.0f, width, height, dim);

    const UiRect panel = chest_panel_rect(app);
    const float border[4] = { 0.04f, 0.04f, 0.05f, 0.95f };
    ui_rect(&ui, panel.x - 4.0f, panel.y - 4.0f, panel.w + 8.0f, panel.h + 8.0f, border);
    const float plate[4] = { 0.16f, 0.16f, 0.18f, 0.97f };
    ui_rect(&ui, panel.x, panel.y, panel.w, panel.h, plate);
    const float light[4] = { 0.42f, 0.43f, 0.47f, 0.90f };
    ui_rect(&ui, panel.x, panel.y, panel.w, 3.0f, light);
    ui_rect(&ui, panel.x, panel.y, 3.0f, panel.h, light);

    const float title[4] = { 0.98f, 0.92f, 0.72f, 1.0f };
    const float text_color[4] = { 0.90f, 0.92f, 0.96f, 1.0f };
    const float muted[4] = { 0.60f, 0.64f, 0.72f, 1.0f };
    draw_shadowed_centered(&ui, width * 0.5f, panel.y + 12.0f, 3.0f, "CHEST", title);

    for (int i = 0; i < CHEST_SLOTS; ++i) {
        const UiRect r = chest_slot_rect(app, i);
        const float cell[4] = { 0.10f, 0.10f, 0.11f, 0.96f };
        ui_rect(&ui, r.x, r.y, r.w, r.h, cell);
        const float bevel[4] = { 0.30f, 0.31f, 0.34f, 0.95f };
        ui_rect(&ui, r.x, r.y, r.w, 2.0f, bevel);
        ui_rect(&ui, r.x, r.y, 2.0f, r.h, bevel);

        const BgStack *stack = &container->slots[i];
        if (stack->count > 0) {
            ui_draw_item_icon(&ui, r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, stack->item, 1.0f);
            if (stack->count > 1) {
                char count[8];
                snprintf(count, sizeof(count), "%u", (unsigned)stack->count);
                ui_text(&ui, r.x + r.w - 3.0f - ui_text_width(count, 1.5f), r.y + r.h - 9.0f,
                        1.5f, count, text_color);
            }
        }
    }

    ui_text(&ui, panel.x + 20.0f, panel.y + 272.0f, 1.6f, "YOUR PACK", muted);
    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        const UiRect r = chest_player_slot_rect(app, i);
        const float cell[4] = { 0.13f, 0.13f, 0.14f, 0.96f };
        ui_rect(&ui, r.x, r.y, r.w, r.h, cell);
        const BgStack *stack = &app->player.inventory[i];
        if (stack->count > 0) {
            ui_draw_item_icon(&ui, r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, stack->item, 1.0f);
            if (stack->count > 1) {
                char count[8];
                snprintf(count, sizeof(count), "%u", (unsigned)stack->count);
                ui_text(&ui, r.x + r.w - 3.0f - ui_text_width(count, 1.5f), r.y + r.h - 9.0f,
                        1.5f, count, text_color);
            }
        }
    }

    ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 76.0f, 1.5f,
                     "CLICK A CHEST SLOT TO TAKE IT   CLICK A PACK SLOT TO STORE IT",
                     text_color);
    ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 52.0f, 1.5f,
                     "E OR ESC CLOSES THE CHEST", muted);
    if (app->toast_timer > 0.0f) {
        ui_text_centered(&ui, width * 0.5f, panel.y + panel.h - 100.0f, 1.6f, app->toast,
                         title);
    }

    if (app->cursor.count > 0) {
        ui_draw_item_icon(&ui, app->mouse_x - 18.0f, app->mouse_y - 18.0f, 36.0f,
                          app->cursor.item, 1.0f);
        if (app->cursor.count > 1) {
            char count[8];
            snprintf(count, sizeof(count), "%u", (unsigned)app->cursor.count);
            ui_text(&ui, app->mouse_x + 2.0f, app->mouse_y - 2.0f, 1.5f, count, text_color);
        }
    }
    renderer_draw_ui(&app->renderer, &ui);
    ui_free(&ui);
}

static void chest_store(App *app, BgContainer *container, BgStack *slot) {
    if (slot->count == 0) {
        return;
    }
    const int maximum = item_max_stack(slot->item);
    for (int i = 0; i < CHEST_SLOTS; ++i) {
        BgStack *target = &container->slots[i];
        if (target->count == 0 || target->item != slot->item || target->count >= maximum) {
            continue;
        }
        const int room = maximum - target->count;
        const int moved = room < slot->count ? room : slot->count;
        target->count = (uint8_t)(target->count + moved);
        slot->count = (uint8_t)(slot->count - moved);
        if (slot->count == 0) {
            slot->item = BG_AIR;
            slot->damage = 0;
            return;
        }
    }
    for (int i = 0; i < CHEST_SLOTS; ++i) {
        BgStack *target = &container->slots[i];
        if (target->count != 0) {
            continue;
        }
        *target = *slot;
        slot->item = BG_AIR;
        slot->count = 0;
        slot->damage = 0;
        return;
    }
    set_toast(app, "That chest is full.");
}

static void handle_chest_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_E) || key_pressed(app, GLFW_KEY_C) ||
        key_pressed(app, GLFW_KEY_ESCAPE)) {
        stash_cursor(app);
        app->open_chest = -1;
        set_cursor_captured(window, true);
        return;
    }
    if (app->open_chest < 0 || (size_t)app->open_chest >= app->world.container_count) {
        app->open_chest = -1;
        set_cursor_captured(window, true);
        return;
    }
    BgContainer *container = &app->world.containers[app->open_chest];

    if (app->input.mouse[0]) {
        for (int i = 0; i < CHEST_SLOTS; ++i) {
            if (!ui_hit(chest_slot_rect(app, i), app->mouse_x, app->mouse_y)) {
                continue;
            }
            BgStack *stack = &container->slots[i];
            if (app->cursor.count > 0) {
                /* A stack on the cursor goes into the chest, stacking or swapping. */
                if (stack->count == 0) {
                    *stack = app->cursor;
                    app->cursor.item = BG_AIR;
                    app->cursor.count = 0;
                    app->cursor.damage = 0;
                } else if (stack->item == app->cursor.item &&
                           stack->count < item_max_stack(stack->item)) {
                    const int room = item_max_stack(stack->item) - stack->count;
                    const int moved = room < app->cursor.count ? room : app->cursor.count;
                    stack->count = (uint8_t)(stack->count + moved);
                    app->cursor.count = (uint8_t)(app->cursor.count - moved);
                    if (app->cursor.count == 0) {
                        app->cursor.item = BG_AIR;
                        app->cursor.damage = 0;
                    }
                } else {
                    const BgStack swap = *stack;
                    *stack = app->cursor;
                    app->cursor = swap;
                }
                audio_play(BG_SOUND_CLICK);
            } else if (stack->count > 0 &&
                       inventory_add(app->player.inventory, stack->item, stack->count)) {
                stack->item = BG_AIR;
                stack->count = 0;
                stack->damage = 0;
                audio_play(BG_SOUND_PICKUP);
            }
            return;
        }
        for (int i = 0; i < HOTBAR_SLOTS; ++i) {
            if (!ui_hit(chest_player_slot_rect(app, i), app->mouse_x, app->mouse_y)) {
                continue;
            }
            BgStack *slot = &app->player.inventory[i];
            if (app->cursor.count > 0) {
                inventory_click(app, slot, false);
            } else {
                chest_store(app, container, slot);
            }
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
             dimension_title(app->loading_dimension));
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

    for (int i = 0; i < 4; ++i) {
        if (i == 0 && !has_world) {
            continue;
        }
        if (ui_hit(title_button_rect(app, i), app->mouse_x, app->mouse_y)) {
            app->menu_choice = i;
        }
    }

    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->menu_choice = (app->menu_choice + 3) % 4;
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->menu_choice = (app->menu_choice + 1) % 4;
    }
    if (app->menu_choice == 0 && !has_world) {
        app->menu_choice = 1;
    }

    bool activate = key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER);
    if (app->input.mouse[0]) {
        for (int i = 0; i < 4; ++i) {
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
        audio_play(BG_SOUND_CLICK);
        if (app->menu_choice == 0) {
            app->world_choice = (app->active_world >= 0 && app->active_world < app->world_count)
                                    ? app->active_world
                                    : 0;
            app->state = STATE_WORLDS;
        } else if (app->menu_choice == 1) {
            begin_create(app);
        } else if (app->menu_choice == 2) {
            app->settings_return = STATE_TITLE;
            app->settings_choice = 0;
            app->state = STATE_SETTINGS;
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void handle_settings_input(App *app, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        settings_save(app);
        app->state = app->settings_return;
        if (app->state == STATE_PAUSED || app->state == STATE_PLAYING) {
            set_cursor_captured(window, true);
        }
        return;
    }

    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->settings_choice = (app->settings_choice + 4) % 5;
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->settings_choice = (app->settings_choice + 1) % 5;
    }

    int change = 0;
    if (key_pressed(app, GLFW_KEY_LEFT) || key_pressed(app, GLFW_KEY_A)) {
        change = -1;
    }
    if (key_pressed(app, GLFW_KEY_RIGHT) || key_pressed(app, GLFW_KEY_D)) {
        change = 1;
    }
    bool toggle = key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER) ||
                  key_pressed(app, GLFW_KEY_SPACE);

    if (app->input.mouse[0]) {
        for (int i = 0; i < 5; ++i) {
            if (ui_hit(settings_row_rect(app, i), app->mouse_x, app->mouse_y)) {
                app->settings_choice = i;
            }
            if (ui_hit(settings_step_rect(app, i, false), app->mouse_x, app->mouse_y)) {
                app->settings_choice = i;
                change = -1;
            }
            if (ui_hit(settings_step_rect(app, i, true), app->mouse_x, app->mouse_y)) {
                app->settings_choice = i;
                change = 1;
            }
        }
    }

    const int choice = app->settings_choice;
    if (choice == 0 && change != 0) {
        app->sensitivity = clampf(app->sensitivity + (float)change * 0.0002f, 0.0006f, 0.008f);
    } else if (choice == 1 && change != 0) {
        float degrees = app->fov * 57.2958f + (float)change * 5.0f;
        degrees = clampf(degrees, 55.0f, 100.0f);
        app->fov = degrees / 57.2958f;
    } else if (choice == 2 && change != 0) {
        app->render_dist = app->render_dist + change;
        if (app->render_dist < 2) app->render_dist = 2;
        if (app->render_dist > 12) app->render_dist = 12;
        reset_streaming(app);
        if (app->state == STATE_PLAYING) {
            stream_around_player(app);
        }
    } else if (choice == 3 && (change != 0 || toggle)) {
        app->invert_y = !app->invert_y;
    } else if (choice == 4 && (change != 0 || toggle)) {
        app->fullscreen = !app->fullscreen;
        settings_apply_fullscreen(app, window);
    }
    settings_save(app);
}

static void handle_ending_input(App *app, GLFWwindow *window) {
    const bool go_on = key_pressed(app, GLFW_KEY_ENTER) ||
                       key_pressed(app, GLFW_KEY_KP_ENTER) ||
                       key_pressed(app, GLFW_KEY_ESCAPE) || app->input.mouse[0];
    if (!go_on) {
        return;
    }
    app->state = STATE_PLAYING;
    app->toast_timer = 0.0f;
    set_cursor_captured(window, true);
}

static void handle_worlds_input(App *app, GLFWwindow *window) {
    if (app->renaming) {
        if (key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER)) {
            finish_rename(app);
        } else if (key_pressed(app, GLFW_KEY_ESCAPE)) {
            app->renaming = false;
        } else if (key_pressed(app, GLFW_KEY_BACKSPACE)) {
            const size_t length = strlen(app->rename_text);
            if (length > 0) {
                app->rename_text[length - 1] = '\0';
            }
        }
        return;
    }

    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_TITLE;
        app->menu_choice = 0;
        app->world_delete_confirm = -1;
        return;
    }

    if (app->world_count > 0) {
        if (app->world_choice < 0 || app->world_choice >= app->world_count) {
            app->world_choice = 0;
        }
        if (key_pressed(app, GLFW_KEY_N)) {
            app->renaming = true;
            snprintf(app->rename_text, sizeof(app->rename_text), "%s",
                     app->worlds[app->world_choice].name);
            set_toast(app, "Type a name, then press ENTER.");
            return;
        }
        if (key_pressed(app, GLFW_KEY_C)) {
            world_copy(app, app->world_choice);
            return;
        }
        if (key_pressed(app, GLFW_KEY_X)) {
            world_export(app, app->world_choice);
            return;
        }
    }
    if (key_pressed(app, GLFW_KEY_I)) {
        world_import_scan(app);
        return;
    }

    if (app->world_count > 0 && key_pressed(app, GLFW_KEY_DELETE)) {
        if (app->world_delete_confirm == app->world_choice) {
            delete_world(app, app->world_choice);
        } else {
            app->world_delete_confirm = app->world_choice;
            set_toast(app, "Press DELETE again to erase it.");
        }
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
        for (int i = 0; i < app->world_count; ++i) {
            if (!ui_hit(worlds_delete_rect(app, i), app->mouse_x, app->mouse_y)) {
                continue;
            }
            if (app->world_delete_confirm == i) {
                delete_world(app, i);
            } else {
                app->world_delete_confirm = i;
                set_toast(app, "Click DELETE again to erase it.");
            }
            return;
        }
        app->world_delete_confirm = -1;

        if (ui_hit(worlds_button_rect(app, 0), app->mouse_x, app->mouse_y)) {
            begin_create(app);
            return;
        }
        if (ui_hit(worlds_button_rect(app, 1), app->mouse_x, app->mouse_y)) {
            world_import_scan(app);
            return;
        }
        if (ui_hit(worlds_button_rect(app, 2), app->mouse_x, app->mouse_y)) {
            app->state = STATE_TITLE;
            app->menu_choice = 0;
            app->world_delete_confirm = -1;
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
    for (int i = 0; i < 4; ++i) {
        if (ui_hit(pause_button_rect(app, i), app->mouse_x, app->mouse_y)) {
            app->pause_choice = i;
        }
    }
    if (key_pressed(app, GLFW_KEY_UP) || key_pressed(app, GLFW_KEY_W)) {
        app->pause_choice = (app->pause_choice + 3) % 4;
    }
    if (key_pressed(app, GLFW_KEY_DOWN) || key_pressed(app, GLFW_KEY_S)) {
        app->pause_choice = (app->pause_choice + 1) % 4;
    }
    if (key_pressed(app, GLFW_KEY_ESCAPE)) {
        app->state = STATE_PLAYING;
        set_cursor_captured(window, true);
        return;
    }

    bool activate = key_pressed(app, GLFW_KEY_ENTER) || key_pressed(app, GLFW_KEY_KP_ENTER);
    if (app->input.mouse[0]) {
        for (int i = 0; i < 4; ++i) {
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
    } else if (app->pause_choice == 2) {
        app->settings_return = STATE_PAUSED;
        app->settings_choice = 0;
        app->state = STATE_SETTINGS;
        set_cursor_captured(window, false);
    } else {
        save_active_world(app);
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
    if (type == BG_CHEST) {
        /* A broken chest spills whatever it was holding. */
        BgContainer *container = world_container_at(&app->world, bx, by, bz, false);
        if (container) {
            for (int i = 0; i < CHEST_SLOTS; ++i) {
                BgStack *slot = &container->slots[i];
                if (slot->count == 0) {
                    continue;
                }
                if (!world_drop_item(app->drops, &app->drop_count, &app->player, slot->item,
                                     slot->count)) {
                    inventory_add(app->player.inventory, slot->item, slot->count);
                }
                slot->item = BG_AIR;
                slot->count = 0;
                slot->damage = 0;
            }
        }
        world_container_remove(&app->world, bx, by, bz);
    }
    remesh_near(app, bx, by, bz);
    uint16_t drop_item = BG_AIR;
    int drop_amount = 0;
    if (block_drop(type, &drop_item, &drop_amount)) {
        inventory_add(app->player.inventory, drop_item, drop_amount);
    }

    /* Picks wear with every block they break, and go dull at the last point. */
    BgStack *tool = &app->player.inventory[app->player.selected];
    if (tool->count > 0 && item_durability_max(tool->item) > 0) {
        if (item_wear(tool, 1)) {
            char message[96];
            snprintf(message, sizeof(message), "Your %s broke.", item_name(tool->item));
            tool->item = BG_AIR;
            tool->count = 0;
            tool->damage = 0;
            set_toast(app, message);
        }
    }
    audio_play(BG_SOUND_BREAK);
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

static bool boss_alive(const App *app) {
    for (int i = 0; i < MAX_MOBS; ++i) {
        if (app->mobs.mobs[i].alive &&
            app->mobs.mobs[i].type == (uint8_t)MOB_CINDER_ALPHA) {
            return true;
        }
    }
    return false;
}

static bool sovereign_alive(const App *app) {
    for (int i = 0; i < MAX_MOBS; ++i) {
        if (app->mobs.mobs[i].alive &&
            app->mobs.mobs[i].type == (uint8_t)MOB_CINDER_SOVEREIGN) {
            return true;
        }
    }
    return false;
}

static void open_chest_at(App *app, int x, int y, int z, GLFWwindow *window) {
    BgContainer *container = world_container_at(&app->world, x, y, z, true);
    if (!container) {
        return;
    }
    app->open_chest = (int)(container - app->world.containers);
    app->show_inventory = false;
    stash_cursor(app);
    set_cursor_captured(window, false);
}

/* The Emberheart only answers in the deep: struck against a rift frame it
 * opens a way into the Cinderhold. Kept apart from the input loop so the
 * right click's own path can be driven headlessly. */
static void strike_emberheart(App *app, bool looking, BlockId looked, int bx, int by, int bz) {
    if (app->world.dimension != BG_UNDERWORLD) {
        set_toast(app, "The Emberheart is quiet outside the deep.");
    } else if (!looking || looked != BG_VOIDSTONE) {
        set_toast(app, "Strike the Emberheart against a voidstone frame.");
    } else if (cinder_gate_open(&app->world, bx, by, bz)) {
        app->world.cinder_open = true;
        remesh_near(app, bx, by, bz);
        audio_play(BG_SOUND_RIFT);
        set_toast(app, "The frame drinks the fire. The Cinderhold is open.");
        save_active_world(app);
    } else {
        set_toast(app, "A gate needs a 4x5 voidstone frame, hollow in the middle.");
    }
}

static void update_playing(App *app, float dt, GLFWwindow *window) {
    if (key_pressed(app, GLFW_KEY_F3)) {
        app->show_debug = !app->show_debug;
    }

    if (app->open_chest >= 0) {
        handle_chest_input(app, window);
        mobs_update(&app->mobs, &app->world, &app->player, dt);
        drops_update(app->drops, &app->drop_count, &app->world, &app->player, dt);
        if (app->player.health <= 0) {
            respawn(app);
        }
        return;
    }

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
        const int before = app->player.health;
        mobs_update(&app->mobs, &app->world, &app->player, dt);
        drops_update(app->drops, &app->drop_count, &app->world, &app->player, dt);
        quest_check(app);
        if (app->player.health < before) {
            audio_play(BG_SOUND_HURT);
        }
        if (app->player.health <= 0) {
            respawn(app);
        }
        return;
    }

    const float look_speed = app->sensitivity;
    app->player.yaw += (float)app->input.mouse_dx * look_speed;
    app->player.pitch += (float)app->input.mouse_dy * look_speed *
                         (app->invert_y ? 1.0f : -1.0f);
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
                audio_play(BG_SOUND_HIT);
                if (mobs_damage(&app->mobs, mob_index, 6, dir_x, dir_z, &killed)) {
                    BgStack mob_drops_out[2];
                    const int drop_total = mob_drops(killed, mob_drops_out);
                    for (int i = 0; i < drop_total; ++i) {
                        if (killed == MOB_CINDER_ALPHA ||
                            killed == MOB_CINDER_SOVEREIGN) {
                            inventory_add(app->player.inventory, mob_drops_out[i].item,
                                          mob_drops_out[i].count);
                        } else if (!world_drop_item(app->drops, &app->drop_count, &app->player,
                                                    mob_drops_out[i].item,
                                                    mob_drops_out[i].count)) {
                            inventory_add(app->player.inventory, mob_drops_out[i].item,
                                          mob_drops_out[i].count);
                        }
                    }
                    char message[96];
                    snprintf(message, sizeof(message), "%s defeated", mob_name(killed));
                    set_toast(app, message);
                    if (killed == MOB_CINDER_ALPHA) {
                        app->world.boss_defeated = true;
                        app->world.quest_stage = 3;
                        audio_play(BG_SOUND_WIN);
                        set_toast(app, "The Cinder Alpha falls. Take the Emberheart home.");
                    } else if (killed == MOB_CINDER_SOVEREIGN) {
                        app->world.cinder_boss_defeated = true;
                        save_active_world(app);
                        audio_play(BG_SOUND_WIN);
                        set_toast(app, "The Cinder Sovereign is unmade. The hold is quiet.");
                    } else {
                        audio_play(BG_SOUND_BREAK);
                    }
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
        int bx = 0, by = 0, bz = 0;
        BlockId looked = BG_AIR;
        int normal[3] = { 0, 0, 0 };
        const bool looking = player_raycast(&app->player, &app->world, 6.0f, &bx, &by, &bz,
                                            &looked, normal);

        if (looking && looked == BG_CHEST && !(held->count > 0 && item_block(held->item, &block))) {
            open_chest_at(app, bx, by, bz, window);
            return;
        }
        if (looking && looked == BG_PORTAL && held->count > 0 &&
            held->item == BG_ITEM_RIFT_KEY) {
            inventory_remove(app->player.inventory, BG_ITEM_RIFT_KEY, 1);
            if (app->world.quest_stage < 2) {
                app->world.quest_stage = 2;
            }
            set_toast(app, "The key sinks in. Something stirs below.");
            audio_play(BG_SOUND_RIFT);
        } else if (held->count > 0 && item_block(held->item, &block)) {
            int px = 0, py = 0, pz = 0;
            if (player_place(&app->player, &app->world, block, &px, &py, &pz)) {
                if (block == BG_CHEST) {
                    world_container_at(&app->world, px, py, pz, true);
                }
                remesh_near(app, px, py, pz);
                held->count--;
                if (held->count == 0) {
                    held->item = BG_AIR;
                }
                audio_play(BG_SOUND_PLACE);
            }
        } else if (held->count > 0 && held->item == BG_ITEM_EMBERHEART) {
            strike_emberheart(app, looking, looked, bx, by, bz);
        } else if (held->count > 0 && held->item == BG_ITEM_EMBER_SHARD) {
            /* An ember shard struck against a voidstone frame lights the rift. */
            if (looking && looked == BG_VOIDSTONE) {
                int cx = 0, cy = 0, cz = 0, axis = 0;
                const RiftFire fire = rift_ignite(&app->world, bx, by, bz, &cx, &cy, &cz, &axis);
                if (fire == RIFT_OPENED) {
                    inventory_remove(app->player.inventory, BG_ITEM_EMBER_SHARD, 1);
                    remesh_near(app, axis == 0 ? cx + 1 : cx, cy + 2,
                                axis == 0 ? cz : cz + 1);
                    set_toast(app, "The rift tears open.");
                    audio_play(BG_SOUND_RIFT);
                } else if (fire == RIFT_ALREADY_OPEN) {
                    set_toast(app, "The rift is already open.");
                } else {
                    set_toast(app, "A rift needs a 4x5 voidstone frame, hollow in the middle.");
                }
            }
        }
    }

    if (key_pressed(app, GLFW_KEY_Q)) {
        throw_selected(app);
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

    const int health_before = app->player.health;
    mobs_update(&app->mobs, &app->world, &app->player, dt);
    drops_update(app->drops, &app->drop_count, &app->world, &app->player, dt);
    quest_check(app);
    if (app->player.health < health_before) {
        audio_play(BG_SOUND_HURT);
    }

    /* The world keeps its own clock, so a save restores the same hour. */
    app->world.time_of_day += dt / DAY_LENGTH;
    if (app->world.time_of_day >= 1.0f) {
        app->world.time_of_day -= 1.0f;
    }
    app->autosave_timer += dt;
    if (app->autosave_timer >= AUTOSAVE_SECONDS) {
        app->autosave_timer = 0.0f;
        save_active_world(app);
        set_toast(app, "Autosaved.");
    }

    stream_around_player(app);

    const BlockId at_feet = world_get_block(&app->world, (int)floorf(app->player.x),
                                            (int)floorf(app->player.y + 0.5f),
                                            (int)floorf(app->player.z));
    const BlockId at_eye = world_get_block(&app->world, (int)floorf(app->player.x),
                                           (int)floorf(app->player.y + EYE_HEIGHT),
                                           (int)floorf(app->player.z));
    const bool on_rift = block_portal(at_feet) || block_portal(at_eye);
    const bool on_cinder_gate = block_cinder_gate(at_feet) || block_cinder_gate(at_eye);
    if (!app->travel_armed && (on_rift || on_cinder_gate)) {
        app->travel_armed = true;
        if (on_cinder_gate) {
            if (app->world.dimension == BG_CINDERHOLD) {
                travel_from_cinderhold(app);
            } else {
                travel_to_cinderhold(app);
            }
        } else {
            travel(app);
            if (app->world.quest_stage == 3 && app->world.boss_defeated &&
                app->world.dimension == BG_UPPERWORLD) {
                app->world.quest_stage = 4;
                save_active_world(app);
                app->state = STATE_ENDING;
                app->ending_timer = 0.0f;
                app->toast_timer = 0.0f;
                set_cursor_captured(window, false);
                audio_play(BG_SOUND_WIN);
                return;
            }
        }
    } else if (!on_rift && !on_cinder_gate) {
        app->travel_armed = false;
    }

    if (app->world.dimension == BG_UNDERWORLD && app->world.quest_stage >= 2 &&
        !app->world.boss_defeated && !boss_alive(app)) {
        if (mobs_spawn_boss(&app->mobs, &app->world, app->player.x, app->player.y,
                            app->player.z)) {
            set_toast(app, "The Cinder Alpha awakens.");
            audio_play(BG_SOUND_BOSS);
        }
    }

    if (app->world.dimension == BG_CINDERHOLD && !app->world.cinder_boss_defeated &&
        !sovereign_alive(app)) {
        if (mobs_spawn_cinder_boss(&app->mobs, &app->world, 0.5f, (float)CINDERHOLD_FLOOR,
                                   0.5f)) {
            set_toast(app, "The Cinder Sovereign rises.");
            audio_play(BG_SOUND_BOSS);
        }
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
    camera.fov = app->fov;
    camera.aspect = app->height > 0 ? (float)app->width / (float)app->height : 16.0f / 9.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 400.0f;

    renderer_set_time(&app->renderer, app->world.time_of_day);
    renderer_draw(&app->renderer, &camera);
    renderer_draw_drops(&app->renderer, &camera, app->drops, app->drop_count);
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
    g_app.world_delete_confirm = -1;
    g_app.open_chest = -1;
    g_app.ending_timer = 0.0f;
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
    settings_load(&g_app);
    settings_apply_fullscreen(&g_app, window);
    audio_init();
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
        } else if (g_app.state == STATE_SETTINGS) {
            handle_settings_input(&g_app, window);
        } else if (g_app.state == STATE_LOADING) {
            update_loading(&g_app, window);
        } else if (g_app.state == STATE_PAUSED) {
            handle_pause_input(&g_app, window);
        } else if (g_app.state == STATE_ENDING) {
            g_app.ending_timer += dt;
            handle_ending_input(&g_app, window);
        } else {
            update_playing(&g_app, dt, window);
        }

        g_app.fps = dt > 0.0001f ? dt : g_app.fps;

        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (g_app.state == STATE_TITLE) {
            render_title(&g_app);
        } else if (g_app.state == STATE_WORLDS) {
            render_worlds(&g_app);
        } else if (g_app.state == STATE_CREATE) {
            render_create(&g_app);
        } else if (g_app.state == STATE_SETTINGS) {
            render_settings(&g_app);
        } else if (g_app.state == STATE_LOADING) {
            render_loading(&g_app);
        } else {
            draw_world(&g_app);
            renderer_draw_hud(&g_app.renderer, &g_app.player);
            render_mining(&g_app);
            render_toast(&g_app);
            if (g_app.show_debug) {
                render_debug(&g_app);
            }
            if (g_app.open_chest >= 0) {
                render_chest(&g_app);
            } else if (g_app.show_inventory) {
                render_inventory(&g_app);
            }
            if (g_app.state == STATE_PAUSED) {
                render_pause(&g_app);
            } else if (g_app.state == STATE_ENDING) {
                render_ending(&g_app);
            }
        }

        glfwSwapBuffers(window);
        memcpy(g_app.prev_keys, g_app.input.keys, sizeof(g_app.prev_keys));
        g_app.input.mouse[0] = false;
        g_app.input.mouse[1] = false;
    }

    if (g_app.active_world >= 0 && g_app.active_world < g_app.world_count &&
        g_app.state != STATE_TITLE) {
        save_active_world(&g_app);
    }
    audio_shutdown();
    renderer_shutdown(&g_app.renderer);
    world_free(&g_app.world);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
