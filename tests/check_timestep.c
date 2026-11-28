/* Headless check: one second of wall time always buys 60 simulation steps,
 * whatever the frame rate, and a stall cannot fast-forward the world. */
#include "timestep.h"

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

static int seconds_at(double fps) {
    BgTimestep clock = { 0.0 };
    const double dt = 1.0 / fps;
    int steps = 0;
    for (int frame = 0; frame < (int)lround(fps); ++frame) {
        steps += timestep_advance(&clock, dt);
    }
    return steps;
}

/* Floating point accumulation can land a step either side of the ideal. */
static int near_60(int steps) {
    return steps >= 59 && steps <= 61;
}

int main(void) {
    expect(BG_PHYSICS_HZ == 60, "the simulation runs at 60 Hz");
    expect(fabs(BG_PHYSICS_STEP - 1.0 / 60.0) < 1e-12, "one step is 1/60 s");

    expect(near_60(seconds_at(60.0)), "60 fps buys 60 steps a second");
    expect(near_60(seconds_at(30.0)), "30 fps buys the same 60 steps");
    expect(near_60(seconds_at(144.0)), "144 fps buys the same 60 steps");
    expect(near_60(seconds_at(300.0)), "300 fps buys the same 60 steps");
    expect(near_60(seconds_at(20.0)), "20 fps still owes 60 steps (3 per frame)");

    /* Jittery frame times must not drift the simulation clock. */
    BgTimestep jitter = { 0.0 };
    unsigned seed = 12345u;
    int merged = 0;
    double elapsed = 0.0;
    for (int frame = 0; frame < 600; ++frame) {
        seed = seed * 1103515245u + 12345u;
        const double dt = 0.001 + (double)((seed >> 16) & 0x7FFu) / 2047.0 * 0.032;
        elapsed += dt;
        merged += timestep_advance(&jitter, dt);
    }
    const int ideal = (int)(elapsed / BG_PHYSICS_STEP);
    expect(merged >= ideal - 1 && merged <= ideal + 1,
           "jittery frames stay within one step of elapsed time");

    /* A ten-second stall yields the capped burst, not 600 steps at once. */
    BgTimestep stall = { 0.0 };
    const int burst = timestep_advance(&stall, 10.0);
    expect(burst == BG_PHYSICS_MAX_STEPS, "a stall is capped to the burst limit");
    expect(stall.accum < BG_PHYSICS_STEP, "a capped stall keeps no backlog");

    BgTimestep idle = { 0.0 };
    expect(timestep_advance(&idle, 0.0) == 0, "a zero frame owes no steps");
    expect(timestep_advance(&idle, -1.0) == 0, "time never runs backwards");
    expect(idle.accum == 0.0, "a negative delta leaves no debt");

    /* Exactly one step in, exactly one step out, with nothing left over. */
    BgTimestep exact = { 0.0 };
    expect(timestep_advance(&exact, BG_PHYSICS_STEP) == 1, "a step-sized frame steps once");
    expect(fabs(exact.accum) < 1e-12, "a step-sized frame leaves no remainder");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
