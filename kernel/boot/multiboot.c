#include "boot/multiboot.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MULTIBOOT_INFO_MEMORY = 1U << 0,
    MULTIBOOT_INFO_MEMORY_MAP = 1U << 6,
    MULTIBOOT_MMAP_MIN_PAYLOAD_SIZE = 20U,
};

_Static_assert(
    offsetof(struct multiboot_info, mmap_length) == 44U,
    "unexpected Multiboot mmap_length offset"
);
_Static_assert(
    offsetof(struct multiboot_info, mmap_addr) == 48U,
    "unexpected Multiboot mmap_addr offset"
);
_Static_assert(
    sizeof(struct multiboot_info) == 52U,
    "unexpected Multiboot information size"
);
_Static_assert(
    sizeof(struct multiboot_mmap_entry) == 24U,
    "unexpected Multiboot memory-map entry size"
);
_Static_assert(
    offsetof(struct multiboot_mmap_entry, base_addr) == 4U,
    "unexpected Multiboot memory-map base offset"
);
_Static_assert(
    offsetof(struct multiboot_mmap_entry, length) == 12U,
    "unexpected Multiboot memory-map length offset"
);
_Static_assert(
    offsetof(struct multiboot_mmap_entry, type) == 20U,
    "unexpected Multiboot memory-map type offset"
);

static bool count_memory_map_entries(
    uint32_t mmap_address,
    uint32_t mmap_length,
    uint32_t *entry_count
)
{
    if (mmap_address == 0U || mmap_length == 0U || entry_count == NULL) {
        return false;
    }

    if (mmap_address > UINT32_MAX - mmap_length) {
        return false;
    }

    const uint32_t mmap_end = mmap_address + mmap_length;
    uint32_t cursor = mmap_address;
    uint32_t count = 0U;

    while (cursor < mmap_end) {
        const uint32_t remaining = mmap_end - cursor;
        if (remaining < sizeof(uint32_t)) {
            return false;
        }

        const struct multiboot_mmap_entry *const entry =
            (const struct multiboot_mmap_entry *)(uintptr_t)cursor;
        const uint32_t payload_size = entry->size;

        if (payload_size < MULTIBOOT_MMAP_MIN_PAYLOAD_SIZE
            || payload_size > UINT32_MAX - sizeof(uint32_t)) {
            return false;
        }

        const uint32_t record_size = payload_size + sizeof(uint32_t);
        if (record_size > remaining || entry->length == 0U) {
            return false;
        }

        if (entry->base_addr > UINT64_MAX - entry->length
            || count == UINT32_MAX) {
            return false;
        }

        ++count;
        cursor += record_size;
    }

    if (count == 0U) {
        return false;
    }

    *entry_count = count;
    return true;
}

bool multiboot_parse(
    uint32_t magic,
    uint32_t info_address,
    struct boot_memory_summary *summary
)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC
        || info_address == 0U
        || info_address > UINT32_MAX - sizeof(struct multiboot_info)
        || summary == NULL) {
        return false;
    }

    const struct multiboot_info *const info =
        (const struct multiboot_info *)(uintptr_t)info_address;
    const uint32_t required_flags =
        MULTIBOOT_INFO_MEMORY | MULTIBOOT_INFO_MEMORY_MAP;

    if ((info->flags & required_flags) != required_flags) {
        return false;
    }

    struct boot_memory_summary parsed = {
        .lower_kib = info->mem_lower,
        .upper_kib = info->mem_upper,
        .mmap_entry_count = 0U,
    };

    if (!count_memory_map_entries(
            info->mmap_addr,
            info->mmap_length,
            &parsed.mmap_entry_count)) {
        return false;
    }

    *summary = parsed;
    return true;
}
