#include "arch/x86/interrupt.h"

#include "console.h"

void interrupt_dispatch(struct interrupt_frame *frame)
{
    console_write("EXCEPTION vector=");
    console_write_u32_dec(frame->vector);
    console_putc('\n');

    if (frame->vector == 3U) {
        return;
    }

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
