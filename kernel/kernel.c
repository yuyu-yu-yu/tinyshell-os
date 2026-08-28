#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/irq.h"
#include "arch/x86/keyboard.h"
#include "arch/x86/pic.h"
#include "arch/x86/pit.h"
#include "boot/multiboot.h"
#include "console.h"
#include "diag/system_status.h"
#include "ipc/ipc.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "shell/runtime.h"
#include "task/task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    LOW_MEMORY_LIMIT = 0x00100000U,
    PIT_TEST_TICKS = 3U,
    TASK_FLOW_MESSAGES = 6U,
    TASK_FLOW_TYPE = 0x54494E59U,
    TASK_FLOW_BUDGET = 256U,
    TASK_FLOW_MIN_SWITCHES =
        2U * (TASK_FLOW_MESSAGES + 1U) + PIT_TEST_TICKS,
};

static const uintptr_t VMM_TEST_ADDRESS = UINT32_C(0xD0000000);
static const uint32_t VMM_TEST_SENTINEL = UINT32_C(0xC0DEF00D);

extern const uint8_t __kernel_start[];
extern const uint8_t __kernel_end[];

static ipc_endpoint_t task_flow_endpoint = IPC_INVALID_ENDPOINT;
static task_id_t producer_task_id = TASK_INVALID_ID;
static uint32_t task_flow_tick_start;
static uint32_t task_flow_irq_start;
static uint32_t produced_messages;
static uint32_t consumed_messages;
static bool producer_finished;
static bool consumer_finished;
static bool timer_observer_finished;
static bool task_flow_failed;

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

static bool vmm_self_test(void)
{
    const uint32_t free_before = pmm_free_page_count();
    uintptr_t physical_address;
    uintptr_t translated_address = UINT32_C(0xA5A5A5A5);
    uintptr_t unmapped_address = 0U;
    volatile uint32_t *virtual_word;
    volatile uint32_t *physical_word;

    if (!vmm_init() || !vmm_is_enabled()) {
        return false;
    }

    physical_address = pmm_alloc_page();
    if (physical_address == 0U
        || physical_address >= (uintptr_t)VMM_IDENTITY_LIMIT
        || pmm_free_page_count() != free_before - 1U
        || !vmm_map_page(
            VMM_TEST_ADDRESS,
            physical_address,
            VMM_WRITABLE)) {
        return false;
    }

    virtual_word = (volatile uint32_t *)VMM_TEST_ADDRESS;
    physical_word = (volatile uint32_t *)physical_address;
    *virtual_word = VMM_TEST_SENTINEL;
    if (*physical_word != VMM_TEST_SENTINEL
        || !vmm_translate(VMM_TEST_ADDRESS + 3U, &translated_address)
        || translated_address != physical_address + 3U
        || !vmm_unmap_page(VMM_TEST_ADDRESS, &unmapped_address)
        || unmapped_address != physical_address) {
        return false;
    }

    translated_address = UINT32_C(0xA5A5A5A5);
    if (vmm_translate(VMM_TEST_ADDRESS, &translated_address)
        || translated_address != UINT32_C(0xA5A5A5A5)
        || !pmm_free_page(physical_address)
        || pmm_free_page_count() != free_before) {
        return false;
    }

    return true;
}

static bool heap_stats_match(
    const struct heap_stats *left,
    const struct heap_stats *right
)
{
    return left->total_bytes == right->total_bytes
        && left->free_bytes == right->free_bytes
        && left->largest_free_block == right->largest_free_block
        && left->allocated_blocks == right->allocated_blocks;
}

