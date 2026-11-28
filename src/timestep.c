#include "timestep.h"

int timestep_advance(BgTimestep *clock, double dt) {
    if (!clock) {
        return 0;
    }
    if (dt < 0.0) {
        dt = 0.0;
    }

    clock->accum += dt;
    const double cap = BG_PHYSICS_STEP * (double)BG_PHYSICS_MAX_STEPS;
    if (clock->accum > cap) {
        clock->accum = cap;
    }

    int steps = 0;
    while (clock->accum >= BG_PHYSICS_STEP && steps < BG_PHYSICS_MAX_STEPS) {
        clock->accum -= BG_PHYSICS_STEP;
        ++steps;
    }
    return steps;
}
