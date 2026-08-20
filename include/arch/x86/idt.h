#ifndef TINYOS_X86_IDT_H
#define TINYOS_X86_IDT_H

#include <stdint.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t reserved;
    uint8_t type_attributes;
    uint16_t offset_high;
} __attribute__((packed));

struct idtr_descriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void idt_set_gate(
    uint8_t vector,
    uint32_t handler,
    uint16_t selector,
    uint8_t type_attributes
);

void idt_init(void);

#endif
