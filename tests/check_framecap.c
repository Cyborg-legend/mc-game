/* Headless check: the frame cap table cycles predictably, clamps bad slots and
 * only ever asks for the time left over inside a frame's budget. */
#include "framecap.h"

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

int main(void) {
    expect(BG_FRAMECAP_COUNT == 5, "the table has five slots");
    expect(bg_framecap_hz(0) == 0, "slot 0 is uncapped");
    expect(bg_framecap_label(0)[0] == 'O', "uncapped reads as OFF");
    expect(bg_framecap_hz(1) == 60, "slot 1 is 60 Hz");
    expect(bg_framecap_hz(2) == 120, "slot 2 is 120 Hz");
    expect(bg_framecap_hz(3) == 144, "slot 3 is 144 Hz");
    expect(bg_framecap_hz(4) == 240, "slot 4 is 240 Hz");

    expect(bg_framecap_next(0) == 1, "cycling off goes to 60");
    expect(bg_framecap_next(4) == 0, "cycling wraps back to off");

    expect(bg_framecap_clamp(-3) == 0, "negative slots clamp to off");
    expect(bg_framecap_clamp(99) == BG_FRAMECAP_COUNT - 1, "huge slots clamp to the last");
    expect(bg_framecap_hz(-1) == 0, "an out-of-range slot is uncapped after clamping");

    expect(bg_framecap_delay(0.010, 0) == 0.0, "uncapped never sleeps");
    expect(bg_framecap_delay(0.010, -60) == 0.0, "a nonsense cap never sleeps");
    expect(fabs(bg_framecap_delay(0.0, 60) - 1.0 / 60.0) < 1e-9,
           "an instant frame waits the whole budget");
    expect(fabs(bg_framecap_delay(0.010, 60) - (1.0 / 60.0 - 0.010)) < 1e-9,
           "a frame inside its budget waits the remainder");
    expect(bg_framecap_delay(1.0 / 60.0, 60) == 0.0, "a frame on budget never sleeps");
    expect(bg_framecap_delay(0.5, 60) == 0.0, "a slow frame never sleeps");

    /* Every slot's budget must leave at least one idle moment per second. */
    int budgets_ok = 1;
    for (int slot = 0; slot < BG_FRAMECAP_COUNT; ++slot) {
        const int hz = bg_framecap_hz(slot);
        if (hz <= 0) {
            continue;
        }
        const double delay = bg_framecap_delay(0.0, hz);
        if (delay <= 0.0 || delay > 1.0 / (double)hz + 1e-9) {
            budgets_ok = 0;
        }
    }
    expect(budgets_ok, "every capped slot sleeps within its own frame budget");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
