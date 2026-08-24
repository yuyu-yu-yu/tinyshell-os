#include "task/task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum task_state {
    TASK_STATE_UNUSED,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_FINISHED,
};

enum {
    EFLAGS_RESERVED = 1U << 1,
    EFLAGS_TRAP = 1U << 8,
    EFLAGS_INTERRUPT = 1U << 9,
    EFLAGS_DIRECTION = 1U << 10,
    EFLAGS_NESTED_TASK = 1U << 14,
    EFLAGS_VIRTUAL_8086 = 1U << 17,
};

struct task_control_block {
    enum task_state state;
    task_entry_t entry;
    void *argument;
    uintptr_t stack_pointer;
};

static struct task_control_block tasks[TASK_MAX_TASKS];
static uint8_t task_stacks[TASK_MAX_TASKS][TASK_STACK_SIZE]
    __attribute__((aligned(16)));

static uintptr_t bootstrap_stack_pointer;
static task_id_t current_task_id = TASK_INVALID_ID;
static task_id_t round_robin_cursor = TASK_INVALID_ID;
static uint32_t remaining_dispatches;
static uint32_t switch_count;
static uint32_t finished_count;
static bool scheduler_initialized;
static bool scheduler_running;

extern void task_context_switch(
    uintptr_t *old_stack_pointer,
    uintptr_t new_stack_pointer,
    uint32_t old_eflags
);

_Static_assert(
    (TASK_STACK_SIZE % 16U) == 0U,
    "task stacks must preserve 16-byte alignment"
);

static uint32_t interrupt_state_save_and_disable(void)
{
    uint32_t flags;

#if defined(TINYOS_TASK_TEST_USER_MODE)
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0"
        : "=r"(flags)
        :
        : "memory"
    );
#else
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
#endif

    return flags;
}

static void interrupt_state_restore(uint32_t flags)
{
#if defined(TINYOS_TASK_TEST_USER_MODE)
    (void)flags;
#else
    if ((flags & EFLAGS_INTERRUPT) != 0U) {
        __asm__ volatile ("sti" : : : "memory");
    } else {
        __asm__ volatile ("cli" : : : "memory");
    }
#endif
}

