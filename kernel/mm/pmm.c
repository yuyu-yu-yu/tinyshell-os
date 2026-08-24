#include "mm/pmm.h"

#include <stddef.h>
#include <stdint.h>

enum {
    PMM_PAGE_SHIFT = 12,
    PMM_PAGE_COUNT = 1048576U,
    PMM_BITMAP_WORDS = PMM_PAGE_COUNT / 32U,
    PMM_PAGE_MASK = PMM_PAGE_SIZE - 1U
};

static const uint64_t PMM_PHYSICAL_LIMIT = UINT64_C(1) << 32;

_Static_assert(
    PMM_PAGE_COUNT == 1048576U,
    "PMM must cover every 4 KiB page in the 32-bit physical space"
);
_Static_assert(
    PMM_BITMAP_WORDS == 32768U,
    "each PMM bitmap is 128 KiB"
);

/*
 * Two bitmaps encode four page states:
 *   managed=0 allocated=0  unknown / never described
 *   managed=1 allocated=0  free (allocatable)
 *   managed=1 allocated=1  currently allocated
 *   managed=0 allocated=1  reserved (includes page 0 forever)
 *
 * Reserved pages stay unallocatable and cannot be freed. Duplicate frees
 * of a free managed page are rejected because allocated=0.
 */
static uint32_t managed_pages[PMM_BITMAP_WORDS];
static uint32_t allocated_pages[PMM_BITMAP_WORDS];
static uint32_t total_pages;
static uint32_t free_pages;
static bool ownership_frozen;

static uint32_t bitmap_word(uint32_t page)
{
    return page / 32U;
}

static uint32_t bitmap_mask(uint32_t page)
{
    return 1U << (page % 32U);
}

static bool page_is_managed(uint32_t page)
{
    return (managed_pages[bitmap_word(page)] & bitmap_mask(page)) != 0U;
}

static bool page_is_allocated(uint32_t page)
{
    return (allocated_pages[bitmap_word(page)] & bitmap_mask(page)) != 0U;
}

static bool page_is_reserved(uint32_t page)
{
    return !page_is_managed(page) && page_is_allocated(page);
}

static void set_managed(uint32_t page, bool value)
{
    const uint32_t word = bitmap_word(page);
    const uint32_t mask = bitmap_mask(page);

    if (value) {
        managed_pages[word] |= mask;
    } else {
        managed_pages[word] &= ~mask;
    }
}

static void set_allocated(uint32_t page, bool value)
{
    const uint32_t word = bitmap_word(page);
    const uint32_t mask = bitmap_mask(page);

    if (value) {
        allocated_pages[word] |= mask;
    } else {
        allocated_pages[word] &= ~mask;
    }
}

static void mark_page_reserved(uint32_t page)
{
    if (page_is_reserved(page)) {
        return;
    }

    if (page_is_managed(page)) {
        if (page_is_allocated(page)) {
            /*
             * A live allocation cannot be silently un-owned. The ownership
             * freeze should have prevented this path; keep the page allocated.
             */
            return;
        }

        set_managed(page, false);
        set_allocated(page, true);
        total_pages -= 1U;
        free_pages -= 1U;
        return;
    }

    set_allocated(page, true);
}

static void mark_page_usable(uint32_t page)
{
    if (page == 0U || page_is_reserved(page) || page_is_managed(page)) {
        return;
    }

    set_managed(page, true);
    set_allocated(page, false);
    total_pages += 1U;
    free_pages += 1U;
}

static bool clip_to_physical(
    uint64_t base,
    uint64_t length,
    uint64_t *clipped_base,
    uint64_t *clipped_end
)
{
    uint64_t end;

    if (clipped_base == NULL || clipped_end == NULL) {
        return false;
    }

    if (length == 0U) {
        *clipped_base = 0U;
        *clipped_end = 0U;
        return true;
    }

    if (base > UINT64_MAX - length) {
        return false;
    }

    end = base + length;
    if (base >= PMM_PHYSICAL_LIMIT) {
        *clipped_base = 0U;
        *clipped_end = 0U;
        return true;
    }

    if (end > PMM_PHYSICAL_LIMIT) {
        end = PMM_PHYSICAL_LIMIT;
    }

    *clipped_base = base;
    *clipped_end = end;
    return true;
}

