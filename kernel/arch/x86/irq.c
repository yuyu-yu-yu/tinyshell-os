#include "arch/x86/irq.h"

#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/pic.h"

#include <stdint.h>

enum {
    IRQ_COUNT = 16,
    IRQ_VECTOR_BASE = 32,
    INTERRUPT_GATE = 0x8E,
};

static irq_handler_t handlers[IRQ_COUNT];
static uint32_t counts[IRQ_COUNT];

extern uint32_t irq_stub_table[IRQ_COUNT];

void irq_init(void)
{
    pic_init();

    for (uint32_t irq = 0; irq < IRQ_COUNT; ++irq) {
        handlers[irq] = 0;
        counts[irq] = 0;
        idt_set_gate(
            (uint8_t)(IRQ_VECTOR_BASE + irq),
            irq_stub_table[irq],
            GDT_KERNEL_CODE_SELECTOR,
            INTERRUPT_GATE
        );
    }
}

bool irq_register_handler(uint8_t irq, irq_handler_t handler)
{
    if (irq >= IRQ_COUNT || handler == 0 || handlers[irq] != 0) {
        return false;
    }

    handlers[irq] = handler;
    return true;
}

bool irq_unregister_handler(uint8_t irq)
{
    if (irq >= IRQ_COUNT || handlers[irq] == 0) {
        return false;
    }

    handlers[irq] = 0;
    return true;
}

bool irq_set_enabled(uint8_t irq, bool enabled)
{
    return pic_set_mask(irq, !enabled);
}

uint32_t irq_count(uint8_t irq)
{
    if (irq >= IRQ_COUNT) {
        return 0;
    }

    return counts[irq];
}

void irq_dispatch(uint8_t irq)
{
    irq_handler_t handler;

    if (irq >= IRQ_COUNT) {
        return;
    }

    counts[irq] += 1U;
    handler = handlers[irq];
    if (handler != 0) {
        handler();
    }
    pic_send_eoi(irq);
}
