#include "arch/x86/pit.h"

#include "io.h"

#include <stdint.h>

enum {
    PIT_INPUT_HZ = 1193182,
    PIT_CHANNEL_0_PORT = 0x40,
    PIT_COMMAND_PORT = 0x43,
    PIT_CHANNEL_0_LO_HI_MODE_3 = 0x36,
    PIT_MAX_DIVISOR = 65535,
};

static volatile uint32_t tick_count;
static uint32_t configured_frequency_hz;

bool pit_configure(uint32_t requested_hz)
{
    uint32_t divisor;
    uint32_t actual_hz;

    if (requested_hz == 0U) {
        return false;
    }

    divisor = PIT_INPUT_HZ / requested_hz;
    if (divisor == 0U || divisor > PIT_MAX_DIVISOR) {
        return false;
    }

    actual_hz = PIT_INPUT_HZ / divisor;

    outb(PIT_COMMAND_PORT, PIT_CHANNEL_0_LO_HI_MODE_3);
    outb(PIT_CHANNEL_0_PORT, (uint8_t)(divisor & 0xFFU));
    outb(PIT_CHANNEL_0_PORT, (uint8_t)((divisor >> 8U) & 0xFFU));

    configured_frequency_hz = actual_hz;
    tick_count = 0U;
    return true;
}

void pit_handle_irq(void)
{
    tick_count += 1U;
}

uint32_t pit_ticks(void)
{
    return tick_count;
}

uint32_t pit_frequency_hz(void)
{
    return configured_frequency_hz;
}
