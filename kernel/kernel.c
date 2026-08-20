#include "boot/multiboot.h"
#include "serial.h"

#include <stddef.h>
#include <stdint.h>

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_COLOR_LIGHT_GREY_ON_BLACK = 0x07,
};

static volatile uint16_t *const vga_buffer = (uint16_t *)0xB8000;
static size_t vga_row;
static size_t vga_column;

static void vga_clear(void)
{
    const uint16_t blank = (uint16_t)' '
        | ((uint16_t)VGA_COLOR_LIGHT_GREY_ON_BLACK << 8);

    for (size_t row = 0; row < VGA_HEIGHT; ++row) {
        for (size_t column = 0; column < VGA_WIDTH; ++column) {
            vga_buffer[row * VGA_WIDTH + column] = blank;
        }
    }

    vga_row = 0;
    vga_column = 0;
}

static void vga_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            vga_column = 0;
            ++vga_row;
        } else {
            const uint16_t entry = (uint16_t)(uint8_t)*text
                | ((uint16_t)VGA_COLOR_LIGHT_GREY_ON_BLACK << 8);
            vga_buffer[vga_row * VGA_WIDTH + vga_column] = entry;
            ++vga_column;
            if (vga_column == VGA_WIDTH) {
                vga_column = 0;
                ++vga_row;
            }
        }

        if (vga_row == VGA_HEIGHT) {
            vga_row = 0;
        }
        ++text;
    }
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_address)
{
    serial_init();
    vga_clear();

    struct boot_memory_summary memory_summary;
    if (!multiboot_parse(
            multiboot_magic,
            multiboot_info_address,
            &memory_summary)) {
        serial_write("TinyShell OS: invalid Multiboot information\n");
        vga_write("TinyShell OS: invalid Multiboot information\n");
        return;
    }

    serial_write("TinyShell OS booting...\n");
    serial_write("Architecture: i386\n");
    serial_write("MULTIBOOT_OK\n");
    serial_write("MEMORY_MAP_OK\n");
    serial_write("BOOT_OK\n");

    vga_write("TinyShell OS booting...\n");
    vga_write("Architecture: i386\n");
    vga_write("MULTIBOOT_OK\n");
    vga_write("MEMORY_MAP_OK\n");
    vga_write("BOOT_OK\n");
}
