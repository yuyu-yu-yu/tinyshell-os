#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "boot/multiboot.h"
#include "console.h"

#include <stdint.h>

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_address)
{
    console_init();
    console_write("TinyShell OS booting...\n");
    console_write("Architecture: i386\n");
    console_write("CONSOLE_OK\n");

    gdt_init();
    console_write("GDT_OK\n");

    idt_init();
    console_write("IDT_OK\n");

    struct boot_memory_summary memory_summary;
    if (!multiboot_parse(
            multiboot_magic,
            multiboot_info_address,
            &memory_summary)) {
        console_write("TinyShell OS: invalid Multiboot information\n");
        return;
    }

    console_write("MULTIBOOT_OK\n");
    console_write("MEMORY_MAP_OK\n");

    __asm__ volatile ("int3");
    console_write("INT3_TEST_OK\n");
    console_write("BOOT_OK\n");
}
