#ifndef BLOCKFORGE_C_CHUNKWORKER_H
#define BLOCKFORGE_C_CHUNKWORKER_H

#include "bg.h"
#include "chunkqueue.h"

#include <stdbool.h>

/* An experimental path: with BLOCKFORGE_ASYNC_CHUNKS the terrain for a chunk is
 * built on one worker thread and handed back as a block array, so the main
 * thread only installs it, lights it and meshes it. Off by default; the build
 * without the switch does not link this file's thread code at all.
 *
 * Only POSIX threads are wired up. On Windows, or without the switch, create
 * returns NULL and the game keeps generating on the main thread. */

/* One finished chunk, waiting to be adopted by the main thread. */
#define BG_CHUNK_RESULTS 24

typedef struct BgChunkResult {
    int cx, cy, cz;
    BlockId blocks[CHUNK_VOLUME];
} BgChunkResult;

/* Called on the worker thread. Must not touch the World, GL or the game state:
 * it gets a coordinate and a seed, and fills the block array. */
typedef void (*BgChunkGenFn)(void *user, const BgChunkJob *job, BlockId *out);

typedef struct BgChunkWorker BgChunkWorker;

/* Starts the thread. Returns NULL when the switch is off, the platform has no
 * pthreads, or the thread could not start. */
BgChunkWorker *chunkworker_create(BgChunkGenFn generate, void *user);

/* Stops the thread, waits for it, and frees its memory. Null is ignored. */
void chunkworker_destroy(BgChunkWorker *worker);

/* Queues one chunk. Returns false when the queue is full, the chunk is already
 * waiting, or there is no worker. Never blocks. */
bool chunkworker_submit(BgChunkWorker *worker, BgChunkJob job);

/* Takes one finished chunk. Returns false when nothing is ready. */
bool chunkworker_take(BgChunkWorker *worker, BgChunkResult *out);

/* Jobs still waiting, and chunks ready to take. Cheap enough for a HUD. */
int chunkworker_pending(const BgChunkWorker *worker);
int chunkworker_ready(const BgChunkWorker *worker);

/* True when this build has a working worker implementation. */
bool chunkworker_available(void);

#endif /* BLOCKFORGE_C_CHUNKWORKER_H */
