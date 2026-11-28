#include "chunkworker.h"

#include <stdlib.h>
#include <string.h>

#if defined(BG_ASYNC_CHUNKS) && !defined(_WIN32)

#include <pthread.h>

struct BgChunkWorker {
    pthread_mutex_t lock;
    pthread_cond_t wake;
    pthread_t thread;
    BgChunkQueue jobs;
    BgChunkResult *results;
    int result_head;
    int result_count;
    BgChunkGenFn generate;
    void *user;
    bool stopping;
    bool started;
};

static void *worker_main(void *argument) {
    BgChunkWorker *worker = (BgChunkWorker *)argument;
    for (;;) {
        pthread_mutex_lock(&worker->lock);
        while (chunkqueue_empty(&worker->jobs) && !worker->stopping) {
            pthread_cond_wait(&worker->wake, &worker->lock);
        }
        if (worker->stopping && chunkqueue_empty(&worker->jobs)) {
            pthread_mutex_unlock(&worker->lock);
            break;
        }
        BgChunkJob job;
        const bool have_job = chunkqueue_pop(&worker->jobs, &job);
        pthread_mutex_unlock(&worker->lock);

        if (!have_job) {
            continue;
        }

        BgChunkResult *result = (BgChunkResult *)malloc(sizeof(*result));
        if (!result) {
            continue;
        }
        result->cx = job.cx;
        result->cy = job.cy;
        result->cz = job.cz;
        worker->generate(worker->user, &job, result->blocks);

        pthread_mutex_lock(&worker->lock);
        if (worker->result_count < BG_CHUNK_RESULTS) {
            const int slot = (worker->result_head + worker->result_count) % BG_CHUNK_RESULTS;
            worker->results[slot] = *result;
            ++worker->result_count;
        }
        /* A full result window means the main thread is behind; the chunk is
         * dropped rather than blocking the worker, and wants regeneration
         * when it is next seen. */
        pthread_mutex_unlock(&worker->lock);
        free(result);
    }
    return NULL;
}

BgChunkWorker *chunkworker_create(BgChunkGenFn generate, void *user) {
    if (!generate) {
        return NULL;
    }
    BgChunkWorker *worker = (BgChunkWorker *)calloc(1, sizeof(*worker));
    if (!worker) {
        return NULL;
    }
    worker->results = (BgChunkResult *)calloc(BG_CHUNK_RESULTS, sizeof(BgChunkResult));
    if (!worker->results) {
        free(worker);
        return NULL;
    }
    chunkqueue_init(&worker->jobs);
    worker->generate = generate;
    worker->user = user;
    if (pthread_mutex_init(&worker->lock, NULL) != 0) {
        free(worker->results);
        free(worker);
        return NULL;
    }
    if (pthread_cond_init(&worker->wake, NULL) != 0) {
        pthread_mutex_destroy(&worker->lock);
        free(worker->results);
        free(worker);
        return NULL;
    }
    if (pthread_create(&worker->thread, NULL, worker_main, worker) != 0) {
        pthread_cond_destroy(&worker->wake);
        pthread_mutex_destroy(&worker->lock);
        free(worker->results);
        free(worker);
        return NULL;
    }
    worker->started = true;
    return worker;
}

void chunkworker_destroy(BgChunkWorker *worker) {
    if (!worker) {
        return;
    }
    if (worker->started) {
        pthread_mutex_lock(&worker->lock);
        worker->stopping = true;
        pthread_cond_broadcast(&worker->wake);
        pthread_mutex_unlock(&worker->lock);
        pthread_join(worker->thread, NULL);
    }
    pthread_cond_destroy(&worker->wake);
    pthread_mutex_destroy(&worker->lock);
    free(worker->results);
    free(worker);
}

bool chunkworker_submit(BgChunkWorker *worker, BgChunkJob job) {
    if (!worker) {
        return false;
    }
    pthread_mutex_lock(&worker->lock);
    const bool pushed = chunkqueue_push(&worker->jobs, job);
    if (pushed) {
        pthread_cond_signal(&worker->wake);
    }
    pthread_mutex_unlock(&worker->lock);
    return pushed;
}

bool chunkworker_take(BgChunkWorker *worker, BgChunkResult *out) {
    if (!worker || !out) {
        return false;
    }
    pthread_mutex_lock(&worker->lock);
    if (worker->result_count <= 0) {
        pthread_mutex_unlock(&worker->lock);
        return false;
    }
    *out = worker->results[worker->result_head];
    worker->result_head = (worker->result_head + 1) % BG_CHUNK_RESULTS;
    --worker->result_count;
    if (worker->result_count == 0) {
        worker->result_head = 0;
    }
    pthread_mutex_unlock(&worker->lock);
    return true;
}

int chunkworker_pending(const BgChunkWorker *worker) {
    if (!worker) {
        return 0;
    }
    pthread_mutex_lock((pthread_mutex_t *)&worker->lock);
    const int count = chunkqueue_count(&worker->jobs);
    pthread_mutex_unlock((pthread_mutex_t *)&worker->lock);
    return count;
}

int chunkworker_ready(const BgChunkWorker *worker) {
    if (!worker) {
        return 0;
    }
    pthread_mutex_lock((pthread_mutex_t *)&worker->lock);
    const int count = worker->result_count;
    pthread_mutex_unlock((pthread_mutex_t *)&worker->lock);
    return count;
}

bool chunkworker_available(void) {
    return true;
}

#else /* switch off, or no pthreads on this platform */

BgChunkWorker *chunkworker_create(BgChunkGenFn generate, void *user) {
    (void)generate;
    (void)user;
    return NULL;
}

void chunkworker_destroy(BgChunkWorker *worker) {
    (void)worker;
}

bool chunkworker_submit(BgChunkWorker *worker, BgChunkJob job) {
    (void)worker;
    (void)job;
    return false;
}

bool chunkworker_take(BgChunkWorker *worker, BgChunkResult *out) {
    (void)worker;
    (void)out;
    return false;
}

int chunkworker_pending(const BgChunkWorker *worker) {
    (void)worker;
    return 0;
}

int chunkworker_ready(const BgChunkWorker *worker) {
    (void)worker;
    return 0;
}

bool chunkworker_available(void) {
    return false;
}

#endif
