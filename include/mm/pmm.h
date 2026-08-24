#ifndef TINYOS_MM_PMM_H
#define TINYOS_MM_PMM_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Physical page allocator for the 32-bit identity-mapped boot stage.
 *
 * This module does not parse Multiboot structures. Member A walks the
 * memory map and feeds clipped regions through the functions below.
 *
 * Ownership protocol (day 2, member A):
 *   pmm_reset
 *   pmm_add_usable_region for every type == 1 range
 *   pmm_reserve_region for 0-1 MiB, the kernel image, and Multiboot buffers
 *   then pmm_alloc_page / pmm_free_page
 *
 * After the first pmm_alloc_page() call, add/reserve are rejected so that
 * page ownership cannot change under live allocations. These setup calls
 * are not interrupt-safe and must run with IRQs masked during init.
 *
 * The allocator never zeroes a returned page, never dereferences a physical
 * address, and does not provide contiguous multi-page, paging, or malloc.
 */
enum {
    PMM_PAGE_SIZE = 4096,
};

void pmm_reset(void);
bool pmm_add_usable_region(uint64_t base, uint64_t length);
bool pmm_reserve_region(uint64_t base, uint64_t length);
uintptr_t pmm_alloc_page(void);
bool pmm_free_page(uintptr_t physical_address);
uint32_t pmm_free_page_count(void);
uint32_t pmm_total_page_count(void);

#endif
