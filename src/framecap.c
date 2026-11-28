#include "framecap.h"

static const int kCaps[BG_FRAMECAP_COUNT] = { 0, 60, 120, 144, 240 };
static const char *kLabels[BG_FRAMECAP_COUNT] = {
    "OFF", "60 FPS", "120 FPS", "144 FPS", "240 FPS"
};

int bg_framecap_clamp(int slot) {
    if (slot < 0) {
        return 0;
    }
    if (slot >= BG_FRAMECAP_COUNT) {
        return BG_FRAMECAP_COUNT - 1;
    }
    return slot;
}

int bg_framecap_hz(int slot) {
    return kCaps[bg_framecap_clamp(slot)];
}

const char *bg_framecap_label(int slot) {
    return kLabels[bg_framecap_clamp(slot)];
}

int bg_framecap_next(int slot) {
    return (bg_framecap_clamp(slot) + 1) % BG_FRAMECAP_COUNT;
}

double bg_framecap_delay(double elapsed, int hz) {
    if (hz <= 0) {
        return 0.0;
    }
    const double budget = 1.0 / (double)hz;
    if (elapsed <= 0.0) {
        return budget;
    }
    if (elapsed >= budget) {
        return 0.0;
    }
    return budget - elapsed;
}
