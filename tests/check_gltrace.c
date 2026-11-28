/* Headless check: driver messages keep their order, drop the oldest when the
 * window fills, stay single-line, and export into the crash header. */
#include "gltrace.h"

#include <stdio.h>
#include <string.h>

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
    gltrace_clear();
    expect(gltrace_count() == 0, "a fresh window is empty");
    expect(gltrace_line(0)[0] == '\0', "an empty window has no lines");

    char out[256];
    expect(gltrace_append(out, sizeof(out)) == 0, "an empty window exports nothing");
    expect(out[0] == '\0', "the export is an empty string");

    gltrace_add("Intel: shader compiled");
    gltrace_add("AMD: performance warning");
    expect(gltrace_count() == 2, "two messages are kept");
    expect(strcmp(gltrace_line(0), "Intel: shader compiled") == 0, "the oldest comes first");
    expect(strcmp(gltrace_line(1), "AMD: performance warning") == 0, "order is preserved");
    expect(gltrace_line(2)[0] == '\0', "reading past the window is empty");

    const size_t length = gltrace_append(out, sizeof(out));
    expect(length > 0, "the export writes bytes");
    expect(strstr(out, "gl: Intel: shader compiled\n") != NULL, "the export prefixes each line");
    expect(strstr(out, "gl: AMD: performance warning\n") != NULL, "both lines are exported");
    expect(out[length] == '\0', "the export is terminated");
    expect(strlen(out) == length, "the returned length matches the text");

    gltrace_add(NULL);
    gltrace_add("");
    expect(gltrace_count() == 2, "empty messages are ignored");

    gltrace_add("one\ntwo\tthree");
    expect(strcmp(gltrace_line(2), "one two three") == 0,
           "newlines and tabs become spaces so one message stays one line");

    /* Fill past the window and check the oldest lines are the ones dropped. */
    gltrace_clear();
    char label[32];
    for (int i = 0; i < BG_GLTRACE_SLOTS + 3; ++i) {
        snprintf(label, sizeof(label), "message %d", i);
        gltrace_add(label);
    }
    expect(gltrace_count() == BG_GLTRACE_SLOTS, "the window is capped");
    expect(strcmp(gltrace_line(0), "message 3") == 0, "the oldest three lines were dropped");
    expect(strcmp(gltrace_line(BG_GLTRACE_SLOTS - 1), "message 8") == 0,
           "the newest line is last");

    char tiny[24];
    const size_t cut = gltrace_append(tiny, sizeof(tiny));
    expect(cut == sizeof(tiny) - 1, "a short buffer is filled without overrunning");
    expect(tiny[sizeof(tiny) - 1] == '\0', "a short buffer stays terminated");

    char long_message[BG_GLTRACE_TEXT * 2];
    memset(long_message, 'x', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';
    gltrace_clear();
    gltrace_add(long_message);
    expect(strlen(gltrace_line(0)) == BG_GLTRACE_TEXT - 1, "a long message is truncated to fit");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
