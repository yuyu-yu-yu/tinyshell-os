#include "console.h"
#include "serial.h"

#include <stddef.h>
#include <stdint.h>

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_COLOR_LIGHT_GREY_ON_BLACK = 0x07,
    VGA_BUFFER_CELLS = VGA_WIDTH * VGA_HEIGHT,
};

/*
 * Invariants:
 * - vga_row is in [0, VGA_HEIGHT)
 * - vga_column is in [0, VGA_WIDTH)
 * - every VGA store uses index < VGA_BUFFER_CELLS
 * Failure paths:
 * - console_write(NULL) is a no-op
 * - backspace at (0, 0) is a no-op
 * - non-printable bytes other than \n, \r, \b are ignored
 * - COM1 transmit spins until the line status register reports ready
 */
static volatile uint16_t *const vga_buffer = (uint16_t *)0xB8000;
static size_t vga_row;
static size_t vga_column;

static uint16_t vga_entry(char value)
{
    return (uint16_t)(uint8_t)value
        | ((uint16_t)VGA_COLOR_LIGHT_GREY_ON_BLACK << 8);
}

static uint16_t vga_blank(void)
{
    return vga_entry(' ');
}

static void vga_put_cell(size_t row, size_t column, uint16_t entry)
{
    const size_t index = row * VGA_WIDTH + column;

    if (index >= VGA_BUFFER_CELLS) {
        return;
    }

    vga_buffer[index] = entry;
}

static void vga_scroll(void)
{
    for (size_t row = 0; row < (size_t)VGA_HEIGHT - 1U; ++row) {
        for (size_t column = 0; column < VGA_WIDTH; ++column) {
            vga_put_cell(row, column,
                vga_buffer[(row + 1U) * VGA_WIDTH + column]);
        }
    }

    for (size_t column = 0; column < VGA_WIDTH; ++column) {
        vga_put_cell((size_t)VGA_HEIGHT - 1U, column, vga_blank());
    }

    vga_row = (size_t)VGA_HEIGHT - 1U;
}

static void vga_advance_line(void)
{
    vga_column = 0;
    ++vga_row;
    if (vga_row == VGA_HEIGHT) {
        vga_scroll();
    }
}

static void vga_putc(char value)
{
    if (value == '\n') {
        vga_advance_line();
        return;
    }

    if (value == '\r') {
        vga_column = 0;
        return;
    }

    if (value == '\b') {
        if (vga_column > 0U) {
            --vga_column;
        } else if (vga_row > 0U) {
            --vga_row;
            vga_column = (size_t)VGA_WIDTH - 1U;
        } else {
            return;
        }

        vga_put_cell(vga_row, vga_column, vga_blank());
        return;
    }

    if ((unsigned char)value < 0x20U || (unsigned char)value > 0x7EU) {
        return;
    }

    vga_put_cell(vga_row, vga_column, vga_entry(value));
    ++vga_column;
    if (vga_column == VGA_WIDTH) {
        vga_advance_line();
    }
}

static void serial_putc(char value)
{
    char buffer[2];

    buffer[0] = value;
    buffer[1] = '\0';
    serial_write(buffer);
}

void console_init(void)
{
    serial_init();
    console_clear();
}

void console_clear(void)
{
    const uint16_t blank = vga_blank();

    for (size_t row = 0; row < VGA_HEIGHT; ++row) {
        for (size_t column = 0; column < VGA_WIDTH; ++column) {
            vga_put_cell(row, column, blank);
        }
    }

    vga_row = 0;
    vga_column = 0;
}

void console_putc(char value)
{
    vga_putc(value);
    serial_putc(value);
}

void console_write(const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        console_putc(*text);
        ++text;
    }
}

void console_write_u32_hex(uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    int nibble;

    console_write("0x");
    for (nibble = 7; nibble >= 0; --nibble) {
        console_putc(digits[(value >> (nibble * 4)) & 0xFU]);
    }
}

void console_write_u32_dec(uint32_t value)
{
    char digits[10];
    size_t count = 0;

    if (value == 0U) {
        console_putc('0');
        return;
    }

    while (value > 0U) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        ++count;
    }

    while (count > 0U) {
        --count;
        console_putc(digits[count]);
    }
}
