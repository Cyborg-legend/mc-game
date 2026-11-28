#ifndef BLOCKFORGE_C_EFFECTS_H
#define BLOCKFORGE_C_EFFECTS_H

/* A charging effect lights one piece at a time so the player can read how long
 * is left before it lands. `remaining` counts down from `duration`. */
static inline int bg_charge_segments(float remaining, float duration, int max_segments) {
    if (duration <= 0.0f || max_segments <= 0) {
        return 0;
    }
    if (remaining <= 0.0f) {
        return max_segments;
    }
    if (remaining >= duration) {
        return 0;
    }
    const float progress = 1.0f - remaining / duration;
    int filled = (int)(progress * (float)max_segments + 0.999f);
    if (filled < 1) {
        filled = 1;
    }
    if (filled > max_segments) {
        filled = max_segments;
    }
    return filled;
}

#endif
