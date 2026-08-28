#include "fs/ramfs.h"

#include <stdbool.h>
#include <stdint.h>

struct ramfs_slot {
    bool alive;
    char name[RAMFS_NAME_MAX + 1];
    uint8_t data[RAMFS_FILE_MAX_BYTES];
    uint32_t length;
};

static struct ramfs_slot slots[RAMFS_MAX_FILES];
static uint32_t live_file_count;

static void clear_slot(struct ramfs_slot *slot)
{
    uint32_t index;

    slot->alive = false;
    slot->length = 0U;
    for (index = 0U; index <= RAMFS_NAME_MAX; ++index) {
        slot->name[index] = '\0';
    }
    for (index = 0U; index < RAMFS_FILE_MAX_BYTES; ++index) {
        slot->data[index] = 0U;
    }
}

static bool name_is_valid(const char *name)
{
    uint32_t length;

    if (name == 0) {
        return false;
    }

    for (length = 0U; length <= RAMFS_NAME_MAX; ++length) {
        char character = name[length];
        bool allowed;

        if (character == '\0') {
            if (length == 0U) {
                return false;
            }
            if (length == 1U && name[0] == '.') {
                return false;
            }
            if (length == 2U && name[0] == '.' && name[1] == '.') {
                return false;
            }
            return true;
        }

        allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '.' || character == '_' || character == '-';
        if (!allowed) {
            return false;
        }
    }

    return false;
}

static bool names_equal(const char *left, const char *right)
{
    uint32_t index;

    for (index = 0U; index <= RAMFS_NAME_MAX; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
    }

    return false;
}

static struct ramfs_slot *find_slot(const char *name)
{
    uint32_t index;

    for (index = 0U; index < RAMFS_MAX_FILES; ++index) {
        if (slots[index].alive && names_equal(slots[index].name, name)) {
            return &slots[index];
        }
    }

    return 0;
}

static struct ramfs_slot *find_empty_slot(void)
{
    uint32_t index;

    for (index = 0U; index < RAMFS_MAX_FILES; ++index) {
        if (!slots[index].alive) {
            return &slots[index];
        }
    }

    return 0;
}

void ramfs_init(void)
{
    uint32_t index;

    for (index = 0U; index < RAMFS_MAX_FILES; ++index) {
        clear_slot(&slots[index]);
    }
    live_file_count = 0U;
}

enum ramfs_result ramfs_create(const char *name)
{
    struct ramfs_slot *slot;
    uint32_t index;

    if (name == 0) {
        return RAMFS_RESULT_INVALID_ARGUMENT;
    }
    if (!name_is_valid(name)) {
        return RAMFS_RESULT_INVALID_NAME;
    }
    if (find_slot(name) != 0) {
        return RAMFS_RESULT_ALREADY_EXISTS;
    }

    slot = find_empty_slot();
    if (slot == 0) {
        return RAMFS_RESULT_NO_SPACE;
    }

    clear_slot(slot);
    for (index = 0U; index <= RAMFS_NAME_MAX; ++index) {
        slot->name[index] = name[index];
        if (name[index] == '\0') {
            break;
        }
    }
    slot->alive = true;
    live_file_count += 1U;
    return RAMFS_RESULT_OK;
}

enum ramfs_result ramfs_write(
    const char *name,
    const uint8_t *data,
    uint32_t length
)
{
    struct ramfs_slot *slot;
    uint32_t index;

    if (name == 0 || (data == 0 && length != 0U)) {
        return RAMFS_RESULT_INVALID_ARGUMENT;
    }
    if (!name_is_valid(name)) {
        return RAMFS_RESULT_INVALID_NAME;
    }

    slot = find_slot(name);
    if (slot == 0) {
        return RAMFS_RESULT_NOT_FOUND;
    }
    if (length > RAMFS_FILE_MAX_BYTES) {
        return RAMFS_RESULT_TOO_LARGE;
    }

    for (index = 0U; index < RAMFS_FILE_MAX_BYTES; ++index) {
        slot->data[index] = index < length ? data[index] : 0U;
    }
    slot->length = length;
    return RAMFS_RESULT_OK;
}

