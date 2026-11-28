#ifndef BLOCKFORGE_C_BINDINGS_H
#define BLOCKFORGE_C_BINDINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* GLFW key codes are fixed by the library, but this module deliberately does
 * not include GLFW so the table can be tested without a window or a GPU.
 * main.c static-asserts these against GLFW's constants. */
#define BG_KEY_SPACE 32
#define BG_KEY_A 65
#define BG_KEY_D 68
#define BG_KEY_E 69
#define BG_KEY_F 70
#define BG_KEY_Q 81
#define BG_KEY_S 83
#define BG_KEY_W 87
#define BG_KEY_LEFT_SHIFT 340

typedef enum {
    BG_ACTION_FORWARD = 0,
    BG_ACTION_BACK,
    BG_ACTION_LEFT,
    BG_ACTION_RIGHT,
    BG_ACTION_JUMP,
    BG_ACTION_SPRINT,
    BG_ACTION_INVENTORY,
    BG_ACTION_DROP,
    BG_ACTION_EAT,
    BG_ACTION_COUNT
} BgAction;

typedef struct BgBindings {
    int keys[BG_ACTION_COUNT];
} BgBindings;

void bindings_default(BgBindings *bindings);

/* The key code for an action, or 0 when the index is out of range. */
int bindings_key(const BgBindings *bindings, int action);

/* Binds one action. A key already used by another action is refused so two
 * actions never share a button; returns false and leaves the table alone. */
bool bindings_assign(BgBindings *bindings, int action, int key);

/* "FORWARD", "SPRINT", ... for the settings list. */
const char *bindings_action_name(int action);

/* Save-file token for an action: "forward", "back", ... */
const char *bindings_action_token(int action);

/* Human name for a key code ("W", "SPACE", "LEFT SHIFT", "KEY 32"), written
 * into out. Returns out so it can be used inline. */
const char *bindings_key_name(int key, char *out, size_t size);

/* Parses a settings token such as "forward" into an action; false if unknown. */
bool bindings_action_from_token(const char *token, int *action);

/* Applies a persisted "bind <token> <key>" line. Returns false when the token
 * is unknown or the assignment would duplicate another action. */
bool bindings_apply_setting(BgBindings *bindings, const char *token, int key);

/* Writes one "bind <token> <key>" line per action. */
void bindings_write(const BgBindings *bindings, FILE *file);

#endif /* BLOCKFORGE_C_BINDINGS_H */
