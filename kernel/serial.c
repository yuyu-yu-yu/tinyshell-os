#include "io.h"
#include "serial.h"

#include <stdint.h>

enum {
    COM1 = 0x3F8,
};

static int transmit_ready(void)
{
    return (inb(COM1 + 5) & 0x20U) != 0;
}

static void serial_write_char(char value)
{
    while (!transmit_ready()) {
    }

    outb(COM1, (uint8_t)value);
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*text);
        ++text;
    }
}