enum ramfs_result ramfs_append(
    const char *name,
    const uint8_t *data,
    uint32_t length
)
{
    struct ramfs_slot *slot;
    uint32_t index;

    if (name == 0 || (data == 0 && length != 0U)) {
        return RAMFS_RESULT_INVALID_ARGUMENT;
    }
    if (!name_is_valid(name)) {
        return RAMFS_RESULT_INVALID_NAME;
    }

    slot = find_slot(name);
    if (slot == 0) {
        return RAMFS_RESULT_NOT_FOUND;
    }
    if (length > RAMFS_FILE_MAX_BYTES - slot->length) {
        return RAMFS_RESULT_TOO_LARGE;
    }

    for (index = 0U; index < length; ++index) {
        slot->data[slot->length + index] = data[index];
    }
    slot->length += length;
    return RAMFS_RESULT_OK;
}

enum ramfs_result ramfs_read(
    const char *name,
    uint8_t *data,
    uint32_t capacity,
    uint32_t *length
)
{
    const struct ramfs_slot *slot;
    uint32_t index;

    if (name == 0 || length == 0) {
        return RAMFS_RESULT_INVALID_ARGUMENT;
    }
    if (!name_is_valid(name)) {
        return RAMFS_RESULT_INVALID_NAME;
    }

    slot = find_slot(name);
    if (slot == 0) {
        return RAMFS_RESULT_NOT_FOUND;
    }
    if (data == 0 && (slot->length != 0U || capacity != 0U)) {
        return RAMFS_RESULT_INVALID_ARGUMENT;
    }
    if (capacity < slot->length) {
        return RAMFS_RESULT_TOO_LARGE;
    }

    for (index = 0U; index < slot->length; ++index) {
        data[index] = slot->data[index];
    }
    *length = slot->length;
    return RAMFS_RESULT_OK;
}

enum ramfs_result ramfs_remove(const char *name)
{
    struct ramfs_slot *slot;

    if (name == 0) {
        return RAMFS_RESULT_INVALID_ARGUMENT;
    }
    if (!name_is_valid(name)) {
        return RAMFS_RESULT_INVALID_NAME;
    }

    slot = find_slot(name);
    if (slot == 0) {
        return RAMFS_RESULT_NOT_FOUND;
    }

    clear_slot(slot);
    live_file_count -= 1U;
    return RAMFS_RESULT_OK;
}

uint32_t ramfs_count(void)
{
    return live_file_count;
}

bool ramfs_get_info(uint32_t index, struct ramfs_file_info *info)
{
    uint32_t slot_index;
    uint32_t live_index = 0U;

    if (info == 0) {
        return false;
    }

    for (slot_index = 0U; slot_index < RAMFS_MAX_FILES; ++slot_index) {
        uint32_t name_index;

        if (!slots[slot_index].alive) {
            continue;
        }
        if (live_index != index) {
            live_index += 1U;
            continue;
        }

        for (name_index = 0U; name_index <= RAMFS_NAME_MAX; ++name_index) {
            info->name[name_index] = slots[slot_index].name[name_index];
        }
        info->size = slots[slot_index].length;
        return true;
    }

    return false;
}

bool ramfs_validate(void)
{
    uint32_t alive_count = 0U;
    uint32_t index;

    if (live_file_count > RAMFS_MAX_FILES) {
        return false;
    }

    for (index = 0U; index < RAMFS_MAX_FILES; ++index) {
        const struct ramfs_slot *slot = &slots[index];
        uint32_t other_index;

        if (!slot->alive) {
            uint32_t field_index;

            if (slot->length != 0U) {
                return false;
            }
            for (field_index = 0U; field_index <= RAMFS_NAME_MAX; ++field_index) {
                if (slot->name[field_index] != '\0') {
                    return false;
                }
            }
            for (field_index = 0U; field_index < RAMFS_FILE_MAX_BYTES;
                 ++field_index) {
                if (slot->data[field_index] != 0U) {
                    return false;
                }
            }
            continue;
        }

        alive_count += 1U;
        if (!name_is_valid(slot->name) ||
            slot->length > RAMFS_FILE_MAX_BYTES) {
            return false;
        }
        for (other_index = index + 1U; other_index < RAMFS_MAX_FILES;
             ++other_index) {
            if (slots[other_index].alive &&
                names_equal(slot->name, slots[other_index].name)) {
                return false;
            }
        }
    }

    return alive_count == live_file_count;
}
