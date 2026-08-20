#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"

#include <stddef.h>
#include <stdint.h>

enum {
    IDT_ENTRY_COUNT = 256,
    CPU_EXCEPTION_COUNT = 32,
    INTERRUPT_GATE = 0x8E,
};

static struct idt_entry idt[IDT_ENTRY_COUNT];
static struct idtr_descriptor idtr;

extern uint32_t isr_stub_table[CPU_EXCEPTION_COUNT];
extern void idt_load(const struct idtr_descriptor *descriptor);

_Static_assert(sizeof(struct idt_entry) == 8U, "IDT entries must be 8 bytes");
_Static_assert(
    sizeof(struct idtr_descriptor) == 6U,
    "IDTR descriptor must be 6 bytes"
);

void idt_set_gate(
    uint8_t vector,
    uint32_t handler,
    uint16_t selector,
    uint8_t type_attributes
)
{
    struct idt_entry *entry = &idt[vector];

    entry->offset_low = (uint16_t)(handler & 0xFFFFU);
    entry->selector = selector;
    entry->reserved = 0;
    entry->type_attributes = type_attributes;
    entry->offset_high = (uint16_t)((handler >> 16) & 0xFFFFU);
}

void idt_init(void)
{
    for (uint32_t vector = 0; vector < IDT_ENTRY_COUNT; ++vector) {
        idt_set_gate((uint8_t)vector, 0, 0, 0);
    }

    for (uint32_t vector = 0; vector < CPU_EXCEPTION_COUNT; ++vector) {
        idt_set_gate(
            (uint8_t)vector,
            isr_stub_table[vector],
            GDT_KERNEL_CODE_SELECTOR,
            INTERRUPT_GATE
        );
    }

    idtr.limit = (uint16_t)(sizeof(idt) - 1U);
    idtr.base = (uint32_t)(uintptr_t)idt;
    idt_load(&idtr);
}
