#include "arch/x86/keyboard.h"

#include "io.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    KEYBOARD_STATUS_PORT = 0x64,
    KEYBOARD_DATA_PORT = 0x60,
    KEYBOARD_STATUS_OUTPUT_FULL = 0x01,
    KEYBOARD_EXTENDED_PREFIX = 0xE0,
    KEYBOARD_RELEASE_BIT = 0x80,
    KEYBOARD_LEFT_SHIFT = 0x2A,
    KEYBOARD_RIGHT_SHIFT = 0x36,
    KEYBOARD_CAPS_LOCK = 0x3A,
    KEYBOARD_ENTER = 0x1C,
    KEYBOARD_BACKSPACE = 0x0E,
    KEYBOARD_SPACE = 0x39,
    KEYBOARD_QUEUE_CAPACITY = 64,
    KEYBOARD_QUEUE_MASK = KEYBOARD_QUEUE_CAPACITY - 1,
};

_Static_assert(
    (KEYBOARD_QUEUE_CAPACITY & KEYBOARD_QUEUE_MASK) == 0,
    "keyboard queue capacity must be a power of two"
);

static char character_queue[KEYBOARD_QUEUE_CAPACITY];

/*
 * IRQ1 is the sole producer and the kernel main loop is the sole consumer.
 * Monotonic unsigned sequence numbers retain all 64 queue slots and make
 * wraparound well-defined. Each side writes only its own sequence number.
 */
static volatile uint32_t write_sequence;
static volatile uint32_t read_sequence;
static volatile uint32_t dropped_characters;

static bool left_shift_down;
static bool right_shift_down;
static bool caps_lock_enabled;
static bool extended_prefix_pending;

static void compiler_barrier(void)
{
    __asm__ volatile ("" : : : "memory");
}

static char letter_for_scancode(uint8_t code)
{
    switch (code) {
    case 0x1EU: return 'a';
    case 0x30U: return 'b';
    case 0x2EU: return 'c';
    case 0x20U: return 'd';
    case 0x12U: return 'e';
    case 0x21U: return 'f';
    case 0x22U: return 'g';
    case 0x23U: return 'h';
    case 0x17U: return 'i';
    case 0x24U: return 'j';
    case 0x25U: return 'k';
    case 0x26U: return 'l';
    case 0x32U: return 'm';
    case 0x31U: return 'n';
    case 0x18U: return 'o';
    case 0x19U: return 'p';
    case 0x10U: return 'q';
    case 0x13U: return 'r';
    case 0x1FU: return 's';
    case 0x14U: return 't';
    case 0x16U: return 'u';
    case 0x2FU: return 'v';
    case 0x11U: return 'w';
    case 0x2DU: return 'x';
    case 0x15U: return 'y';
    case 0x2CU: return 'z';
    default: return '\0';
    }
}

static char digit_for_scancode(uint8_t code, bool shifted)
{
    static const char unshifted_digits[] = "1234567890";
    static const char shifted_digits[] = "!@#$%^&*()";
    uint8_t index;

    if (code < 0x02U || code > 0x0BU) {
        return '\0';
    }

    index = (uint8_t)(code - 0x02U);
    return shifted ? shifted_digits[index] : unshifted_digits[index];
}

static bool queue_push(char character)
{
    uint32_t write = write_sequence;
    uint32_t read = read_sequence;

    if ((uint32_t)(write - read) >= KEYBOARD_QUEUE_CAPACITY) {
        dropped_characters += 1U;
        return false;
    }

    character_queue[write & KEYBOARD_QUEUE_MASK] = character;
    compiler_barrier();
    write_sequence = write + 1U;
    return true;
}

void keyboard_init(void)
{
    write_sequence = 0U;
    read_sequence = 0U;
    dropped_characters = 0U;
    left_shift_down = false;
    right_shift_down = false;
    caps_lock_enabled = false;
    extended_prefix_pending = false;
}

void keyboard_handle_irq(void)
{
    uint8_t status = inb(KEYBOARD_STATUS_PORT);

    if ((status & KEYBOARD_STATUS_OUTPUT_FULL) == 0U) {
        return;
    }

    (void)keyboard_feed_scancode(inb(KEYBOARD_DATA_PORT));
}

bool keyboard_feed_scancode(uint8_t scancode)
{
    uint8_t code;
    bool released;
    bool shifted;
    char character;

    if (scancode == KEYBOARD_EXTENDED_PREFIX) {
        extended_prefix_pending = true;
        return false;
    }

    if (extended_prefix_pending) {
        extended_prefix_pending = false;
        return false;
    }

    released = (scancode & KEYBOARD_RELEASE_BIT) != 0U;
    code = (uint8_t)(scancode & (uint8_t)~KEYBOARD_RELEASE_BIT);

    if (code == KEYBOARD_LEFT_SHIFT) {
        left_shift_down = !released;
        return false;
    }

    if (code == KEYBOARD_RIGHT_SHIFT) {
        right_shift_down = !released;
        return false;
    }

    if (released) {
        return false;
    }

    if (code == KEYBOARD_CAPS_LOCK) {
        caps_lock_enabled = !caps_lock_enabled;
        return false;
    }

    shifted = left_shift_down || right_shift_down;
    character = letter_for_scancode(code);
    if (character != '\0') {
        if (shifted != caps_lock_enabled) {
            character = (char)(character - ('a' - 'A'));
        }
        return queue_push(character);
    }

    character = digit_for_scancode(code, shifted);
    if (character != '\0') {
        return queue_push(character);
    }

    switch (code) {
    case KEYBOARD_ENTER:
        return queue_push('\n');
    case KEYBOARD_BACKSPACE:
        return queue_push('\b');
    case KEYBOARD_SPACE:
        return queue_push(' ');
    default:
        return false;
    }
}

bool keyboard_pop_char(char *character)
{
    uint32_t read;
    uint32_t write;

    if (character == 0) {
        return false;
    }

    read = read_sequence;
    write = write_sequence;
    if (read == write) {
        return false;
    }

    compiler_barrier();
    *character = character_queue[read & KEYBOARD_QUEUE_MASK];
    compiler_barrier();
    read_sequence = read + 1U;
    return true;
}

uint32_t keyboard_dropped_count(void)
{
    return dropped_characters;
}
