#ifndef BLOCKFORGE_C_CRASH_H
#define BLOCKFORGE_C_CRASH_H

/* What the crash report says about the moment before the crash. The strings
 * are copied when the context is set, so they need not outlive the call. */
typedef struct BgCrashContext {
    const char *version;
    const char *realm;
    const char *world;
    float x, y, z;
    int health;
    unsigned quest_stage;
    int gate_open;
    /* Recent GPU driver messages, already formatted as lines (see gltrace.h).
     * Empty when the driver sent nothing worth keeping. */
    const char *gl_trace;
} BgCrashContext;

/* Formats and stores the context. Call it from normal game code (once a
 * second is plenty), never from a signal handler. */
void crash_set_context(const BgCrashContext *context);

/* Installs the fatal-signal handlers. */
void crash_install(void);

/* Writes worlds/last_crash.log and returns. Only async-signal-safe calls are
 * used here, so it is safe to run it from a signal handler. */
void crash_report(int signal_number);

#endif /* BLOCKFORGE_C_CRASH_H */