static _Noreturn void task_stop(void)
{
#if defined(TINYOS_TASK_TEST_USER_MODE)
    for (;;) {
        __asm__ volatile ("" : : : "memory");
    }
#else
    __asm__ volatile ("cli" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
#endif
}

static task_id_t find_next_ready(task_id_t after)
{
    uint32_t start = 0U;

    if (after != TASK_INVALID_ID) {
        start = (after + 1U) % TASK_MAX_TASKS;
    }

    for (uint32_t offset = 0U; offset < TASK_MAX_TASKS; ++offset) {
        const task_id_t candidate =
            (task_id_t)((start + offset) % TASK_MAX_TASKS);

        if (tasks[candidate].state == TASK_STATE_READY) {
            return candidate;
        }
    }

    return TASK_INVALID_ID;
}

static bool activate_task(task_id_t id)
{
    if (id >= TASK_MAX_TASKS
        || tasks[id].state != TASK_STATE_READY
        || remaining_dispatches == 0U) {
        return false;
    }

    tasks[id].state = TASK_STATE_RUNNING;
    current_task_id = id;
    round_robin_cursor = id;
    remaining_dispatches -= 1U;
    switch_count += 1U;
    return true;
}

static uint32_t initial_task_eflags(uint32_t creator_flags)
{
    const uint32_t unsafe = EFLAGS_TRAP
        | EFLAGS_DIRECTION
        | EFLAGS_NESTED_TASK
        | EFLAGS_VIRTUAL_8086;

    return (creator_flags | EFLAGS_RESERVED) & ~unsafe;
}

static _Noreturn void task_trampoline(void)
{
    const task_id_t id = current_task_id;

    if (id >= TASK_MAX_TASKS
        || tasks[id].state != TASK_STATE_RUNNING
        || tasks[id].entry == NULL) {
        task_stop();
    }

    tasks[id].entry(tasks[id].argument);
    task_exit();
}

static void prepare_initial_stack(task_id_t id, uint32_t creator_flags)
{
    uintptr_t top = (uintptr_t)&task_stacks[id][TASK_STACK_SIZE];
    uint32_t *stack;

    top &= ~(uintptr_t)0x0FU;
    stack = (uint32_t *)top;

    /*
     * context_switch pops EDI, ESI, EBX, EBP, EFLAGS, then returns to the
     * trampoline. The final task_exit word is its fake caller return address.
     * This leaves ESP == 12 (mod 16) at the trampoline's C entry, matching a
     * normal i386 SysV call made with a 16-byte-aligned call site.
     */
    *--stack = (uint32_t)(uintptr_t)task_exit;
    *--stack = (uint32_t)(uintptr_t)task_trampoline;
    *--stack = initial_task_eflags(creator_flags);
    *--stack = 0U; /* EBP */
    *--stack = 0U; /* EBX */
    *--stack = 0U; /* ESI */
    *--stack = 0U; /* EDI */

    tasks[id].stack_pointer = (uintptr_t)stack;
}

static void switch_to_bootstrap(task_id_t previous, uint32_t previous_flags)
{
    current_task_id = TASK_INVALID_ID;
    scheduler_running = false;
    task_context_switch(
        &tasks[previous].stack_pointer,
        bootstrap_stack_pointer,
        previous_flags
    );
}

void task_system_init(void)
{
    const uint32_t flags = interrupt_state_save_and_disable();

    if (scheduler_running) {
        interrupt_state_restore(flags);
        return;
    }

    for (uint32_t id = 0U; id < TASK_MAX_TASKS; ++id) {
        tasks[id].state = TASK_STATE_UNUSED;
        tasks[id].entry = NULL;
        tasks[id].argument = NULL;
        tasks[id].stack_pointer = 0U;
    }

    bootstrap_stack_pointer = 0U;
    current_task_id = TASK_INVALID_ID;
    round_robin_cursor = TASK_INVALID_ID;
    remaining_dispatches = 0U;
    switch_count = 0U;
    finished_count = 0U;
    scheduler_running = false;
    scheduler_initialized = true;

    interrupt_state_restore(flags);
}

task_id_t task_create(task_entry_t entry, void *argument)
{
    const uint32_t flags = interrupt_state_save_and_disable();
    task_id_t id = TASK_INVALID_ID;

    if (!scheduler_initialized || scheduler_running || entry == NULL) {
        interrupt_state_restore(flags);
        return TASK_INVALID_ID;
    }

    for (uint32_t candidate = 0U;
         candidate < TASK_MAX_TASKS;
         ++candidate) {
        if (tasks[candidate].state == TASK_STATE_UNUSED) {
            id = (task_id_t)candidate;
            break;
        }
    }

    if (id == TASK_INVALID_ID) {
        interrupt_state_restore(flags);
        return TASK_INVALID_ID;
    }

    tasks[id].entry = entry;
    tasks[id].argument = argument;
    prepare_initial_stack(id, flags);
    tasks[id].state = TASK_STATE_READY;

    interrupt_state_restore(flags);
    return id;
}

bool task_run(uint32_t switch_budget)
{
    uint32_t flags = interrupt_state_save_and_disable();
    task_id_t next;
    bool complete;

    if (!scheduler_initialized || scheduler_running) {
        interrupt_state_restore(flags);
        return false;
    }

    next = find_next_ready(round_robin_cursor);
    if (next == TASK_INVALID_ID) {
        interrupt_state_restore(flags);
        return true;
    }

    if (switch_budget == 0U) {
        interrupt_state_restore(flags);
        return false;
    }

    remaining_dispatches = switch_budget;
    scheduler_running = true;
    if (!activate_task(next)) {
        scheduler_running = false;
        interrupt_state_restore(flags);
        return false;
    }

    task_context_switch(
        &bootstrap_stack_pointer,
        tasks[next].stack_pointer,
        flags
    );

    /* The bootstrap EFLAGS were restored by task_context_switch. */
    flags = interrupt_state_save_and_disable();
    complete = find_next_ready(round_robin_cursor) == TASK_INVALID_ID;
    interrupt_state_restore(flags);
    return complete;
}

void task_yield(void)
{
    const uint32_t flags = interrupt_state_save_and_disable();
    task_id_t previous;
    task_id_t next;

    if (!scheduler_initialized
        || !scheduler_running
        || current_task_id >= TASK_MAX_TASKS
        || tasks[current_task_id].state != TASK_STATE_RUNNING) {
        interrupt_state_restore(flags);
        return;
    }

    previous = current_task_id;
    tasks[previous].state = TASK_STATE_READY;
    next = find_next_ready(previous);

    if (next != TASK_INVALID_ID && remaining_dispatches != 0U) {
        if (!activate_task(next)) {
            task_stop();
        }

        if (next == previous) {
            interrupt_state_restore(flags);
            return;
        }

        task_context_switch(
            &tasks[previous].stack_pointer,
            tasks[next].stack_pointer,
            flags
        );
        return;
    }

    switch_to_bootstrap(previous, flags);
}

_Noreturn void task_exit(void)
{
    const uint32_t flags = interrupt_state_save_and_disable();
    task_id_t previous;
    task_id_t next;

    if (!scheduler_initialized
        || !scheduler_running
        || current_task_id >= TASK_MAX_TASKS
        || tasks[current_task_id].state != TASK_STATE_RUNNING) {
        task_stop();
    }

    previous = current_task_id;
    tasks[previous].state = TASK_STATE_FINISHED;
    finished_count += 1U;
    next = find_next_ready(previous);

    if (next != TASK_INVALID_ID && remaining_dispatches != 0U) {
        if (!activate_task(next)) {
            task_stop();
        }

        task_context_switch(
            &tasks[previous].stack_pointer,
            tasks[next].stack_pointer,
            flags
        );
        task_stop();
    }

    switch_to_bootstrap(previous, flags);
    task_stop();
}

task_id_t task_current_id(void)
{
    if (!scheduler_running || current_task_id >= TASK_MAX_TASKS) {
        return TASK_INVALID_ID;
    }

    return current_task_id;
}

uint32_t task_switch_count(void)
{
    return switch_count;
}

uint32_t task_finished_count(void)
{
    return finished_count;
}
