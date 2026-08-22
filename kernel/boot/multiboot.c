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

struct multiboot_view {
    const struct multiboot_info *info;
};

static bool validate_multiboot_info(
    uint32_t magic,
    uint32_t info_address,
    struct multiboot_view *view
)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC
        || info_address == 0U
        || info_address > UINT32_MAX - sizeof(struct multiboot_info)
        || view == NULL) {
        return false;
    }

    const struct multiboot_info *const info =
        (const struct multiboot_info *)(uintptr_t)info_address;
    const uint32_t required_flags =
        MULTIBOOT_INFO_MEMORY | MULTIBOOT_INFO_MEMORY_MAP;

    if ((info->flags & required_flags) != required_flags
        || info->mmap_addr == 0U
        || info->mmap_length == 0U
        || info->mmap_addr > UINT32_MAX - info->mmap_length) {
        return false;
    }

    const struct multiboot_view validated = {
        .info = info,
    };
    *view = validated;
    return true;
}

static bool walk_memory_map(
    const struct multiboot_view *view,
    boot_memory_region_visitor visitor,
    void *context,
    uint32_t *entry_count
)
{
    if (view == NULL || view->info == NULL) {
        return false;
    }

    const uint32_t mmap_address = view->info->mmap_addr;
    const uint32_t mmap_end = mmap_address + view->info->mmap_length;
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

        const struct boot_memory_region region = {
            .base = entry->base_addr,
            .length = entry->length,
            .type = entry->type,
        };

        ++count;

        if (visitor != NULL && !visitor(&region, context)) {
            return false;
        }

        cursor += record_size;
    }

    if (count == 0U) {
        return false;
    }

    if (entry_count != NULL) {
        *entry_count = count;
    }
    return true;
}

bool multiboot_parse(
    uint32_t magic,
    uint32_t info_address,
    struct boot_memory_summary *summary
)
{
    if (summary == NULL) {
        return false;
    }

    struct multiboot_view view;
    if (!validate_multiboot_info(magic, info_address, &view)) {
        return false;
    }

    struct boot_memory_summary parsed = {
        .lower_kib = view.info->mem_lower,
        .upper_kib = view.info->mem_upper,
        .mmap_entry_count = 0U,
    };

    if (!walk_memory_map(&view, NULL, NULL, &parsed.mmap_entry_count)) {
        return false;
    }

    *summary = parsed;
    return true;
}

bool multiboot_for_each_memory_region(
    uint32_t magic,
    uint32_t info_address,
    boot_memory_region_visitor visitor,
    void *context
)
{
    if (visitor == NULL) {
        return false;
    }

    struct multiboot_view view;
    if (!validate_multiboot_info(magic, info_address, &view)) {
        return false;
    }

    /* Validate the complete buffer before allowing visitor side effects. */
    if (!walk_memory_map(&view, NULL, NULL, NULL)) {
        return false;
    }

    return walk_memory_map(&view, visitor, context, NULL);
}

bool multiboot_get_owned_ranges(
    uint32_t magic,
    uint32_t info_address,
    struct boot_owned_ranges *ranges
)
{
    if (ranges == NULL) {
        return false;
    }

    struct multiboot_view view;
    if (!validate_multiboot_info(magic, info_address, &view)
        || !walk_memory_map(&view, NULL, NULL, NULL)) {
        return false;
    }

    const struct boot_owned_ranges owned = {
        .info_address = info_address,
        .info_length = (uint32_t)sizeof(struct multiboot_info),
        .mmap_address = view.info->mmap_addr,
        .mmap_length = view.info->mmap_length,
    };
    *ranges = owned;
    return true;
}