static bool heap_self_test(void)
{
    struct heap_stats initial;
    struct heap_stats final;
    uint8_t foreign = 0U;
    uint8_t *zeroed;
    void *first;
    void *middle;
    void *last;
    size_t index;

    if (!kheap_init()
        || !kheap_validate()
        || !kheap_get_stats(&initial)
        || initial.total_bytes == 0U
        || initial.free_bytes != initial.total_bytes
        || initial.largest_free_block != initial.total_bytes
        || initial.allocated_blocks != 0U
        || kmalloc(0U) != NULL
        || kcalloc(0U, 16U) != NULL
        || kcalloc(SIZE_MAX, 2U) != NULL) {
        return false;
    }

    zeroed = (uint8_t *)kcalloc(17U, 3U);
    if (zeroed == NULL
        || ((uintptr_t)zeroed & 0x0FU) != 0U
        || kfree(NULL)
        || kfree(&foreign)
        || kfree(zeroed + 1U)) {
        return false;
    }

    for (index = 0U; index < 51U; ++index) {
        if (zeroed[index] != 0U) {
            return false;
        }
    }

    if (!kfree(zeroed) || kfree(zeroed) || !kheap_validate()) {
        return false;
    }

    first = kmalloc(64U);
    middle = kmalloc(96U);
    last = kmalloc(128U);
    if (first == NULL
        || middle == NULL
        || last == NULL
        || ((uintptr_t)first & 0x0FU) != 0U
        || ((uintptr_t)middle & 0x0FU) != 0U
        || ((uintptr_t)last & 0x0FU) != 0U
        || !kfree(middle)
        || !kheap_validate()
        || !kfree(first)
        || !kheap_validate()
        || !kfree(last)
        || !kheap_validate()
        || !kheap_get_stats(&final)
        || !heap_stats_match(&initial, &final)) {
        return false;
    }

    return true;
}

static void prepare_message(
    struct ipc_message *message,
    uint32_t sender,
    uint32_t type,
    uint32_t length,
    uint8_t seed
)
{
    uint32_t index;

    message->sender = sender;
    message->type = type;
    message->length = length;
    for (index = 0U; index < IPC_PAYLOAD_MAX; ++index) {
        message->payload[index] = (uint8_t)(seed + index);
    }
}

static bool payload_tail_is_zero(
    const struct ipc_message *message,
    uint32_t from
)
{
    uint32_t index;

    for (index = from; index < IPC_PAYLOAD_MAX; ++index) {
        if (message->payload[index] != 0U) {
            return false;
        }
    }
    return true;
}

static bool ipc_self_test(ipc_endpoint_t *flow_endpoint)
{
    ipc_endpoint_t endpoints[IPC_MAX_ENDPOINTS];
    struct ipc_message input;
    struct ipc_message output;
    uint32_t endpoint_index;
    uint32_t message_index;

    if (flow_endpoint == NULL) {
        return false;
    }

    ipc_init();
    for (endpoint_index = 0U;
         endpoint_index < IPC_MAX_ENDPOINTS;
         ++endpoint_index) {
        uint32_t previous;

        endpoints[endpoint_index] = ipc_endpoint_create();
        if (endpoints[endpoint_index] == IPC_INVALID_ENDPOINT) {
            return false;
        }
        for (previous = 0U; previous < endpoint_index; ++previous) {
            if (endpoints[previous] == endpoints[endpoint_index]) {
                return false;
            }
        }
    }

    if (ipc_endpoint_create() != IPC_INVALID_ENDPOINT
        || ipc_pending(endpoints[1]) != 0U) {
        return false;
    }

    prepare_message(&input, 7U, 9U, 3U, 0x20U);
    if (!ipc_send(endpoints[0], &input)) {
        return false;
    }
    input.sender = 99U;
    input.type = 100U;
    input.payload[0] = 0xFFU;
    if (!ipc_receive(endpoints[0], &output)
        || output.sender != 7U
        || output.type != 9U
        || output.length != 3U
        || output.payload[0] != 0x20U
        || output.payload[1] != 0x21U
        || output.payload[2] != 0x22U
        || !payload_tail_is_zero(&output, 3U)) {
        return false;
    }

    prepare_message(&output, 0x11U, 0x22U, 32U, 0x33U);
    if (ipc_receive(endpoints[0], &output)
        || output.sender != 0x11U
        || output.type != 0x22U
        || output.length != 32U
        || output.payload[0] != 0x33U
        || output.payload[31] != 0x52U) {
        return false;
    }

    prepare_message(&input, 1U, 2U, IPC_PAYLOAD_MAX + 1U, 0x40U);
    if (ipc_send(endpoints[0], &input)
        || ipc_rejected_count(endpoints[0]) != 0U) {
        return false;
    }

    prepare_message(&input, 3U, 4U, 0U, 0x70U);
    if (!ipc_send(endpoints[0], &input)
        || !ipc_receive(endpoints[0], &output)
        || output.length != 0U
        || !payload_tail_is_zero(&output, 0U)) {
        return false;
    }

    for (message_index = 0U;
         message_index < IPC_QUEUE_DEPTH;
         ++message_index) {
        prepare_message(
            &input,
            5U,
            message_index,
            1U,
            (uint8_t)message_index
        );
        if (!ipc_send(endpoints[0], &input)) {
            return false;
        }
    }

    if (ipc_pending(endpoints[0]) != IPC_QUEUE_DEPTH
        || ipc_send(endpoints[0], &input)
        || ipc_rejected_count(endpoints[0]) != 1U
        || ipc_pending(endpoints[1]) != 0U) {
        return false;
    }

    for (message_index = 0U;
         message_index < IPC_QUEUE_DEPTH;
         ++message_index) {
        if (!ipc_receive(endpoints[0], &output)
            || output.sender != 5U
            || output.type != message_index
            || output.length != 1U
            || output.payload[0] != (uint8_t)message_index
            || !payload_tail_is_zero(&output, 1U)) {
            return false;
        }
    }

    prepare_message(&input, 8U, 10U, IPC_PAYLOAD_MAX, 0x10U);
    if (!ipc_send(endpoints[1], &input)
        || !ipc_receive(endpoints[1], &output)
        || output.length != IPC_PAYLOAD_MAX) {
        return false;
    }
    for (message_index = 0U;
         message_index < IPC_PAYLOAD_MAX;
         ++message_index) {
        if (output.payload[message_index]
            != (uint8_t)(0x10U + message_index)) {
            return false;
        }
    }

    ipc_init();
    *flow_endpoint = ipc_endpoint_create();
    return *flow_endpoint != IPC_INVALID_ENDPOINT
        && ipc_pending(*flow_endpoint) == 0U
        && ipc_rejected_count(*flow_endpoint) == 0U;
}

