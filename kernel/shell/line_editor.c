#include "shell/line_editor.h"

#include <stddef.h>

static bool is_printable_ascii(char character)
{
    const unsigned char byte = (unsigned char)character;

    return byte >= 0x20U && byte <= 0x7EU;
}

static void clear_line_buffer(struct shell_line_editor *editor)
{
    uint32_t index;

    for (index = 0U; index <= SHELL_LINE_MAX; ++index) {
        editor->buffer[index] = '\0';
    }
    editor->length = 0U;
    editor->ready = false;
}

void shell_line_editor_init(struct shell_line_editor *editor)
{
    if (editor == NULL) {
        return;
    }

    clear_line_buffer(editor);
    editor->overflow_count = 0U;
}

enum shell_line_event shell_line_editor_feed(
    struct shell_line_editor *editor,
    char character
)
{
    if (editor == NULL || editor->ready) {
        return SHELL_LINE_NONE;
    }

    if (character == '\b') {
        if (editor->length == 0U) {
            return SHELL_LINE_NONE;
        }

        editor->length -= 1U;
        editor->buffer[editor->length] = '\0';
        return SHELL_LINE_ERASE;
    }

    if (character == '\n' || character == '\r') {
        editor->ready = true;
        return SHELL_LINE_READY;
    }

    if (!is_printable_ascii(character)) {
        return SHELL_LINE_NONE;
    }

    if (editor->length >= SHELL_LINE_MAX) {
        editor->overflow_count += 1U;
        return SHELL_LINE_FULL;
    }

    editor->buffer[editor->length] = character;
    editor->length += 1U;
    editor->buffer[editor->length] = '\0';
    return SHELL_LINE_ECHO;
}

bool shell_line_editor_take(
    struct shell_line_editor *editor,
    char *line,
    uint32_t capacity
)
{
    uint32_t index;

    if (editor == NULL
        || line == NULL
        || !editor->ready
        || editor->length > SHELL_LINE_MAX
        || capacity <= editor->length) {
        return false;
    }

    for (index = 0U; index <= editor->length; ++index) {
        line[index] = editor->buffer[index];
    }

    clear_line_buffer(editor);
    return true;
}

uint32_t shell_line_editor_overflow_count(
    const struct shell_line_editor *editor
)
{
    if (editor == NULL) {
        return 0U;
    }

    return editor->overflow_count;
}
