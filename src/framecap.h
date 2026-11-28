#ifndef BLOCKFORGE_C_FRAMECAP_H
#define BLOCKFORGE_C_FRAMECAP_H

/* Vsync paces the swap to the display. When it is turned off the game would
 * otherwise render as many frames as the GPU can push, which burns power and
 * makes the fans spin for no visible benefit. The settings screen cycles a
 * short table of caps; slot 0 means uncapped.
 *
 * Everything here is pure arithmetic so a headless check can cover it. */

#define BG_FRAMECAP_COUNT 5

/* Refresh rate for a slot, 0 when uncapped. Out-of-range slots clamp. */
int bg_framecap_hz(int slot);

/* Short settings-screen text for a slot, e.g. "144 FPS" or "OFF". */
const char *bg_framecap_label(int slot);

/* Slot after this one, wrapping; used by the -/+ steppers. */
int bg_framecap_next(int slot);

/* Keep a loaded or stored slot inside the table. */
int bg_framecap_clamp(int slot);

/* How long to wait after a frame that has already taken `elapsed` seconds,
 * given a cap in Hz. Returns 0 when uncapped or when the frame is already
 * over budget, so a slow frame never sleeps. */
double bg_framecap_delay(double elapsed, int hz);

#endif
