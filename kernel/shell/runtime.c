#include "shell/runtime.h"

#include "console.h"
#include "diag/system_status.h"
#include "fs/ramfs.h"
#include "shell/command.h"
#include "shell/line_editor.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    SHELL_TEXT_CAPACITY = 256,
};

static struct shell_line_editor runtime_editor;
static bool runtime_initialized;
static bool runtime_started;

static bool strings_equal(const char *left, const char *right)
{
    uint32_t index = 0U;

    for (;;) {
        if (left[index] != right[index]) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
        index += 1U;
    }
}

static bool bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    uint32_t length
)
{
    uint32_t index;

    for (index = 0U; index < length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool ramfs_self_test(void)
{
    static const uint8_t first[] = {'a', 'b', 'c'};
    static const uint8_t appended[] = {'d'};
    static const uint8_t expected[] = {'a', 'b', 'c', 'd'};
    uint8_t output[sizeof(expected)];
    struct ramfs_file_info info;
    uint32_t length;

    ramfs_init();
    if (!ramfs_validate()
        || ramfs_count() != 0U
        || ramfs_create("self-test") != RAMFS_RESULT_OK
        || ramfs_create("self-test") != RAMFS_RESULT_ALREADY_EXISTS
        || ramfs_create("bad/name") != RAMFS_RESULT_INVALID_NAME
        || ramfs_write("self-test", first, sizeof(first)) != RAMFS_RESULT_OK
        || ramfs_append("self-test", appended, sizeof(appended))
            != RAMFS_RESULT_OK
        || ramfs_count() != 1U
        || !ramfs_get_info(0U, &info)
        || !strings_equal(info.name, "self-test")
        || info.size != sizeof(expected)) {
        return false;
    }

    output[0] = 0xA5U;
    length = UINT32_C(0xA5A5A5A5);
    if (ramfs_read("self-test", output, sizeof(output) - 1U, &length)
            != RAMFS_RESULT_TOO_LARGE
        || output[0] != 0xA5U
        || length != UINT32_C(0xA5A5A5A5)
        || ramfs_append(
            "self-test",
            appended,
            RAMFS_FILE_MAX_BYTES - sizeof(expected) + 1U)
            != RAMFS_RESULT_TOO_LARGE) {
        return false;
    }

    length = 0U;
    if (ramfs_read("self-test", output, sizeof(output), &length)
            != RAMFS_RESULT_OK
        || length != sizeof(expected)
        || !bytes_equal(output, expected, sizeof(expected))
        || !ramfs_validate()
        || ramfs_remove("self-test") != RAMFS_RESULT_OK
        || ramfs_count() != 0U
        || !ramfs_validate()) {
        return false;
    }

    return true;
}

static bool line_editor_self_test(void)
{
    struct shell_line_editor editor;
    char output[SHELL_LINE_MAX + 1];
    uint32_t index;

    shell_line_editor_init(&editor);
    if (editor.length != 0U
        || editor.ready
        || shell_line_editor_overflow_count(&editor) != 0U
        || shell_line_editor_feed(&editor, 'a') != SHELL_LINE_ECHO
        || shell_line_editor_feed(&editor, 'b') != SHELL_LINE_ECHO
        || shell_line_editor_feed(&editor, '\b') != SHELL_LINE_ERASE
        || shell_line_editor_feed(&editor, '\n') != SHELL_LINE_READY
        || shell_line_editor_feed(&editor, 'x') != SHELL_LINE_NONE
        || shell_line_editor_take(&editor, output, 1U)
        || !shell_line_editor_take(&editor, output, sizeof(output))
        || !strings_equal(output, "a")) {
        return false;
    }

    for (index = 0U; index < SHELL_LINE_MAX; ++index) {
        if (shell_line_editor_feed(&editor, 'x') != SHELL_LINE_ECHO) {
            return false;
        }
    }
    if (shell_line_editor_feed(&editor, 'y') != SHELL_LINE_FULL
        || shell_line_editor_overflow_count(&editor) != 1U
        || shell_line_editor_feed(&editor, '\r') != SHELL_LINE_READY
        || !shell_line_editor_take(&editor, output, sizeof(output))
        || output[SHELL_LINE_MAX] != '\0'
        || editor.length != 0U
        || editor.ready) {
        return false;
    }

    return true;
}

static bool parser_self_test(void)
{
    struct shell_command command;

    if (shell_command_parse("  write\tnote  hello world ", &command)
            != SHELL_PARSE_OK
        || command.kind != SHELL_COMMAND_WRITE
        || command.argument_count != 3U
        || !strings_equal(command.name, "write")
        || !strings_equal(command.arguments[0], "note")
        || !strings_equal(command.arguments[1], "hello")
        || !strings_equal(command.arguments[2], "world")
        || !shell_command_has_valid_arity(&command)
        || shell_command_usage(command.kind) == 0
        || shell_command_help() == 0) {
        return false;
    }

    if (shell_command_parse("nosuch", &command) != SHELL_PARSE_OK
        || command.kind != SHELL_COMMAND_UNKNOWN
        || shell_command_has_valid_arity(&command)
        || shell_command_parse(" \t ", &command) != SHELL_PARSE_EMPTY) {
        return false;
    }

    return true;
}

static bool buffer_append_char(
    char *buffer,
    uint32_t capacity,
    uint32_t *length,
    char character
)
{
    if (*length + 1U >= capacity) {
        return false;
    }

    buffer[*length] = character;
    *length += 1U;
    buffer[*length] = '\0';
    return true;
}

static bool join_arguments(
    const struct shell_command *command,
    uint32_t first_argument,
    bool leading_space,
    char *buffer,
    uint32_t capacity,
    uint32_t *length
)
{
    uint32_t argument;

    if (command == 0 || buffer == 0 || length == 0 || capacity == 0U
        || first_argument > command->argument_count) {
        return false;
    }

    *length = 0U;
    buffer[0] = '\0';
    if (leading_space
        && !buffer_append_char(buffer, capacity, length, ' ')) {
        return false;
    }

    for (argument = first_argument;
         argument < command->argument_count;
         ++argument) {
        uint32_t character = 0U;

        if (argument != first_argument
            && !buffer_append_char(buffer, capacity, length, ' ')) {
            return false;
        }
        while (command->arguments[argument][character] != '\0') {
            if (!buffer_append_char(
                    buffer,
                    capacity,
                    length,
                    command->arguments[argument][character])) {
                return false;
            }
            character += 1U;
        }
    }

    return true;
}

static void print_prompt(void)
{
    console_write("tiny> ");
}

static void print_ramfs_result(enum ramfs_result result)
{
    switch (result) {
    case RAMFS_RESULT_INVALID_ARGUMENT:
        console_write("error: invalid argument\n");
        break;
    case RAMFS_RESULT_INVALID_NAME:
        console_write("error: invalid name\n");
        break;
    case RAMFS_RESULT_NOT_FOUND:
        console_write("error: file not found\n");
        break;
    case RAMFS_RESULT_ALREADY_EXISTS:
        console_write("error: file exists\n");
        break;
    case RAMFS_RESULT_NO_SPACE:
        console_write("error: ramfs full\n");
        break;
    case RAMFS_RESULT_TOO_LARGE:
        console_write("error: file too large\n");
        break;
    case RAMFS_RESULT_OK:
    default:
        break;
    }
}

static void print_status(void)
{
    struct system_status status;

    if (!system_status_read(&status)) {
        console_write("error: status unavailable\n");
        return;
    }

    console_write("PMM pages: free=");
    console_write_u32_dec(status.pmm_free_pages);
    console_write(" total=");
    console_write_u32_dec(status.pmm_total_pages);
    console_putc('\n');

    console_write("Heap bytes: free=");
    console_write_u32_dec(status.heap_free_bytes);
    console_write(" total=");
    console_write_u32_dec(status.heap_total_bytes);
    console_write(" largest=");
    console_write_u32_dec(status.heap_largest_free_block);
    console_write(" blocks=");
    console_write_u32_dec(status.heap_allocated_blocks);
    console_putc('\n');

    console_write("Timer: ticks=");
    console_write_u32_dec(status.pit_ticks);
    console_write(" irq0=");
    console_write_u32_dec(status.irq0_count);
    console_putc('\n');

    console_write("Keyboard: dropped=");
    console_write_u32_dec(status.keyboard_dropped);
    console_putc('\n');

    console_write("Tasks: switches=");
    console_write_u32_dec(status.task_switches);
    console_write(" finished=");
    console_write_u32_dec(status.task_finished);
    console_putc('\n');
}

static bool file_size(const char *name, uint32_t *size)
{
    uint32_t index;
    const uint32_t count = ramfs_count();

    for (index = 0U; index < count; ++index) {
        struct ramfs_file_info info;

        if (!ramfs_get_info(index, &info)) {
            return false;
        }
        if (strings_equal(info.name, name)) {
            *size = info.size;
            return true;
        }
    }
    return false;
}

static void execute_ls(void)
{
    uint32_t index;
    const uint32_t count = ramfs_count();

    if (count == 0U) {
        console_write("(empty)\n");
        return;
    }

    for (index = 0U; index < count; ++index) {
        struct ramfs_file_info info;

        if (!ramfs_get_info(index, &info)) {
            console_write("error: ramfs state\n");
            return;
        }
        console_write(info.name);
        console_putc(' ');
        console_write_u32_dec(info.size);
        console_putc('\n');
    }
}

static void execute_cat(const char *name)
{
    uint8_t data[RAMFS_FILE_MAX_BYTES];
    uint32_t length;
    uint32_t index;
    enum ramfs_result result = ramfs_read(
        name,
        data,
        sizeof(data),
        &length
    );

    if (result != RAMFS_RESULT_OK) {
        print_ramfs_result(result);
        return;
    }
    for (index = 0U; index < length; ++index) {
        console_putc((char)data[index]);
    }
    console_putc('\n');
}

static void execute_command(const struct shell_command *command)
{
    char text[SHELL_TEXT_CAPACITY];
    uint32_t text_length;
    enum ramfs_result result;

    switch (command->kind) {
    case SHELL_COMMAND_HELP:
        console_write(shell_command_help());
        console_putc('\n');
        return;
    case SHELL_COMMAND_CLEAR:
        console_clear();
        return;
    case SHELL_COMMAND_ECHO:
        if (!join_arguments(
                command,
                0U,
                false,
                text,
                sizeof(text),
                &text_length)) {
            console_write("error: text too long\n");
            return;
        }
        console_write(text);
        console_putc('\n');
        return;
    case SHELL_COMMAND_LS:
        execute_ls();
        return;
    case SHELL_COMMAND_CAT:
        execute_cat(command->arguments[0]);
        return;
    case SHELL_COMMAND_TOUCH:
        result = ramfs_create(command->arguments[0]);
        break;
    case SHELL_COMMAND_WRITE:
        if (!join_arguments(
                command,
                1U,
                false,
                text,
                sizeof(text),
                &text_length)) {
            console_write("error: text too long\n");
            return;
        }
        result = ramfs_write(
            command->arguments[0],
            (const uint8_t *)text,
            text_length
        );
        break;
    case SHELL_COMMAND_APPEND:
    {
        uint32_t existing_size = 0U;
        const bool found = file_size(command->arguments[0], &existing_size);

        if (!join_arguments(
                command,
                1U,
                found && existing_size != 0U,
                text,
                sizeof(text),
                &text_length)) {
            console_write("error: text too long\n");
            return;
        }
        result = ramfs_append(
            command->arguments[0],
            (const uint8_t *)text,
            text_length
        );
        break;
    }
    case SHELL_COMMAND_RM:
        result = ramfs_remove(command->arguments[0]);
        break;
    case SHELL_COMMAND_STATUS:
        print_status();
        return;
    case SHELL_COMMAND_ABOUT:
        console_write(
            "TinyShell OS is an i386 teaching microkernel prototype. "
            "Shell and RAMFS currently run in Ring 0.\n"
        );
        return;
    case SHELL_COMMAND_UNKNOWN:
    default:
        console_write("error: unknown command\n");
        return;
    }

    if (result == RAMFS_RESULT_OK) {
        console_write("ok\n");
    } else {
        print_ramfs_result(result);
    }
}

bool shell_runtime_init(void)
{
    runtime_initialized = false;
    runtime_started = false;

    if (!ramfs_self_test()
        || !line_editor_self_test()
        || !parser_self_test()) {
        return false;
    }

    ramfs_init();
    shell_line_editor_init(&runtime_editor);
    runtime_initialized = true;
    return true;
}

void shell_runtime_start(void)
{
    if (!runtime_initialized || runtime_started) {
        return;
    }

    runtime_started = true;
    console_write("TinyShell OS interactive shell (Ring 0)\n");
    print_prompt();
}

void shell_runtime_handle_char(char character)
{
    enum shell_line_event event;

    if (!runtime_initialized || !runtime_started) {
        return;
    }

    event = shell_line_editor_feed(&runtime_editor, character);
    switch (event) {
    case SHELL_LINE_ECHO:
        console_putc(character);
        return;
    case SHELL_LINE_ERASE:
        console_putc('\b');
        return;
    case SHELL_LINE_READY:
    {
        char line[SHELL_LINE_MAX + 1];
        struct shell_command command;
        enum shell_parse_result parse_result;

        console_putc('\n');
        if (!shell_line_editor_take(&runtime_editor, line, sizeof(line))) {
            console_write("error: line unavailable\n");
            shell_line_editor_init(&runtime_editor);
            print_prompt();
            return;
        }

        parse_result = shell_command_parse(line, &command);
        if (parse_result == SHELL_PARSE_EMPTY) {
            print_prompt();
            return;
        }
        if (parse_result == SHELL_PARSE_TOO_MANY_ARGUMENTS) {
            console_write("error: too many arguments\n");
            print_prompt();
            return;
        }
        if (parse_result == SHELL_PARSE_ARGUMENT_TOO_LONG) {
            console_write("error: argument too long\n");
            print_prompt();
            return;
        }
        if (parse_result != SHELL_PARSE_OK) {
            console_write("error: invalid command line\n");
            print_prompt();
            return;
        }
        if (command.kind == SHELL_COMMAND_UNKNOWN) {
            console_write("error: unknown command\n");
            print_prompt();
            return;
        }
        if (!shell_command_has_valid_arity(&command)) {
            console_write("error: usage: ");
            console_write(shell_command_usage(command.kind));
            console_putc('\n');
            print_prompt();
            return;
        }

        execute_command(&command);
        print_prompt();
        return;
    }
    case SHELL_LINE_NONE:
    case SHELL_LINE_FULL:
    default:
        return;
    }
}
