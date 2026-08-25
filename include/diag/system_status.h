#ifndef TINYOS_DIAG_SYSTEM_STATUS_H
#define TINYOS_DIAG_SYSTEM_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Read-only snapshot of public kernel counters for the Shell `status`
 * command. This module does not print, allocate, mask interrupts, or
 * reset any of the counters it observes.
 *
 * pit_ticks and irq0_count are sampled with two separate loads, so they
 * need not come from the same CPU cycle while IRQ0 is live.
 */
struct system_status {
    uint32_t pmm_total_pages;
    uint32_t pmm_free_pages;
    uint32_t heap_total_bytes;
    uint32_t heap_free_bytes;
    uint32_t heap_largest_free_block;
    uint32_t heap_allocated_blocks;
    uint32_t pit_ticks;
    uint32_t irq0_count;
    uint32_t keyboard_dropped;
    uint32_t task_switches;
    uint32_t task_finished;
};

bool system_status_read(struct system_status *status);

#endif