static void producer_task(void *argument)
{
    uint32_t sequence;

    (void)argument;
    for (sequence = 0U; sequence < TASK_FLOW_MESSAGES; ++sequence) {
        struct ipc_message message;

        prepare_message(
            &message,
            task_current_id(),
            TASK_FLOW_TYPE,
            4U,
            0U
        );
        message.payload[0] = (uint8_t)sequence;
        message.payload[1] = 0xA5U;
        message.payload[2] = 0x5AU;
        message.payload[3] = (uint8_t)(sequence ^ 0xFFU);

        while (!ipc_send(task_flow_endpoint, &message)) {
            task_yield();
        }
        produced_messages += 1U;
        task_yield();
    }

    producer_finished = true;
}

static void consumer_task(void *argument)
{
    uint32_t sequence;

    (void)argument;
    for (sequence = 0U; sequence < TASK_FLOW_MESSAGES; ++sequence) {
        struct ipc_message message;

        while (!ipc_receive(task_flow_endpoint, &message)) {
            task_yield();
        }

        if (message.sender != producer_task_id
            || message.type != TASK_FLOW_TYPE
            || message.length != 4U
            || message.payload[0] != (uint8_t)sequence
            || message.payload[1] != 0xA5U
            || message.payload[2] != 0x5AU
            || message.payload[3] != (uint8_t)(sequence ^ 0xFFU)
            || !payload_tail_is_zero(&message, 4U)) {
            task_flow_failed = true;
            return;
        }

        consumed_messages += 1U;
        task_yield();
    }

    consumer_finished = true;
}

static void timer_observer_task(void *argument)
{
    (void)argument;

    task_flow_tick_start = pit_ticks();
    task_flow_irq_start = irq_count(0U);

    while ((uint32_t)(pit_ticks() - task_flow_tick_start) < PIT_TEST_TICKS
        || (uint32_t)(irq_count(0U) - task_flow_irq_start) < PIT_TEST_TICKS) {
        __asm__ volatile ("hlt");
        task_yield();
    }

    timer_observer_finished = true;
    task_exit();
}

