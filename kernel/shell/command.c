#include "shell/command.h"

#include <stdbool.h>
#include <stdint.h>

static bool is_separator(char character)
{
    return character == ' ' || character == '\t';
}

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

static enum shell_command_kind command_kind(const char *name)
{
    if (strings_equal(name, "help")) {
        return SHELL_COMMAND_HELP;
    }
    if (strings_equal(name, "clear")) {
        return SHELL_COMMAND_CLEAR;
    }
    if (strings_equal(name, "echo")) {
        return SHELL_COMMAND_ECHO;
    }
    if (strings_equal(name, "ls")) {
        return SHELL_COMMAND_LS;
    }
    if (strings_equal(name, "cat")) {
        return SHELL_COMMAND_CAT;
    }
    if (strings_equal(name, "touch")) {
        return SHELL_COMMAND_TOUCH;
    }
    if (strings_equal(name, "write")) {
        return SHELL_COMMAND_WRITE;
    }
    if (strings_equal(name, "append")) {
        return SHELL_COMMAND_APPEND;
    }
    if (strings_equal(name, "rm")) {
        return SHELL_COMMAND_RM;
    }
    if (strings_equal(name, "status")) {
        return SHELL_COMMAND_STATUS;
    }
    if (strings_equal(name, "about")) {
        return SHELL_COMMAND_ABOUT;
    }
    return SHELL_COMMAND_UNKNOWN;
}

static void command_clear(struct shell_command *command)
{
    uint32_t argument;
    uint32_t index;

    command->kind = SHELL_COMMAND_UNKNOWN;
    command->argument_count = 0U;
    for (index = 0U; index <= SHELL_COMMAND_ARG_MAX; ++index) {
        command->name[index] = '\0';
    }
    for (argument = 0U; argument < SHELL_COMMAND_MAX_ARGS; ++argument) {
        for (index = 0U; index <= SHELL_COMMAND_ARG_MAX; ++index) {
            command->arguments[argument][index] = '\0';
        }
    }
}

static enum shell_parse_result copy_token(
    const char **cursor,
    char destination[SHELL_COMMAND_ARG_MAX + 1]
)
{
    uint32_t length = 0U;

    while (**cursor != '\0' && !is_separator(**cursor)) {
        if (length >= SHELL_COMMAND_ARG_MAX) {
            return SHELL_PARSE_ARGUMENT_TOO_LONG;
        }
        destination[length] = **cursor;
        length += 1U;
        *cursor += 1;
    }
    destination[length] = '\0';
    return SHELL_PARSE_OK;
}

enum shell_parse_result shell_command_parse(
    const char *line,
    struct shell_command *command
)
{
    struct shell_command parsed;
    const char *cursor;
    enum shell_parse_result result;

    if (line == 0 || command == 0) {
        return SHELL_PARSE_INVALID_ARGUMENT;
    }

    cursor = line;
    while (is_separator(*cursor)) {
        cursor += 1;
    }
    if (*cursor == '\0') {
        return SHELL_PARSE_EMPTY;
    }

    command_clear(&parsed);
    result = copy_token(&cursor, parsed.name);
    if (result != SHELL_PARSE_OK) {
        return result;
    }
    parsed.kind = command_kind(parsed.name);

    for (;;) {
        while (is_separator(*cursor)) {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        if (parsed.argument_count >= SHELL_COMMAND_MAX_ARGS) {
            return SHELL_PARSE_TOO_MANY_ARGUMENTS;
        }

        result = copy_token(
            &cursor,
            parsed.arguments[parsed.argument_count]
        );
        if (result != SHELL_PARSE_OK) {
            return result;
        }
        parsed.argument_count += 1U;
    }

    *command = parsed;
    return SHELL_PARSE_OK;
}

bool shell_command_has_valid_arity(const struct shell_command *command)
{
    if (command == 0) {
        return false;
    }

    switch (command->kind) {
    case SHELL_COMMAND_HELP:
    case SHELL_COMMAND_CLEAR:
    case SHELL_COMMAND_LS:
    case SHELL_COMMAND_STATUS:
    case SHELL_COMMAND_ABOUT:
        return command->argument_count == 0U;
    case SHELL_COMMAND_CAT:
    case SHELL_COMMAND_TOUCH:
    case SHELL_COMMAND_RM:
        return command->argument_count == 1U;
    case SHELL_COMMAND_WRITE:
    case SHELL_COMMAND_APPEND:
        return command->argument_count >= 2U &&
               command->argument_count <= SHELL_COMMAND_MAX_ARGS;
    case SHELL_COMMAND_ECHO:
        return command->argument_count <= SHELL_COMMAND_MAX_ARGS;
    case SHELL_COMMAND_UNKNOWN:
    default:
        return false;
    }
}

const char *shell_command_usage(enum shell_command_kind kind)
{
    switch (kind) {
    case SHELL_COMMAND_HELP:
        return "help";
    case SHELL_COMMAND_CLEAR:
        return "clear";
    case SHELL_COMMAND_ECHO:
        return "echo [text ...]";
    case SHELL_COMMAND_LS:
        return "ls";
    case SHELL_COMMAND_CAT:
        return "cat <name>";
    case SHELL_COMMAND_TOUCH:
        return "touch <name>";
    case SHELL_COMMAND_WRITE:
        return "write <name> <text ...>";
    case SHELL_COMMAND_APPEND:
        return "append <name> <text ...>";
    case SHELL_COMMAND_RM:
        return "rm <name>";
    case SHELL_COMMAND_STATUS:
        return "status";
    case SHELL_COMMAND_ABOUT:
        return "about";
    case SHELL_COMMAND_UNKNOWN:
    default:
        return 0;
    }
}

const char *shell_command_help(void)
{
    return
        "help\n"
        "clear\n"
        "echo [text ...]\n"
        "ls\n"
        "cat <name>\n"
        "touch <name>\n"
        "write <name> <text ...>\n"
        "append <name> <text ...>\n"
        "rm <name>\n"
        "status\n"
        "about";
}
