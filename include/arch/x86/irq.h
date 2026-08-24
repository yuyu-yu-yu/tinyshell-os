#ifndef TINYOS_X86_IRQ_H
#define TINYOS_X86_IRQ_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*irq_handler_t)(void);

/*
 * Registration, unregistration, and mask changes are initialization-only
 * operations and must be called with IF cleared.
 */
void irq_init(void);
bool irq_register_handler(uint8_t irq, irq_handler_t handler);
bool irq_unregister_handler(uint8_t irq);
bool irq_set_enabled(uint8_t irq, bool enabled);
uint32_t irq_count(uint8_t irq);

void irq_dispatch(uint8_t irq);

#endif
