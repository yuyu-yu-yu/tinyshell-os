#ifndef TINYOS_X86_INTERRUPT_H
#define TINYOS_X86_INTERRUPT_H

#include <stdint.h>

struct interrupt_frame {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t saved_esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t vector;
    uint32_t error_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
};

void interrupt_dispatch(struct interrupt_frame *frame);

#endif
