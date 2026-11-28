#include "chunkqueue.h"

#include <string.h>

void chunkqueue_init(BgChunkQueue *queue) {
    if (!queue) {
        return;
    }
    memset(queue, 0, sizeof(*queue));
}

bool chunkqueue_push(BgChunkQueue *queue, BgChunkJob job) {
    if (!queue || queue->count >= BG_CHUNKQUEUE_CAPACITY) {
        return false;
    }
    if (chunkqueue_contains(queue, job.cx, job.cy, job.cz)) {
        return false;
    }
    queue->jobs[(queue->head + queue->count) % BG_CHUNKQUEUE_CAPACITY] = job;
    ++queue->count;
    return true;
}

bool chunkqueue_pop(BgChunkQueue *queue, BgChunkJob *out) {
    if (!queue || queue->count <= 0) {
        return false;
    }
    if (out) {
        *out = queue->jobs[queue->head];
    }
    queue->head = (queue->head + 1) % BG_CHUNKQUEUE_CAPACITY;
    --queue->count;
    if (queue->count == 0) {
        queue->head = 0;
    }
    return true;
}

bool chunkqueue_contains(const BgChunkQueue *queue, int cx, int cy, int cz) {
    if (!queue) {
        return false;
    }
    for (int i = 0; i < queue->count; ++i) {
        const BgChunkJob *job = &queue->jobs[(queue->head + i) % BG_CHUNKQUEUE_CAPACITY];
        if (job->cx == cx && job->cy == cy && job->cz == cz) {
            return true;
        }
    }
    return false;
}

void chunkqueue_clear(BgChunkQueue *queue) {
    if (!queue) {
        return;
    }
    queue->head = 0;
    queue->count = 0;
}

int chunkqueue_count(const BgChunkQueue *queue) {
    return queue ? queue->count : 0;
}

bool chunkqueue_empty(const BgChunkQueue *queue) {
    return chunkqueue_count(queue) == 0;
}

bool chunkqueue_full(const BgChunkQueue *queue) {
    return queue && queue->count >= BG_CHUNKQUEUE_CAPACITY;
}
