#ifndef BLOCKFORGE_C_CHUNKQUEUE_H
#define BLOCKFORGE_C_CHUNKQUEUE_H

#include <stdbool.h>

/* A bounded FIFO of chunk coordinates waiting to be generated. It holds no
 * locks of its own: the worker wraps calls in its mutex. Keeping the queue
 * pure means its ordering, capacity and de-duplication can be checked without
 * a thread. */

#define BG_CHUNKQUEUE_CAPACITY 64

typedef struct BgChunkJob {
    int cx, cy, cz;
} BgChunkJob;

typedef struct BgChunkQueue {
    BgChunkJob jobs[BG_CHUNKQUEUE_CAPACITY];
    int head;
    int count;
} BgChunkQueue;

void chunkqueue_init(BgChunkQueue *queue);

/* Adds a job. Fails when the queue is full or the same chunk is already
 * waiting, so walking back and forth cannot flood it with duplicates. */
bool chunkqueue_push(BgChunkQueue *queue, BgChunkJob job);

/* Removes the oldest job. False when the queue is empty. */
bool chunkqueue_pop(BgChunkQueue *queue, BgChunkJob *out);

/* True when that exact chunk is already waiting. */
bool chunkqueue_contains(const BgChunkQueue *queue, int cx, int cy, int cz);

void chunkqueue_clear(BgChunkQueue *queue);
int chunkqueue_count(const BgChunkQueue *queue);
bool chunkqueue_empty(const BgChunkQueue *queue);
bool chunkqueue_full(const BgChunkQueue *queue);

#endif /* BLOCKFORGE_C_CHUNKQUEUE_H */
