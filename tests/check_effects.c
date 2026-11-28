/* Headless check: the slam ring starts empty, fills steadily, and is complete
 * exactly when the slam lands. */
#include "effects.h"

#include <stdio.h>

static int checks = 0;
static int failures = 0;

static void expect(int ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

int main(void) {
    const float duration = 0.8f;

    expect(bg_charge_segments(duration, duration, 48) == 0, "a fresh wind-up is dark");
    expect(bg_charge_segments(0.6f, duration, 48) == 12, "a quarter elapsed lights a quarter");
    expect(bg_charge_segments(0.4f, duration, 48) == 24, "half elapsed lights half");
    expect(bg_charge_segments(0.001f, duration, 48) == 48, "the last instant is fully lit");
    expect(bg_charge_segments(0.0f, duration, 48) == 48, "the landing frame is fully lit");
    expect(bg_charge_segments(1.5f, duration, 48) == 0, "waiting longer than the wind-up is dark");
    expect(bg_charge_segments(0.79f, duration, 48) >= 1, "the first moment shows at least one seg");
    expect(bg_charge_segments(0.5f, 0.0f, 48) == 0, "a zero-length charge is dark");
    expect(bg_charge_segments(0.5f, duration, 0) == 0, "a ring with no segments is dark");

    int previous = 0;
    int monotonic = 1;
    for (int step = 0; step <= 80; ++step) {
        const float remaining = duration - (float)step * 0.01f;
        const int lit = bg_charge_segments(remaining, duration, 48);
        if (lit < previous || lit > 48) {
            monotonic = 0;
        }
        previous = lit;
    }
    expect(monotonic, "the ring never shrinks as the slam approaches");
    expect(previous == 48, "the ring is complete by the landing frame");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