static bool run_task_flow(void)
{
    task_id_t consumer_id;
    task_id_t observer_id;
    uint32_t switches;

    produced_messages = 0U;
    consumed_messages = 0U;
    producer_finished = false;
    consumer_finished = false;
    timer_observer_finished = false;
    task_flow_failed = false;
    task_system_init();
    producer_task_id = task_create(producer_task, NULL);
    consumer_id = task_create(consumer_task, NULL);
    observer_id = task_create(timer_observer_task, NULL);
    if (producer_task_id == TASK_INVALID_ID
        || consumer_id == TASK_INVALID_ID
        || observer_id == TASK_INVALID_ID
        || producer_task_id == consumer_id
        || producer_task_id == observer_id
        || consumer_id == observer_id
        || !task_run(TASK_FLOW_BUDGET)) {
        return false;
    }

    switches = task_switch_count();
    return !task_flow_failed
        && producer_finished
        && consumer_finished
        && timer_observer_finished
        && produced_messages == TASK_FLOW_MESSAGES
        && consumed_messages == TASK_FLOW_MESSAGES
        && ipc_pending(task_flow_endpoint) == 0U
        && ipc_rejected_count(task_flow_endpoint) == 0U
        && task_finished_count() == 3U
        && task_current_id() == TASK_INVALID_ID
        && switches >= TASK_FLOW_MIN_SWITCHES
        && switches <= TASK_FLOW_BUDGET
        && (uint32_t)(pit_ticks() - task_flow_tick_start) >= PIT_TEST_TICKS
        && (uint32_t)(irq_count(0U) - task_flow_irq_start) >= PIT_TEST_TICKS;
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
    struct system_status status;

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
    if (kernel_end <= kernel_start
        || kernel_end > (uintptr_t)VMM_IDENTITY_LIMIT) {
        boot_fail("kernel-range");
    }

    pmm_reset();
    if (!multiboot_for_each_memory_region(
            multiboot_magic,
            multiboot_info_address,
            add_memory_region,
            NULL)) {
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

    if (!vmm_self_test()) {
        boot_fail("vmm-self-test");
    }
    console_write("PAGING_OK\n");
    console_write("VMM_MAP_OK\n");

    if (!heap_self_test()) {
        boot_fail("heap-self-test");
    }
    console_write("HEAP_OK\n");
    console_write("HEAP_COALESCE_OK\n");

    if (!ipc_self_test(&task_flow_endpoint)) {
        boot_fail("ipc-self-test");
    }
    console_write("IPC_OK\n");

    if (!shell_runtime_init()) {
        boot_fail("shell-runtime");
    }
    console_write("RAMFS_OK\n");
    console_write("SHELL_INPUT_OK\n");
    console_write("SHELL_PARSE_OK\n");

    tick_start = pit_ticks();
    irq_start = irq_count(0U);
    __asm__ volatile ("sti" : : : "memory");

    while ((uint32_t)(pit_ticks() - tick_start) < PIT_TEST_TICKS
        || (uint32_t)(irq_count(0U) - irq_start) < PIT_TEST_TICKS) {
        __asm__ volatile ("hlt");
    }
    console_write("TIMER_IRQ_OK\n");

    if (!run_task_flow()) {
        boot_fail("task-flow");
    }
    console_write("TASK_OK\n");
    console_write("SCHEDULER_OK\n");
    console_write("IPC_TASK_FLOW_OK\n");

    if (!system_status_read(&status)
        || status.pmm_total_pages == 0U
        || status.pmm_free_pages == 0U
        || status.heap_total_bytes == 0U
        || status.pit_ticks < PIT_TEST_TICKS
        || status.irq0_count < PIT_TEST_TICKS
        || status.keyboard_dropped != 0U
        || status.task_switches < TASK_FLOW_MIN_SWITCHES
        || status.task_finished != 3U) {
        boot_fail("system-status");
    }
    console_write("SYSTEM_STATUS_OK\n");
    console_write("SHELL_READY\n");
    console_write("BOOT_OK\n");
    shell_runtime_start();

    for (;;) {
        char character;

        __asm__ volatile ("hlt");
        while (keyboard_pop_char(&character)) {
            shell_runtime_handle_char(character);
        }
    }
}
