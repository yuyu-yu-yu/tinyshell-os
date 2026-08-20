#ifndef TINYOS_CONSOLE_H
#define TINYOS_CONSOLE_H

#include <stdint.h>

/*
 * Shared early-boot console: VGA text at 0xB8000 and COM1 serial.
 *
 * Day-2 integration (member A): call console_init() first, then print
 * CONSOLE_OK with console_write(). Do not replace kernel.c VGA on day 1.
 */
void console_init(void);
void console_clear(void);
void console_putc(char value);
void console_write(const char *text);
void console_write_u32_hex(uint32_t value);
void console_write_u32_dec(uint32_t value);

#endif
