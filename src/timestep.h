#ifndef BLOCKFORGE_C_TIMESTEP_H
#define BLOCKFORGE_C_TIMESTEP_H

/* The simulation advances in fixed 60 Hz steps so walking, gravity and mobs
 * behave the same at 30, 60 or 300 fps. Rendering still follows the display;
 * this only decides how many simulation steps a frame is owed. */

#define BG_PHYSICS_HZ 60
#define BG_PHYSICS_STEP (1.0 / (double)BG_PHYSICS_HZ)
#define BG_PHYSICS_MAX_STEPS 8

typedef struct BgTimestep {
    double accum;
} BgTimestep;

/* Add a frame's elapsed time and return how many fixed steps to simulate now.
 * The backlog is capped so one long stall cannot snowball into a fast-forward.
 * Negative or zero deltas simply return 0. */
int timestep_advance(BgTimestep *clock, double dt);

#endif
