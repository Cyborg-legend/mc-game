/* Headless check: the render blend stays between the two simulation
 * positions, never overshoots, and a zero step cannot divide by zero. */
#include "interp.h"

#include <math.h>
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

static int near(float a, float b, float epsilon) {
    return fabsf(a - b) <= epsilon;
}

int main(void) {
    const double step = 1.0 / 60.0;

    expect(near(bg_interp_alpha(0.0, step), 0.0f, 1e-6f),
           "a just-stepped frame draws the previous position");
    expect(near(bg_interp_alpha(step * 0.5, step), 0.5f, 1e-6f),
           "half a step blends halfway");
    expect(near(bg_interp_alpha(step, step), 1.0f, 1e-6f),
           "a full step reaches the current position");
    expect(near(bg_interp_alpha(step * 3.0, step), 1.0f, 1e-6f),
           "a backlog cannot overshoot");
    expect(near(bg_interp_alpha(-step, step), 0.0f, 1e-6f),
           "a negative accumulator clamps to the previous position");
    expect(near(bg_interp_alpha(step * 0.5, 0.0), 0.0f, 1e-6f),
           "a zero step is safe");

    expect(near(bg_lerp(2.0f, 6.0f, 0.0f), 2.0f, 1e-6f), "alpha 0 keeps the start");
    expect(near(bg_lerp(2.0f, 6.0f, 1.0f), 6.0f, 1e-6f), "alpha 1 reaches the end");
    expect(near(bg_lerp(2.0f, 6.0f, 0.5f), 4.0f, 1e-6f), "alpha 0.5 lands in the middle");
    expect(near(bg_lerp(-3.0f, 3.0f, 0.25f), -1.5f, 1e-6f), "negative positions blend too");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
