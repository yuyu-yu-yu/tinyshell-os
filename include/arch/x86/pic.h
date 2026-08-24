#ifndef TINYOS_X86_PIC_H
#define TINYOS_X86_PIC_H

#include <stdbool.h>
#include <stdint.h>

void pic_init(void);
bool pic_set_mask(uint8_t irq, bool masked);
uint16_t pic_get_mask(void);
void pic_send_eoi(uint8_t irq);

#endif
