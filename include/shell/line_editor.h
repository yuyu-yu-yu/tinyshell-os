#ifndef TINYOS_SHELL_LINE_EDITOR_H
#define TINYOS_SHELL_LINE_EDITOR_H

#include <stdbool.h>
#include <stdint.h>

enum {
    SHELL_LINE_MAX = 127,
};

enum shell_line_event {
    SHELL_LINE_NONE,
    SHELL_LINE_ECHO,
    SHELL_LINE_ERASE,
    SHELL_LINE_READY,
    SHELL_LINE_FULL,
};

struct shell_line_editor {
    char buffer[SHELL_LINE_MAX + 1];
    uint32_t length;
    uint32_t overflow_count;
    bool ready;
};

/* Reset the current line, ready state, and cumulative overflow count. */
void shell_line_editor_init(struct shell_line_editor *editor);

/* Feed one foreground character and report the resulting editing event. */
enum shell_line_event shell_line_editor_feed(
    struct shell_line_editor *editor,
    char character
);

/*
 * Copy a submitted NUL-terminated line and reset it for the next command.
 * Failure leaves both the editor and caller output unchanged.
 */
bool shell_line_editor_take(
    struct shell_line_editor *editor,
    char *line,
    uint32_t capacity
);

uint32_t shell_line_editor_overflow_count(
    const struct shell_line_editor *editor
);

#endif
