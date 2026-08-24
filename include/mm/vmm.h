#ifndef TINYOS_MM_VMM_H
#define TINYOS_MM_VMM_H

#include <stdbool.h>
#include <stdint.h>

enum {
    VMM_PAGE_SIZE = 4096,
    VMM_IDENTITY_LIMIT = 0x08000000,
    VMM_DYNAMIC_START = 0x40000000,
    VMM_DYNAMIC_END = 0xF0000000,
};

enum vmm_page_flags {
    VMM_WRITABLE = 1U << 0,
    VMM_USER = 1U << 1,
};

bool vmm_init(void);
bool vmm_map_page(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    uint32_t flags
);
bool vmm_unmap_page(
    uintptr_t virtual_address,
    uintptr_t *physical_address
);
bool vmm_translate(
    uintptr_t virtual_address,
    uintptr_t *physical_address
);
bool vmm_is_enabled(void);

#endif
