#ifndef TINYOS_X86_PAGING_H
#define TINYOS_X86_PAGING_H

#include <stdint.h>

struct interrupt_frame;

void paging_load_directory(uintptr_t directory_address);
void paging_enable(void);
uintptr_t paging_fault_address(void);
void paging_invalidate(uintptr_t virtual_address);
_Noreturn void paging_handle_page_fault(const struct interrupt_frame *frame);

#endif
