#ifndef TINYOS_BOOT_MULTIBOOT_H
#define TINYOS_BOOT_MULTIBOOT_H

#include <stdbool.h>
#include <stdint.h>

enum {
    MULTIBOOT_BOOTLOADER_MAGIC = 0x2BADB002U,
};

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t symbols[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

struct boot_memory_summary {
    uint32_t lower_kib;
    uint32_t upper_kib;
    uint32_t mmap_entry_count;
};

bool multiboot_parse(
    uint32_t magic,
    uint32_t info_address,
    struct boot_memory_summary *summary
);

#endif
