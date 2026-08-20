#ifndef TINYOS_ARCH_X86_GDT_H
#define TINYOS_ARCH_X86_GDT_H

#include <stdint.h>

enum {
    GDT_NULL_SELECTOR = 0x00,
    GDT_KERNEL_CODE_SELECTOR = 0x08,
    GDT_KERNEL_DATA_SELECTOR = 0x10,
    GDT_USER_CODE_SELECTOR = 0x1B,
    GDT_USER_DATA_SELECTOR = 0x23,
};

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_descriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void gdt_init(void);

#endif
