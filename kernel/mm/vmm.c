#include "mm/vmm.h"

#include "arch/x86/paging.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    PAGE_DIRECTORY_ENTRIES = 1024,
    PAGE_TABLE_ENTRIES = 1024,
    PAGE_TABLE_COUNT = 64,
    IDENTITY_TABLE_COUNT = 32,
    PAGE_PRESENT = 1U << 0,
    PAGE_WRITABLE = 1U << 1,
    PAGE_USER = 1U << 2,
    PAGE_ADDRESS_MASK = 0xFFFFF000U,
    PAGE_OFFSET_MASK = VMM_PAGE_SIZE - 1U,
    VMM_ALLOWED_FLAGS = VMM_WRITABLE | VMM_USER,
    TABLE_INDEX_NONE = UINT16_MAX,
};

static uint32_t page_directory[PAGE_DIRECTORY_ENTRIES]
    __attribute__((aligned(VMM_PAGE_SIZE)));
static uint32_t page_tables[PAGE_TABLE_COUNT][PAGE_TABLE_ENTRIES]
    __attribute__((aligned(VMM_PAGE_SIZE)));
static uint16_t directory_table_indices[PAGE_DIRECTORY_ENTRIES];
static bool table_in_use[PAGE_TABLE_COUNT];
static bool enabled;

_Static_assert(
    IDENTITY_TABLE_COUNT * PAGE_TABLE_ENTRIES * VMM_PAGE_SIZE
        == VMM_IDENTITY_LIMIT,
    "identity page tables must cover exactly 128 MiB"
);
_Static_assert(
    PAGE_TABLE_COUNT > IDENTITY_TABLE_COUNT,
    "the page-table pool must retain dynamic tables"
);

static void clear_table(uint32_t table_index)
{
    uint32_t entry;

    for (entry = 0U; entry < PAGE_TABLE_ENTRIES; ++entry) {
        page_tables[table_index][entry] = 0U;
    }
}

static bool dynamic_address_is_valid(uintptr_t virtual_address)
{
    return virtual_address >= (uintptr_t)VMM_DYNAMIC_START
        && virtual_address < (uintptr_t)VMM_DYNAMIC_END
        && (virtual_address & PAGE_OFFSET_MASK) == 0U;
}

static uint32_t make_page_flags(uint32_t flags)
{
    uint32_t hardware_flags = PAGE_PRESENT;

    if ((flags & VMM_WRITABLE) != 0U) {
        hardware_flags |= PAGE_WRITABLE;
    }
    if ((flags & VMM_USER) != 0U) {
        hardware_flags |= PAGE_USER;
    }
    return hardware_flags;
}

static uint16_t allocate_dynamic_table(void)
{
    uint16_t index;

    for (index = IDENTITY_TABLE_COUNT; index < PAGE_TABLE_COUNT; ++index) {
        if (!table_in_use[index]) {
            table_in_use[index] = true;
            clear_table(index);
            return index;
        }
    }

    return TABLE_INDEX_NONE;
}

static bool table_is_empty(uint16_t table_index)
{
    uint32_t entry;

    for (entry = 0U; entry < PAGE_TABLE_ENTRIES; ++entry) {
        if ((page_tables[table_index][entry] & PAGE_PRESENT) != 0U) {
            return false;
        }
    }

    return true;
}

bool vmm_init(void)
{
    uint32_t directory_index;
    uint32_t table_index;
    uint32_t entry_index;

    if (enabled) {
        return true;
    }

    for (directory_index = 0U;
         directory_index < PAGE_DIRECTORY_ENTRIES;
         ++directory_index) {
        page_directory[directory_index] = 0U;
        directory_table_indices[directory_index] = TABLE_INDEX_NONE;
    }

    for (table_index = 0U; table_index < PAGE_TABLE_COUNT; ++table_index) {
        table_in_use[table_index] = table_index < IDENTITY_TABLE_COUNT;
        clear_table(table_index);
    }

    for (table_index = 0U;
         table_index < IDENTITY_TABLE_COUNT;
         ++table_index) {
        const uintptr_t table_address = (uintptr_t)&page_tables[table_index][0];

        directory_table_indices[table_index] = (uint16_t)table_index;
        page_directory[table_index] =
            ((uint32_t)table_address & PAGE_ADDRESS_MASK)
            | PAGE_PRESENT
            | PAGE_WRITABLE;

        for (entry_index = 0U;
             entry_index < PAGE_TABLE_ENTRIES;
             ++entry_index) {
            const uint32_t physical_address =
                (table_index * PAGE_TABLE_ENTRIES + entry_index)
                * VMM_PAGE_SIZE;

            if (physical_address != 0U) {
                page_tables[table_index][entry_index] =
                    physical_address | PAGE_PRESENT | PAGE_WRITABLE;
            }
        }
    }

    paging_load_directory((uintptr_t)&page_directory[0]);
    paging_enable();
    enabled = true;
    return true;
}

