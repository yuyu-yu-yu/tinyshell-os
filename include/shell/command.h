#ifndef TINYOS_SHELL_COMMAND_H
#define TINYOS_SHELL_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

enum {
    SHELL_COMMAND_MAX_ARGS = 8,
    SHELL_COMMAND_ARG_MAX = 31,
};

enum shell_command_kind {
    SHELL_COMMAND_HELP,
    SHELL_COMMAND_CLEAR,
    SHELL_COMMAND_ECHO,
    SHELL_COMMAND_LS,
    SHELL_COMMAND_CAT,
    SHELL_COMMAND_TOUCH,
    SHELL_COMMAND_WRITE,
    SHELL_COMMAND_APPEND,
    SHELL_COMMAND_RM,
    SHELL_COMMAND_STATUS,
    SHELL_COMMAND_ABOUT,
    SHELL_COMMAND_UNKNOWN,
};

enum shell_parse_result {
    SHELL_PARSE_OK,
    SHELL_PARSE_EMPTY,
    SHELL_PARSE_INVALID_ARGUMENT,
    SHELL_PARSE_TOO_MANY_ARGUMENTS,
    SHELL_PARSE_ARGUMENT_TOO_LONG,
};

struct shell_command {
    enum shell_command_kind kind;
    char name[SHELL_COMMAND_ARG_MAX + 1];
    uint32_t argument_count;
    char arguments[SHELL_COMMAND_MAX_ARGS][SHELL_COMMAND_ARG_MAX + 1];
};

enum shell_parse_result shell_command_parse(
    const char *line,
    struct shell_command *command
);

bool shell_command_has_valid_arity(
    const struct shell_command *command
);

const char *shell_command_usage(enum shell_command_kind kind);
const char *shell_command_help(void);

#endif
