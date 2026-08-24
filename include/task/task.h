#ifndef TINYOS_TASK_TASK_H
#define TINYOS_TASK_TASK_H

#include <stdbool.h>
#include <stdint.h>

enum {
    TASK_MAX_TASKS = 8,
    TASK_STACK_SIZE = 16384,
};

typedef uint32_t task_id_t;
typedef void (*task_entry_t)(void *argument);

#define TASK_INVALID_ID UINT32_MAX

/* Reset the cooperative scheduler. Call only from bootstrap context. */
void task_system_init(void);

/* Tasks use fixed private stacks; finished slots are reused only after reset. */
task_id_t task_create(task_entry_t entry, void *argument);

/*
 * Dispatch at most switch_budget task slices. Return true only when no READY
 * task remains. A task slice ends at yield, exit, or a normal entry return.
 */
bool task_run(uint32_t switch_budget);

/* Yield is a safe no-op outside a running task. */
void task_yield(void);

/* Exit the current task. Calling this outside task context halts the CPU. */
_Noreturn void task_exit(void);

task_id_t task_current_id(void);

/* Number of task dispatches since task_system_init(). */
uint32_t task_switch_count(void);

uint32_t task_finished_count(void);

#endif
