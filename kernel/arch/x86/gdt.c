#include "arch/x86/gdt.h"

#include <stddef.h>
#include <stdint.h>

enum {
    GDT_ENTRY_COUNT = 5,
    GDT_FLAT_LIMIT = 0x000FFFFF,
    GDT_FLAG_32_BIT_GRANULAR_4K = 0xC0,
    GDT_ACCESS_KERNEL_CODE = 0x9A,
    GDT_ACCESS_KERNEL_DATA = 0x92,
    GDT_ACCESS_USER_CODE = 0xFA,
    GDT_ACCESS_USER_DATA = 0xF2,
};

static struct gdt_entry gdt[GDT_ENTRY_COUNT] __attribute__((aligned(8)));
static struct gdt_descriptor gdtr;

extern void gdt_load(const struct gdt_descriptor *descriptor);

_Static_assert(sizeof(struct gdt_entry) == 8, "GDT entries must be 8 bytes");
_Static_assert(sizeof(struct gdt_descriptor) == 6, "GDTR must be 6 bytes");

static void gdt_set_entry(
    size_t index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t flags
)
{
    gdt[index].limit_low = (uint16_t)(limit & 0xFFFFU);
    gdt[index].base_low = (uint16_t)(base & 0xFFFFU);
    gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFFU);
    gdt[index].access = access;
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0FU)
        | (flags & 0xF0U));
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xFFU);
}

void gdt_init(void)
{
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(
        1,
        0,
        GDT_FLAT_LIMIT,
        GDT_ACCESS_KERNEL_CODE,
        GDT_FLAG_32_BIT_GRANULAR_4K
    );
    gdt_set_entry(
        2,
        0,
        GDT_FLAT_LIMIT,
        GDT_ACCESS_KERNEL_DATA,
        GDT_FLAG_32_BIT_GRANULAR_4K
    );
    gdt_set_entry(
        3,
        0,
        GDT_FLAT_LIMIT,
        GDT_ACCESS_USER_CODE,
        GDT_FLAG_32_BIT_GRANULAR_4K
    );
    gdt_set_entry(
        4,
        0,
        GDT_FLAT_LIMIT,
        GDT_ACCESS_USER_DATA,
        GDT_FLAG_32_BIT_GRANULAR_4K
    );

    gdtr.limit = (uint16_t)(sizeof(gdt) - 1U);
    gdtr.base = (uint32_t)(uintptr_t)gdt;
    gdt_load(&gdtr);
}
