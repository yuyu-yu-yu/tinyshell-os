#ifndef TINYOS_FS_RAMFS_H
#define TINYOS_FS_RAMFS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    RAMFS_MAX_FILES = 16,
    RAMFS_NAME_MAX = 31,
    RAMFS_FILE_MAX_BYTES = 512,
};

enum ramfs_result {
    RAMFS_RESULT_OK,
    RAMFS_RESULT_INVALID_ARGUMENT,
    RAMFS_RESULT_INVALID_NAME,
    RAMFS_RESULT_NOT_FOUND,
    RAMFS_RESULT_ALREADY_EXISTS,
    RAMFS_RESULT_NO_SPACE,
    RAMFS_RESULT_TOO_LARGE,
};

struct ramfs_file_info {
    char name[RAMFS_NAME_MAX + 1];
    uint32_t size;
};

void ramfs_init(void);
enum ramfs_result ramfs_create(const char *name);
enum ramfs_result ramfs_write(
    const char *name,
    const uint8_t *data,
    uint32_t length
);
enum ramfs_result ramfs_append(
    const char *name,
    const uint8_t *data,
    uint32_t length
);
enum ramfs_result ramfs_read(
    const char *name,
    uint8_t *data,
    uint32_t capacity,
    uint32_t *length
);
enum ramfs_result ramfs_remove(const char *name);
uint32_t ramfs_count(void);
bool ramfs_get_info(uint32_t index, struct ramfs_file_info *info);
bool ramfs_validate(void);

#endif
