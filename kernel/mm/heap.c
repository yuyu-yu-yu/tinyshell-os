#include "mm/heap.h"

#include <stddef.h>
#include <stdint.h>

enum {
    KHEAP_ARENA_SIZE = 256U * 1024U,
    KHEAP_ALIGNMENT = 16U,
    HEAP_BLOCK_FREE = 1U,
    HEAP_BLOCK_ALLOCATED = 2U,
};

#define HEAP_BLOCK_MAGIC UINT32_C(0x48454150)

/* may_alias permits metadata views over the byte arena under strict aliasing. */
struct __attribute__((aligned(KHEAP_ALIGNMENT), may_alias)) heap_block {
    uint32_t magic;
    uint32_t state;
    size_t payload_size;
    struct heap_block *previous;
    struct heap_block *next;
};

_Static_assert(
    (KHEAP_ARENA_SIZE % KHEAP_ALIGNMENT) == 0U,
    "heap arena must contain complete alignment units"
);
_Static_assert(
    (sizeof(struct heap_block) % KHEAP_ALIGNMENT) == 0U,
    "heap metadata must preserve payload alignment"
);
_Static_assert(
    sizeof(struct heap_block) + KHEAP_ALIGNMENT <= KHEAP_ARENA_SIZE,
    "heap arena must hold metadata and one payload"
);

static _Alignas(KHEAP_ALIGNMENT) uint8_t heap_arena[KHEAP_ARENA_SIZE];
static struct heap_block *first_block;
static bool heap_initialized;

static uintptr_t arena_begin(void)
{
    return (uintptr_t)&heap_arena[0];
}

static uintptr_t arena_end(void)
{
    return arena_begin() + (uintptr_t)KHEAP_ARENA_SIZE;
}

static void *block_payload(struct heap_block *block)
{
    return (void *)((uint8_t *)block + sizeof(struct heap_block));
}

static bool align_request(size_t size, size_t *aligned_size)
{
    const size_t mask = (size_t)KHEAP_ALIGNMENT - 1U;

    if (aligned_size == NULL || size == 0U || size > SIZE_MAX - mask) {
        return false;
    }

    *aligned_size = (size + mask) & ~mask;
    return true;
}

static bool inspect_heap(struct heap_stats *stats)
{
    const uintptr_t begin = arena_begin();
    const uintptr_t end = arena_end();
    const size_t total_capacity =
        (size_t)KHEAP_ARENA_SIZE - sizeof(struct heap_block);
    const size_t maximum_blocks =
        (size_t)KHEAP_ARENA_SIZE / sizeof(struct heap_block);
    struct heap_stats measured = {
        .total_bytes = total_capacity,
        .free_bytes = 0U,
        .largest_free_block = 0U,
        .allocated_blocks = 0U,
    };
    struct heap_block *previous = NULL;
    uintptr_t cursor = begin;
    size_t payload_bytes = 0U;
    size_t block_count = 0U;
    bool previous_was_free = false;

    if (!heap_initialized
        || (uintptr_t)first_block != begin
        || begin > UINTPTR_MAX - (uintptr_t)KHEAP_ARENA_SIZE) {
        return false;
    }

    for (;;) {
        struct heap_block *block;
        uintptr_t payload_start;
        uintptr_t block_end;
        bool is_free;

        if (block_count >= maximum_blocks
            || (cursor & ((uintptr_t)KHEAP_ALIGNMENT - 1U)) != 0U
            || cursor > end - sizeof(struct heap_block)) {
            return false;
        }

        block = (struct heap_block *)cursor;
        if (block->magic != HEAP_BLOCK_MAGIC
            || (block->state != HEAP_BLOCK_FREE
                && block->state != HEAP_BLOCK_ALLOCATED)
            || block->previous != previous
            || block->payload_size < KHEAP_ALIGNMENT
            || (block->payload_size
                & ((size_t)KHEAP_ALIGNMENT - 1U)) != 0U) {
            return false;
        }

        payload_start = cursor + sizeof(struct heap_block);
        if (block->payload_size > (size_t)(end - payload_start)) {
            return false;
        }
        block_end = payload_start + (uintptr_t)block->payload_size;

        if (payload_bytes > SIZE_MAX - block->payload_size) {
            return false;
        }
        payload_bytes += block->payload_size;
        ++block_count;

        is_free = block->state == HEAP_BLOCK_FREE;
        if (is_free) {
            if (previous_was_free
                || measured.free_bytes > SIZE_MAX - block->payload_size) {
                return false;
            }
            measured.free_bytes += block->payload_size;
            if (block->payload_size > measured.largest_free_block) {
                measured.largest_free_block = block->payload_size;
            }
        } else {
            if (measured.allocated_blocks == UINT32_MAX) {
                return false;
            }
            ++measured.allocated_blocks;
        }

        if (block->next == NULL) {
            if (block_end != end) {
                return false;
            }
            break;
        }

        if (block_end >= end || (uintptr_t)block->next != block_end) {
            return false;
        }

        previous_was_free = is_free;
        previous = block;
        cursor = block_end;
    }

    if (block_count == 0U
        || block_count - 1U > SIZE_MAX / sizeof(struct heap_block)) {
        return false;
    }

    const size_t extra_metadata =
        (block_count - 1U) * sizeof(struct heap_block);
    if (payload_bytes > total_capacity
        || extra_metadata != total_capacity - payload_bytes
        || measured.free_bytes > total_capacity
        || measured.largest_free_block > measured.free_bytes) {
        return false;
    }

    if (stats != NULL) {
        *stats = measured;
    }
    return true;
}