static void inward_page_span(
    uint64_t base,
    uint64_t end,
    uint32_t *first,
    uint32_t *last_excl
)
{
    const uint64_t aligned_start =
        (base + (uint64_t)PMM_PAGE_MASK) & ~(uint64_t)PMM_PAGE_MASK;
    const uint64_t aligned_end = end & ~(uint64_t)PMM_PAGE_MASK;
    uint32_t start_page;
    uint32_t end_page;

    *first = 0U;
    *last_excl = 0U;

    if (aligned_start >= aligned_end) {
        return;
    }

    start_page = (uint32_t)(aligned_start >> PMM_PAGE_SHIFT);
    end_page = (uint32_t)(aligned_end >> PMM_PAGE_SHIFT);
    if (start_page == 0U) {
        start_page = 1U;
    }

    if (start_page >= end_page) {
        return;
    }

    *first = start_page;
    *last_excl = end_page;
}

static void outward_page_span(
    uint64_t base,
    uint64_t end,
    uint32_t *first,
    uint32_t *last_excl
)
{
    const uint64_t aligned_start = base & ~(uint64_t)PMM_PAGE_MASK;
    uint64_t aligned_end = end;
    uint32_t start_page;
    uint32_t end_page;

    *first = 0U;
    *last_excl = 0U;

    if (base >= end) {
        return;
    }

    if ((aligned_end & (uint64_t)PMM_PAGE_MASK) != 0U) {
        aligned_end =
            (aligned_end + (uint64_t)PMM_PAGE_MASK) & ~(uint64_t)PMM_PAGE_MASK;
    }

    start_page = (uint32_t)(aligned_start >> PMM_PAGE_SHIFT);
    if (aligned_end >= PMM_PHYSICAL_LIMIT) {
        end_page = PMM_PAGE_COUNT;
    } else {
        end_page = (uint32_t)(aligned_end >> PMM_PAGE_SHIFT);
    }

    if (start_page >= end_page) {
        return;
    }

    *first = start_page;
    *last_excl = end_page;
}

void pmm_reset(void)
{
    uint32_t word;

    for (word = 0U; word < PMM_BITMAP_WORDS; ++word) {
        managed_pages[word] = 0U;
        allocated_pages[word] = 0U;
    }

    total_pages = 0U;
    free_pages = 0U;
    ownership_frozen = false;
    mark_page_reserved(0U);
}

bool pmm_add_usable_region(uint64_t base, uint64_t length)
{
    uint64_t clipped_base;
    uint64_t clipped_end;
    uint32_t first;
    uint32_t last_excl;
    uint32_t page;

    if (ownership_frozen) {
        return false;
    }

    if (!clip_to_physical(base, length, &clipped_base, &clipped_end)) {
        return false;
    }

    inward_page_span(clipped_base, clipped_end, &first, &last_excl);
    for (page = first; page < last_excl; ++page) {
        mark_page_usable(page);
    }

    return true;
}

bool pmm_reserve_region(uint64_t base, uint64_t length)
{
    uint64_t clipped_base;
    uint64_t clipped_end;
    uint32_t first;
    uint32_t last_excl;
    uint32_t page;

    if (ownership_frozen) {
        return false;
    }

    if (!clip_to_physical(base, length, &clipped_base, &clipped_end)) {
        return false;
    }

    outward_page_span(clipped_base, clipped_end, &first, &last_excl);
    for (page = first; page < last_excl; ++page) {
        mark_page_reserved(page);
    }

    return true;
}

uintptr_t pmm_alloc_page(void)
{
    uint32_t word;

    ownership_frozen = true;
    if (free_pages == 0U) {
        return 0U;
    }

    for (word = 0U; word < PMM_BITMAP_WORDS; ++word) {
        const uint32_t free_mask = managed_pages[word] & ~allocated_pages[word];
        uint32_t bit;
        uint32_t page;

        if (free_mask == 0U) {
            continue;
        }

        bit = (uint32_t)__builtin_ctz(free_mask);
        page = word * 32U + bit;
        set_allocated(page, true);
        free_pages -= 1U;
        return (uintptr_t)page << PMM_PAGE_SHIFT;
    }

    return 0U;
}

bool pmm_free_page(uintptr_t physical_address)
{
    uint32_t page;

    if (physical_address == 0U) {
        return false;
    }

    if ((physical_address & (uintptr_t)PMM_PAGE_MASK) != 0U) {
        return false;
    }

    if (physical_address > ((uintptr_t)(PMM_PAGE_COUNT - 1U) << PMM_PAGE_SHIFT)) {
        return false;
    }

    page = (uint32_t)(physical_address >> PMM_PAGE_SHIFT);
    if (!page_is_managed(page) || !page_is_allocated(page)) {
        return false;
    }

    set_allocated(page, false);
    free_pages += 1U;
    return true;
}

uint32_t pmm_free_page_count(void)
{
    return free_pages;
}

uint32_t pmm_total_page_count(void)
{
    return total_pages;
}
