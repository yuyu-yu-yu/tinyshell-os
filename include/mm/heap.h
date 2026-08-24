#ifndef TINYOS_MM_HEAP_H
#define TINYOS_MM_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct heap_stats {
    size_t total_bytes;
    size_t free_bytes;
    size_t largest_free_block;
    uint32_t allocated_blocks;
};

/*
 * Bootstrap kernel heap backed by a fixed 256 KiB arena.
 *
 * The allocator is neither interrupt-safe nor concurrency-safe. Callers must
 * serialize access and must not allocate from IRQ handlers. kheap_init()
 * resets the complete arena, so it is only valid during boot or isolated
 * tests when no live allocation exists.
 */
bool kheap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
bool kfree(void *pointer);
bool kheap_get_stats(struct heap_stats *stats);
bool kheap_validate(void);

#endif
