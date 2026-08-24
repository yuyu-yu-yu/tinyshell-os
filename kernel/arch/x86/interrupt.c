#include "arch/x86/interrupt.h"

#include "arch/x86/irq.h"
#include "arch/x86/paging.h"
#include "console.h"

void interrupt_dispatch(struct interrupt_frame *frame)
{
    if (frame->vector >= 32U && frame->vector < 48U) {
        irq_dispatch((uint8_t)(frame->vector - 32U));
        return;
    }

    if (frame->vector == 14U) {
        paging_handle_page_fault(frame);
    }

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
