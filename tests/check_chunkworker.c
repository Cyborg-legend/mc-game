/* Headless check: the worker generates chunks off the calling thread and hands
 * back exactly the coordinates and bytes the generator produced. */
#define _POSIX_C_SOURCE 199309L /* nanosleep under a strict -std=c11 */
#include "chunkworker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int checks = 0;
static int failures = 0;

static void expect(int ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

static void sleep_ms(int ms) {
    struct timespec pause;
    pause.tv_sec = ms / 1000;
    pause.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&pause, NULL);
}

/* Same formula as the check, so the payload can be verified byte for byte. */
static unsigned char expected_block(int cx, int i) {
    return (unsigned char)((i + cx * 7) % 32);
}

static void generate(void *user, const BgChunkJob *job, BlockId *out) {
    (void)user;
    for (int i = 0; i < CHUNK_VOLUME; ++i) {
        out[i] = (BlockId)expected_block(job->cx, i);
    }
}

int main(void) {
    if (!chunkworker_available()) {
        printf("worker unavailable in this build; nothing to check\n");
        return 0;
    }

    expect(chunkworker_create(NULL, NULL) == NULL, "a worker needs a generator");
    expect(!chunkworker_submit(NULL, (BgChunkJob){ 0, 0, 0 }), "a null worker refuses work");
    expect(chunkworker_ready(NULL) == 0, "a null worker has no results");

    BgChunkWorker *worker = chunkworker_create(generate, NULL);
    expect(worker != NULL, "the worker starts");
    if (!worker) {
        printf("%d checks, %d failures\n", checks, failures);
        return 1;
    }

    const int wanted = 8;
    int submitted = 0;
    for (int i = 0; i < wanted; ++i) {
        BgChunkJob job;
        job.cx = i;
        job.cy = 0;
        job.cz = -i;
        if (chunkworker_submit(worker, job)) {
            ++submitted;
        }
    }
    expect(submitted == wanted, "every distinct chunk is queued");

    BgChunkResult *taken = (BgChunkResult *)calloc((size_t)wanted, sizeof(BgChunkResult));
    int received = 0;
    for (int waited = 0; waited < 300 && received < wanted; waited += 5) {
        BgChunkResult result;
        while (received < wanted && chunkworker_take(worker, &result)) {
            taken[received++] = result;
        }
        if (received < wanted) {
            sleep_ms(5);
        }
    }
    expect(received == wanted, "every queued chunk comes back");

    int seen[8] = { 0 };
    int payload_ok = 1;
    int coordinates_ok = 1;
    for (int i = 0; i < received; ++i) {
        const BgChunkResult *result = &taken[i];
        if (result->cx < 0 || result->cx >= wanted ||
            result->cz != -result->cx || result->cy != 0) {
            coordinates_ok = 0;
            continue;
        }
        if (seen[result->cx]) {
            coordinates_ok = 0;
        }
        seen[result->cx] = 1;
        for (int b = 0; b < CHUNK_VOLUME; ++b) {
            if ((unsigned char)result->blocks[b] != expected_block(result->cx, b)) {
                payload_ok = 0;
                break;
            }
        }
    }
    expect(coordinates_ok, "each result is a different requested chunk");
    expect(payload_ok, "the worker's block array arrives intact");
    expect(chunkworker_ready(worker) == 0, "the result window drains");
    expect(chunkworker_take(worker, &taken[0]) == false, "an empty worker returns nothing");

    chunkworker_destroy(worker);
    expect(1, "the worker shuts down without hanging");
    free(taken);

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
