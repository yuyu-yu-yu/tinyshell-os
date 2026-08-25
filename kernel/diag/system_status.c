#include "diag/system_status.h"

#include "arch/x86/irq.h"
#include "arch/x86/keyboard.h"
#include "arch/x86/pit.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "task/task.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Invariants:
 * - Only public getters are used; no other module's static state is read.
 * - On failure the caller's struct is left unchanged.
 * - size_t fields are range-checked before narrowing to uint32_t.
 * Failure paths:
 * - status == NULL
 * - kheap_get_stats() fails
 * - any heap size_t value does not fit in uint32_t
 */

static bool size_to_u32(size_t value, uint32_t *out)
{
#if SIZE_MAX > UINT32_MAX
    if (value > (size_t)UINT32_MAX) {
        return false;
    }
#endif
    *out = (uint32_t)value;
    return true;
}

bool system_status_read(struct system_status *status)
{
    struct system_status snapshot;
    struct heap_stats heap;

    if (status == NULL) {
        return false;
    }

    if (!kheap_get_stats(&heap)) {
        return false;
    }

    if (!size_to_u32(heap.total_bytes, &snapshot.heap_total_bytes)
        || !size_to_u32(heap.free_bytes, &snapshot.heap_free_bytes)
        || !size_to_u32(heap.largest_free_block, &snapshot.heap_largest_free_block)) {
        return false;
    }

    snapshot.heap_allocated_blocks = heap.allocated_blocks;
    snapshot.pmm_total_pages = pmm_total_page_count();
    snapshot.pmm_free_pages = pmm_free_page_count();
    snapshot.pit_ticks = pit_ticks();
    snapshot.irq0_count = irq_count(0U);
    snapshot.keyboard_dropped = keyboard_dropped_count();
    snapshot.task_switches = task_switch_count();
    snapshot.task_finished = task_finished_count();

    *status = snapshot;
    return true;
}
