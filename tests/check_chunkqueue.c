/* Headless check: the chunk job queue is FIFO, bounded, wraps without losing
 * order and refuses duplicates of chunks already waiting. */
#include "chunkqueue.h"

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

static BgChunkJob job(int cx, int cy, int cz) {
    BgChunkJob j;
    j.cx = cx;
    j.cy = cy;
    j.cz = cz;
    return j;
}

int main(void) {
    BgChunkQueue queue;
    chunkqueue_init(&queue);

    expect(chunkqueue_empty(&queue), "a fresh queue is empty");
    expect(chunkqueue_count(&queue) == 0, "a fresh queue counts zero");
    expect(!chunkqueue_pop(&queue, NULL), "popping an empty queue fails");

    expect(chunkqueue_push(&queue, job(1, 0, 2)), "push succeeds");
    expect(chunkqueue_count(&queue) == 1, "the push is counted");
    expect(chunkqueue_contains(&queue, 1, 0, 2), "the waiting chunk is found");
    expect(!chunkqueue_contains(&queue, 1, 1, 2), "a different row is not found");
    expect(!chunkqueue_push(&queue, job(1, 0, 2)), "a duplicate is refused");
    expect(chunkqueue_count(&queue) == 1, "the refused duplicate changes nothing");

    BgChunkJob out;
    expect(chunkqueue_pop(&queue, &out), "pop succeeds");
    expect(out.cx == 1 && out.cy == 0 && out.cz == 2, "pop returns the pushed job");
    expect(chunkqueue_empty(&queue), "the queue empties again");

    /* FIFO across a wrap of the ring buffer. */
    BgChunkQueue ring;
    chunkqueue_init(&ring);
    const int extra = 5;
    int pushed = 0;
    for (int i = 0; i < BG_CHUNKQUEUE_CAPACITY + extra; ++i) {
        if (chunkqueue_push(&ring, job(i, 0, -i))) {
            ++pushed;
        }
    }
    expect(pushed == BG_CHUNKQUEUE_CAPACITY, "the queue stops at its capacity");
    expect(chunkqueue_full(&ring), "a full queue says so");
    expect(!chunkqueue_push(&ring, job(999, 0, 999)), "a full queue refuses more");

    int ordered = 1;
    for (int i = 0; i < BG_CHUNKQUEUE_CAPACITY; ++i) {
        if (!chunkqueue_pop(&ring, &out) || out.cx != i || out.cz != -i) {
            ordered = 0;
        }
    }
    expect(ordered, "jobs come out in the order they went in");
    expect(chunkqueue_empty(&ring), "the ring drains completely");

    /* Reuse after draining must not keep stale entries. */
    expect(chunkqueue_push(&ring, job(5, 1, 5)), "the queue accepts work again");
    expect(chunkqueue_count(&ring) == 1, "only the new job waits");
    expect(!chunkqueue_contains(&ring, 0, 0, 0), "stale coordinates are gone");
    chunkqueue_clear(&ring);
    expect(chunkqueue_empty(&ring), "clear empties the queue");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
