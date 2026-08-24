#include "arch/x86/paging.h"

#include "arch/x86/interrupt.h"
#include "console.h"

#include <stdint.h>

void paging_load_directory(uintptr_t directory_address)
{
    __asm__ volatile ("movl %0, %%cr3" : : "r"(directory_address) : "memory");
}

void paging_enable(void)
{
    uint32_t control;

    __asm__ volatile ("movl %%cr0, %0" : "=r"(control));
    control |= 1U << 31;
    __asm__ volatile ("movl %0, %%cr0" : : "r"(control) : "memory");
}

uintptr_t paging_fault_address(void)
{
    uintptr_t address;

    __asm__ volatile ("movl %%cr2, %0" : "=r"(address));
    return address;
}

void paging_invalidate(uintptr_t virtual_address)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

_Noreturn void paging_handle_page_fault(const struct interrupt_frame *frame)
{
    const uint32_t error = frame->error_code;

    console_write("PAGE_FAULT address=");
    console_write_u32_hex((uint32_t)paging_fault_address());
    console_write(" error=");
    console_write_u32_hex(error);
    console_write(" present=");
    console_write_u32_dec(error & 1U);
    console_write(" write=");
    console_write_u32_dec((error >> 1U) & 1U);
    console_write(" user=");
    console_write_u32_dec((error >> 2U) & 1U);
    console_putc('\n');

    __asm__ volatile ("cli" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
