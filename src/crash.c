/* sigaltstack, stack_t and SA_ONSTACK are XSI, so ask for them even in a
 * strict -std=c11 build (CMake sets C_EXTENSIONS OFF). */
#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "crash.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/* Written beside the worlds, which the game creates at startup: a handler
 * cannot make directories, so a report is skipped if the folder is missing. */
#define CRASH_PATH "worlds/last_crash.log"
#define CRASH_TEXT_MAX 512

/* The report header is formatted ahead of time, in ordinary code, because
 * snprintf and friends are not safe to call once a signal has landed. */
static char s_head[CRASH_TEXT_MAX];
static size_t s_head_len;
static volatile sig_atomic_t s_ready;
static volatile sig_atomic_t s_handling;

void crash_set_context(const BgCrashContext *context) {
    if (!context) {
        s_ready = 0;
        return;
    }
    s_ready = 0;
    const int length = snprintf(s_head, sizeof(s_head),
                                "BlockForge %s crash\n"
                                "realm: %s\n"
                                "position: %.2f %.2f %.2f\n"
                                "health: %d\n"
                                "quest: %u  gate: %d\n"
                                "world: %s\n",
                                context->version ? context->version : "?",
                                context->realm ? context->realm : "?",
                                (double)context->x, (double)context->y, (double)context->z,
                                context->health, context->quest_stage, context->gate_open,
                                context->world ? context->world : "none");
    if (length <= 0) {
        s_head_len = 0;
        return;
    }
    s_head_len = (size_t)length < sizeof(s_head) ? (size_t)length : sizeof(s_head) - 1;
    s_ready = 1;
}

/* Plain copies and digit arithmetic only: every helper below is async-signal-
 * safe, which is why this file does not touch stdio in a handler. */
static size_t append_text(char *out, size_t at, size_t capacity, const char *text) {
    while (text && *text && at < capacity) {
        out[at++] = *text++;
    }
    return at;
}

static size_t append_uint(char *out, size_t at, size_t capacity, unsigned value) {
    char digits[12];
    int count = 0;
    if (value == 0) {
        digits[count++] = '0';
    }
    while (value > 0 && count < (int)sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count > 0 && at < capacity) {
        out[at++] = digits[--count];
    }
    return at;
}

void crash_report(int signal_number) {
    char body[CRASH_TEXT_MAX + 64];
    size_t length = 0;

    if (s_ready) {
        const size_t copy = s_head_len < sizeof(s_head) ? s_head_len : sizeof(s_head);
        for (size_t i = 0; i < copy; ++i) {
            body[length++] = s_head[i];
        }
    } else {
        length = append_text(body, length, sizeof(body), "BlockForge crash\n");
    }
    length = append_text(body, length, sizeof(body), "reason: fatal signal ");
    length = append_uint(body, length, sizeof(body),
                         (unsigned)(signal_number < 0 ? 0 : signal_number));
    length = append_text(body, length, sizeof(body), "\n");

#ifdef _WIN32
    const int fd = _open(CRASH_PATH,
                         _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                         _S_IREAD | _S_IWRITE);
#else
    const int fd = open(CRASH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
    if (fd < 0) {
        return;
    }
    size_t written = 0;
    while (written < length) {
#ifdef _WIN32
        const int result = _write(fd, body + written, (unsigned)(length - written));
#else
        const ssize_t result = write(fd, body + written, length - written);
#endif
        if (result <= 0) {
            break;
        }
        written += (size_t)result;
    }
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
}

static void on_fatal_signal(int signal_number) {
    if (s_handling) {
        _Exit(3);
    }
    s_handling = 1;
    crash_report(signal_number);
    _Exit(3);
}

void crash_install(void) {
#ifdef _WIN32
    signal(SIGSEGV, on_fatal_signal);
    signal(SIGABRT, on_fatal_signal);
    signal(SIGFPE, on_fatal_signal);
    signal(SIGILL, on_fatal_signal);
#else
    /* A blown stack needs a stack of its own, or the handler dies too. */
    static char alternate_stack[64 * 1024];
    stack_t stack;
    stack.ss_sp = alternate_stack;
    stack.ss_size = sizeof(alternate_stack);
    stack.ss_flags = 0;
    sigaltstack(&stack, NULL);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_fatal_signal;
    /* SA_RESETHAND restores the default so a second fault cannot loop. */
    action.sa_flags = SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    static const int kFatal[] = { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS };
    for (size_t i = 0; i < sizeof(kFatal) / sizeof(kFatal[0]); ++i) {
        sigaction(kFatal[i], &action, NULL);
    }
#endif
}
