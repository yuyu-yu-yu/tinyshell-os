#include "arch/x86/interrupt.h"

#include "io.h"

void interrupt_dispatch(struct interrupt_frame *frame)
{
    if (frame->vector == 3U) {
        return;
    }

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
