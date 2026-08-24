#include "arch/x86/pic.h"

#include "io.h"

enum {
    PIC_MASTER_COMMAND = 0x20,
    PIC_MASTER_DATA = 0x21,
    PIC_SLAVE_COMMAND = 0xA0,
    PIC_SLAVE_DATA = 0xA1,
    PIC_ICW1_INIT = 0x11,
    PIC_ICW4_8086 = 0x01,
    PIC_MASTER_VECTOR_OFFSET = 0x20,
    PIC_SLAVE_VECTOR_OFFSET = 0x28,
    PIC_MASTER_CASCADE_IRQ = 0x04,
    PIC_SLAVE_CASCADE_ID = 0x02,
    PIC_ALL_MASKED = 0xFF,
    PIC_SLAVE_IRQ_BASE = 8,
};

static uint8_t master_mask = PIC_ALL_MASKED;
static uint8_t slave_mask = PIC_ALL_MASKED;

static void pic_write_masks(void)
{
    outb(PIC_MASTER_DATA, master_mask);
    outb(PIC_SLAVE_DATA, slave_mask);
}

void pic_init(void)
{
    outb(PIC_MASTER_COMMAND, PIC_ICW1_INIT);
    outb(PIC_SLAVE_COMMAND, PIC_ICW1_INIT);

    outb(PIC_MASTER_DATA, PIC_MASTER_VECTOR_OFFSET);
    outb(PIC_SLAVE_DATA, PIC_SLAVE_VECTOR_OFFSET);

    outb(PIC_MASTER_DATA, PIC_MASTER_CASCADE_IRQ);
    outb(PIC_SLAVE_DATA, PIC_SLAVE_CASCADE_ID);

    outb(PIC_MASTER_DATA, PIC_ICW4_8086);
    outb(PIC_SLAVE_DATA, PIC_ICW4_8086);

    master_mask = PIC_ALL_MASKED;
    slave_mask = PIC_ALL_MASKED;
    pic_write_masks();
}

bool pic_set_mask(uint8_t irq, bool masked)
{
    uint8_t *mask;
    uint8_t port;
    uint8_t bit;

    if (irq >= 16U) {
        return false;
    }

    if (irq < PIC_SLAVE_IRQ_BASE) {
        mask = &master_mask;
        port = PIC_MASTER_DATA;
        bit = irq;
    } else {
        mask = &slave_mask;
        port = PIC_SLAVE_DATA;
        bit = (uint8_t)(irq - PIC_SLAVE_IRQ_BASE);
    }

    if (masked) {
        *mask = (uint8_t)(*mask | (uint8_t)(1U << bit));
    } else {
        *mask = (uint8_t)(*mask & (uint8_t)~(1U << bit));
    }
    outb(port, *mask);
    return true;
}

uint16_t pic_get_mask(void)
{
    return (uint16_t)master_mask | ((uint16_t)slave_mask << 8);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 16U) {
        return;
    }

    if (irq >= PIC_SLAVE_IRQ_BASE) {
        outb(PIC_SLAVE_COMMAND, 0x20);
    }
    outb(PIC_MASTER_COMMAND, 0x20);
}
