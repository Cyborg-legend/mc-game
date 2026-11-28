#include "bindings.h"

#include <stdio.h>
#include <string.h>

static const char *const kActionNames[BG_ACTION_COUNT] = {
    "FORWARD", "BACK", "LEFT", "RIGHT", "JUMP", "SPRINT", "INVENTORY", "DROP", "EAT"
};

static const char *const kActionTokens[BG_ACTION_COUNT] = {
    "forward", "back", "left", "right", "jump", "sprint", "inventory", "drop", "eat"
};

void bindings_default(BgBindings *bindings) {
    if (!bindings) {
        return;
    }
    bindings->keys[BG_ACTION_FORWARD] = BG_KEY_W;
    bindings->keys[BG_ACTION_BACK] = BG_KEY_S;
    bindings->keys[BG_ACTION_LEFT] = BG_KEY_A;
    bindings->keys[BG_ACTION_RIGHT] = BG_KEY_D;
    bindings->keys[BG_ACTION_JUMP] = BG_KEY_SPACE;
    bindings->keys[BG_ACTION_SPRINT] = BG_KEY_LEFT_SHIFT;
    bindings->keys[BG_ACTION_INVENTORY] = BG_KEY_E;
    bindings->keys[BG_ACTION_DROP] = BG_KEY_Q;
    bindings->keys[BG_ACTION_EAT] = BG_KEY_F;
}

int bindings_key(const BgBindings *bindings, int action) {
    if (!bindings || action < 0 || action >= BG_ACTION_COUNT) {
        return 0;
    }
    return bindings->keys[action];
}

bool bindings_assign(BgBindings *bindings, int action, int key) {
    if (!bindings || action < 0 || action >= BG_ACTION_COUNT || key <= 0) {
        return false;
    }
    for (int i = 0; i < BG_ACTION_COUNT; ++i) {
        if (i != action && bindings->keys[i] == key) {
            return false;
        }
    }
    bindings->keys[action] = key;
    return true;
}

const char *bindings_action_name(int action) {
    if (action < 0 || action >= BG_ACTION_COUNT) {
        return "UNKNOWN";
    }
    return kActionNames[action];
}

const char *bindings_action_token(int action) {
    if (action < 0 || action >= BG_ACTION_COUNT) {
        return "unknown";
    }
    return kActionTokens[action];
}

const char *bindings_key_name(int key, char *out, size_t size) {
    if (!out || size == 0) {
        return "";
    }
    if (key >= 'A' && key <= 'Z') {
        snprintf(out, size, "%c", (char)key);
        return out;
    }
    if (key >= '0' && key <= '9') {
        snprintf(out, size, "%c", (char)key);
        return out;
    }
    if (key >= 290 && key <= 301) {
        snprintf(out, size, "F%d", key - 289);
        return out;
    }
    switch (key) {
        case BG_KEY_SPACE:      snprintf(out, size, "SPACE"); break;
        case BG_KEY_LEFT_SHIFT: snprintf(out, size, "L SHIFT"); break;
        case 344:               snprintf(out, size, "R SHIFT"); break;
        case 341:               snprintf(out, size, "L CTRL"); break;
        case 345:               snprintf(out, size, "R CTRL"); break;
        case 342:               snprintf(out, size, "L ALT"); break;
        case 346:               snprintf(out, size, "R ALT"); break;
        case 258:               snprintf(out, size, "TAB"); break;
        case 257:               snprintf(out, size, "ENTER"); break;
        case 256:               snprintf(out, size, "ESCAPE"); break;
        case 259:               snprintf(out, size, "BACKSPACE"); break;
        case 261:               snprintf(out, size, "DELETE"); break;
        case 262:               snprintf(out, size, "RIGHT"); break;
        case 263:               snprintf(out, size, "LEFT"); break;
        case 264:               snprintf(out, size, "DOWN"); break;
        case 265:               snprintf(out, size, "UP"); break;
        case 266:               snprintf(out, size, "PAGE UP"); break;
        case 267:               snprintf(out, size, "PAGE DOWN"); break;
        case 268:               snprintf(out, size, "HOME"); break;
        case 269:               snprintf(out, size, "END"); break;
        default:                snprintf(out, size, "KEY %d", key); break;
    }
    return out;
}

bool bindings_action_from_token(const char *token, int *action) {
    if (!token || !action) {
        return false;
    }
    for (int i = 0; i < BG_ACTION_COUNT; ++i) {
        if (strcmp(token, kActionTokens[i]) == 0) {
            *action = i;
            return true;
        }
    }
    return false;
}

bool bindings_apply_setting(BgBindings *bindings, const char *token, int key) {
    int action = 0;
    if (!bindings_action_from_token(token, &action)) {
        return false;
    }
    return bindings_assign(bindings, action, key);
}

void bindings_write(const BgBindings *bindings, FILE *file) {
    if (!bindings || !file) {
        return;
    }
    for (int i = 0; i < BG_ACTION_COUNT; ++i) {
        fprintf(file, "bind %s %d\n", kActionTokens[i], bindings->keys[i]);
    }
}
