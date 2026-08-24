#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/irq.h"
#include "arch/x86/keyboard.h"
#include "arch/x86/pic.h"
#include "arch/x86/pit.h"
#include "boot/multiboot.h"
#include "console.h"
#include "mm/pmm.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    LOW_MEMORY_LIMIT = 0x00100000U,
    PIT_TEST_TICKS = 3U,
};

extern const uint8_t __kernel_start[];
extern const uint8_t __kernel_end[];

static _Noreturn void boot_fail(const char *stage)
{
    console_write("BOOT_FAIL:");
    console_write(stage);
    console_putc('\n');

    __asm__ volatile ("cli" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static bool add_memory_region(
    const struct boot_memory_region *region,
    void *context
)
{
    (void)context;

    if (region->type == 1U) {
        return pmm_add_usable_region(region->base, region->length);
    }

    return pmm_reserve_region(region->base, region->length);
}

static bool ranges_overlap(
    uint64_t first_base,
    uint64_t first_length,
    uint64_t second_base,
    uint64_t second_length
)
{
    return first_base < second_base + second_length
        && second_base < first_base + first_length;
}

static bool allocation_is_reserved(
    uintptr_t allocation,
    const struct boot_owned_ranges *owned,
    uintptr_t kernel_start,
    uintptr_t kernel_end
)
{
    const uint64_t page = (uint64_t)allocation;

    return ranges_overlap(page, PMM_PAGE_SIZE, 0U, LOW_MEMORY_LIMIT)
        || ranges_overlap(
            page,
            PMM_PAGE_SIZE,
            (uint64_t)kernel_start,
            (uint64_t)(kernel_end - kernel_start)
        )
        || ranges_overlap(
            page,
            PMM_PAGE_SIZE,
            owned->info_address,
            owned->info_length
        )
        || ranges_overlap(
            page,
            PMM_PAGE_SIZE,
            owned->mmap_address,
            owned->mmap_length
        );
}

static bool keyboard_self_test(void)
{
    char character = '\0';

    keyboard_init();
    if (keyboard_feed_scancode(0x2AU)
        || !keyboard_feed_scancode(0x1EU)
        || keyboard_feed_scancode(0x9EU)
        || keyboard_feed_scancode(0xAAU)
        || !keyboard_pop_char(&character)
        || character != 'A'
        || keyboard_pop_char(&character)
        || keyboard_dropped_count() != 0U) {
        return false;
    }

    return true;
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_address)
{
    struct boot_memory_summary memory_summary;
    struct boot_owned_ranges owned_ranges;
    uintptr_t kernel_start;
    uintptr_t kernel_end;
    uintptr_t allocated_page;
    uint32_t free_before;
    uint32_t tick_start;
    uint32_t irq_start;

    __asm__ volatile ("cli" : : : "memory");

    console_init();
    console_write("TinyShell OS booting...\n");
    console_write("Architecture: i386\n");
    console_write("CONSOLE_OK\n");

    gdt_init();
    console_write("GDT_OK\n");

    idt_init();
    console_write("IDT_OK\n");

    if (!multiboot_parse(
            multiboot_magic,
            multiboot_info_address,
            &memory_summary)
        || !multiboot_get_owned_ranges(
            multiboot_magic,
            multiboot_info_address,
            &owned_ranges)) {
        boot_fail("multiboot");
    }

    console_write("MULTIBOOT_OK\n");
    console_write("MEMORY_MAP_OK\n");

    __asm__ volatile ("int3");
    console_write("INT3_TEST_OK\n");

    kernel_start = (uintptr_t)__kernel_start;
    kernel_end = (uintptr_t)__kernel_end;
    if (kernel_end <= kernel_start) {
        boot_fail("kernel-range");
    }

    pmm_reset();
    if (!multiboot_for_each_memory_region(
            multiboot_magic,
            multiboot_info_address,
            add_memory_region,
            0)) {
        boot_fail("pmm-map");
    }

    if (!pmm_reserve_region(0U, LOW_MEMORY_LIMIT)
        || !pmm_reserve_region(
            (uint64_t)kernel_start,
            (uint64_t)(kernel_end - kernel_start))
        || !pmm_reserve_region(
            owned_ranges.info_address,
            owned_ranges.info_length)
        || !pmm_reserve_region(
            owned_ranges.mmap_address,
            owned_ranges.mmap_length)
        || pmm_total_page_count() == 0U
        || pmm_free_page_count() == 0U) {
        boot_fail("pmm-reserve");
    }

    free_before = pmm_free_page_count();
    allocated_page = pmm_alloc_page();
    if (allocated_page == 0U
        || (allocated_page & (PMM_PAGE_SIZE - 1U)) != 0U
        || pmm_free_page_count() != free_before - 1U
        || allocation_is_reserved(
            allocated_page,
            &owned_ranges,
            kernel_start,
            kernel_end)
        || pmm_add_usable_region(0U, PMM_PAGE_SIZE)
        || pmm_reserve_region(allocated_page, PMM_PAGE_SIZE)
        || !pmm_free_page(allocated_page)
        || pmm_free_page_count() != free_before
        || pmm_free_page(allocated_page)) {
        boot_fail("pmm-self-test");
    }

    console_write("PMM_OK\n");
    console_write("PMM_ALLOC_FREE_OK\n");
    console_write("PMM_FREE_PAGES=");
    console_write_u32_dec(pmm_free_page_count());
    console_putc('\n');

    irq_init();
    if (pic_get_mask() != 0xFFFFU) {
        boot_fail("pic-mask-initial");
    }
    console_write("PIC_OK\n");

    if (!pit_configure(100U) || pit_frequency_hz() != 100U) {
        boot_fail("pit-configure");
    }
    console_write("PIT_OK\n");

    if (!keyboard_self_test()) {
        boot_fail("keyboard-decode");
    }
    console_write("KEYBOARD_DECODE_OK\n");
    keyboard_init();

    if (!irq_register_handler(0U, pit_handle_irq)
        || !irq_register_handler(1U, keyboard_handle_irq)
        || !irq_set_enabled(0U, true)
        || !irq_set_enabled(1U, true)
        || pic_get_mask() != 0xFFFCU) {
        boot_fail("irq-setup");
    }
    console_write("IRQ_OK\n");
    console_write("KEYBOARD_READY\n");

    tick_start = pit_ticks();
    irq_start = irq_count(0U);
    __asm__ volatile ("sti" : : : "memory");

    while ((uint32_t)(pit_ticks() - tick_start) < PIT_TEST_TICKS
        || (uint32_t)(irq_count(0U) - irq_start) < PIT_TEST_TICKS) {
        __asm__ volatile ("hlt");
    }

    console_write("TIMER_IRQ_OK\n");
    console_write("BOOT_OK\n");

    for (;;) {
        char character;

        __asm__ volatile ("hlt");
        while (keyboard_pop_char(&character)) {
            console_putc(character);
        }
    }
}
