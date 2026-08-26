#ifndef TINYOS_SHELL_RUNTIME_H
#define TINYOS_SHELL_RUNTIME_H

#include <stdbool.h>

bool shell_runtime_init(void);
void shell_runtime_start(void);
void shell_runtime_handle_char(char character);

#endif
