#ifndef TINYOS_X86_PIT_H
#define TINYOS_X86_PIT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Configure PIT channel 0 while interrupts are disabled. On failure, the
 * existing frequency and tick count are left unchanged.
 */
bool pit_configure(uint32_t requested_hz);

/* IRQ0 handler: increments the unsigned tick counter and performs no I/O. */
void pit_handle_irq(void);

uint32_t pit_ticks(void);
uint32_t pit_frequency_hz(void);

#endif