static void invalidate_block(struct heap_block *block)
{
    block->magic = 0U;
    block->state = 0U;
    block->payload_size = 0U;
    block->previous = NULL;
    block->next = NULL;
}

static struct heap_block *merge_with_next(struct heap_block *left)
{
    struct heap_block *right = left->next;
    struct heap_block *after = right->next;

    left->payload_size += sizeof(struct heap_block) + right->payload_size;
    left->next = after;
    if (after != NULL) {
        after->previous = left;
    }
    invalidate_block(right);
    return left;
}

bool kheap_init(void)
{
    size_t index;

    heap_initialized = false;
    first_block = NULL;
    for (index = 0U; index < (size_t)KHEAP_ARENA_SIZE; ++index) {
        heap_arena[index] = 0U;
    }

    first_block = (struct heap_block *)(void *)&heap_arena[0];
    first_block->magic = HEAP_BLOCK_MAGIC;
    first_block->state = HEAP_BLOCK_FREE;
    first_block->payload_size =
        (size_t)KHEAP_ARENA_SIZE - sizeof(struct heap_block);
    first_block->previous = NULL;
    first_block->next = NULL;
    heap_initialized = true;

    if (!inspect_heap(NULL)) {
        heap_initialized = false;
        first_block = NULL;
        return false;
    }
    return true;
}

void *kmalloc(size_t size)
{
    size_t aligned_size;
    struct heap_block *block;

    if (!align_request(size, &aligned_size) || !inspect_heap(NULL)) {
        return NULL;
    }

    for (block = first_block; block != NULL; block = block->next) {
        if (block->state == HEAP_BLOCK_FREE
            && block->payload_size >= aligned_size) {
            const size_t remaining = block->payload_size - aligned_size;

            if (remaining >= sizeof(struct heap_block) + KHEAP_ALIGNMENT) {
                struct heap_block *split =
                    (struct heap_block *)((uint8_t *)block_payload(block)
                                          + aligned_size);

                split->magic = HEAP_BLOCK_MAGIC;
                split->state = HEAP_BLOCK_FREE;
                split->payload_size = remaining - sizeof(struct heap_block);
                split->previous = block;
                split->next = block->next;
                if (split->next != NULL) {
                    split->next->previous = split;
                }

                block->payload_size = aligned_size;
                block->next = split;
            }

            block->state = HEAP_BLOCK_ALLOCATED;
            return block_payload(block);
        }
    }

    return NULL;
}

void *kcalloc(size_t count, size_t size)
{
    size_t total;
    uint8_t *memory;
    size_t index;

    if (count == 0U || size == 0U || size > SIZE_MAX / count) {
        return NULL;
    }

    total = count * size;
    memory = (uint8_t *)kmalloc(total);
    if (memory == NULL) {
        return NULL;
    }

    for (index = 0U; index < total; ++index) {
        memory[index] = 0U;
    }
    return memory;
}

bool kfree(void *pointer)
{
    const uintptr_t begin = arena_begin();
    const uintptr_t end = arena_end();
    const uintptr_t address = (uintptr_t)pointer;
    struct heap_block *block;

    if (pointer == NULL
        || address < begin + sizeof(struct heap_block)
        || address >= end
        || (address & ((uintptr_t)KHEAP_ALIGNMENT - 1U)) != 0U
        || !inspect_heap(NULL)) {
        return false;
    }

    for (block = first_block; block != NULL; block = block->next) {
        if ((uintptr_t)block_payload(block) == address) {
            break;
        }
    }

    if (block == NULL || block->state != HEAP_BLOCK_ALLOCATED) {
        return false;
    }

    block->state = HEAP_BLOCK_FREE;
    if (block->previous != NULL
        && block->previous->state == HEAP_BLOCK_FREE) {
        block = merge_with_next(block->previous);
    }
    if (block->next != NULL && block->next->state == HEAP_BLOCK_FREE) {
        (void)merge_with_next(block);
    }
    return true;
}

bool kheap_get_stats(struct heap_stats *stats)
{
    struct heap_stats measured;

    if (stats == NULL || !inspect_heap(&measured)) {
        return false;
    }

    *stats = measured;
    return true;
}

bool kheap_validate(void)
{
    return inspect_heap(NULL);
}