bool vmm_map_page(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    uint32_t flags
)
{
    const uint32_t directory_index = (uint32_t)(virtual_address >> 22U);
    const uint32_t entry_index =
        (uint32_t)((virtual_address >> 12U) & 0x3FFU);
    uint16_t table_index;
    uint32_t hardware_flags;

    if (!enabled
        || !dynamic_address_is_valid(virtual_address)
        || (physical_address & PAGE_OFFSET_MASK) != 0U
        || (flags & ~VMM_ALLOWED_FLAGS) != 0U) {
        return false;
    }

    table_index = directory_table_indices[directory_index];
    if (table_index == TABLE_INDEX_NONE) {
        table_index = allocate_dynamic_table();
        if (table_index == TABLE_INDEX_NONE) {
            return false;
        }

        directory_table_indices[directory_index] = table_index;
        page_directory[directory_index] =
            ((uint32_t)(uintptr_t)&page_tables[table_index][0]
                & PAGE_ADDRESS_MASK)
            | PAGE_PRESENT
            | PAGE_WRITABLE;
    }

    if ((page_tables[table_index][entry_index] & PAGE_PRESENT) != 0U) {
        return false;
    }

    hardware_flags = make_page_flags(flags);
    if ((hardware_flags & PAGE_USER) != 0U) {
        page_directory[directory_index] |= PAGE_USER;
    }
    page_tables[table_index][entry_index] =
        ((uint32_t)physical_address & PAGE_ADDRESS_MASK) | hardware_flags;
    paging_invalidate(virtual_address);
    return true;
}

bool vmm_unmap_page(
    uintptr_t virtual_address,
    uintptr_t *physical_address
)
{
    const uint32_t directory_index = (uint32_t)(virtual_address >> 22U);
    const uint32_t entry_index =
        (uint32_t)((virtual_address >> 12U) & 0x3FFU);
    uint16_t table_index;
    uint32_t entry;
    uintptr_t unmapped_address;

    if (!enabled
        || physical_address == 0
        || !dynamic_address_is_valid(virtual_address)) {
        return false;
    }

    table_index = directory_table_indices[directory_index];
    if (table_index == TABLE_INDEX_NONE) {
        return false;
    }

    entry = page_tables[table_index][entry_index];
    if ((entry & PAGE_PRESENT) == 0U) {
        return false;
    }

    unmapped_address = (uintptr_t)(entry & PAGE_ADDRESS_MASK);
    page_tables[table_index][entry_index] = 0U;
    paging_invalidate(virtual_address);

    if (table_index >= IDENTITY_TABLE_COUNT && table_is_empty(table_index)) {
        page_directory[directory_index] = 0U;
        directory_table_indices[directory_index] = TABLE_INDEX_NONE;
        table_in_use[table_index] = false;
    }

    *physical_address = unmapped_address;
    return true;
}

bool vmm_translate(
    uintptr_t virtual_address,
    uintptr_t *physical_address
)
{
    const uint32_t directory_index = (uint32_t)(virtual_address >> 22U);
    const uint32_t entry_index =
        (uint32_t)((virtual_address >> 12U) & 0x3FFU);
    const uintptr_t offset = virtual_address & PAGE_OFFSET_MASK;
    uint16_t table_index;
    uint32_t entry;
    uintptr_t translated;

    if (!enabled || physical_address == 0) {
        return false;
    }

    if ((page_directory[directory_index] & PAGE_PRESENT) == 0U) {
        return false;
    }

    table_index = directory_table_indices[directory_index];
    if (table_index == TABLE_INDEX_NONE) {
        return false;
    }

    entry = page_tables[table_index][entry_index];
    if ((entry & PAGE_PRESENT) == 0U) {
        return false;
    }

    translated = (uintptr_t)(entry & PAGE_ADDRESS_MASK) + offset;
    *physical_address = translated;
    return true;
}

bool vmm_is_enabled(void)
{
    return enabled;
}
