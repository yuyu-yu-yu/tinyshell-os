#ifndef TINYOS_X86_KEYBOARD_H
#define TINYOS_X86_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Reset software state while interrupts are disabled. */
void keyboard_init(void);

/* IRQ1 handler: reads at most one controller byte and performs no output. */
void keyboard_handle_irq(void);

/*
 * Feed one Set 1 scan-code byte. This returns true only when a translated
 * character was enqueued. Call it from IRQ1, or with IRQ1 disabled during
 * synthetic tests, so the queue and decoder retain a single producer.
 */
bool keyboard_feed_scancode(uint8_t scancode);

/* Return false for a null output pointer or an empty queue. */
bool keyboard_pop_char(char *character);

uint32_t keyboard_dropped_count(void);

#endif
