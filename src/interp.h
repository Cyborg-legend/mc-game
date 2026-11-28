#ifndef BLOCKFORGE_C_INTERP_H
#define BLOCKFORGE_C_INTERP_H

/* Rendering runs faster than the fixed 60 Hz simulation, so the camera and the
 * moving actors are drawn between the last two simulation positions. */

/* How far the frame sits between the last step and the next, 0..1. */
static inline float bg_interp_alpha(double accum, double step) {
    if (step <= 0.0) {
        return 0.0f;
    }
    const float t = (float)(accum / step);
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

static inline float bg_lerp(float from, float to, float alpha) {
    return from + (to - from) * alpha;
}

#endif
