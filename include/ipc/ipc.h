#ifndef TINYOS_IPC_IPC_H
#define TINYOS_IPC_IPC_H

#include <stdbool.h>
#include <stdint.h>

enum {
    IPC_MAX_ENDPOINTS = 8,
    IPC_QUEUE_DEPTH = 8,
    IPC_PAYLOAD_MAX = 32,
};

typedef uint32_t ipc_endpoint_t;

#define IPC_INVALID_ENDPOINT UINT32_MAX

struct ipc_message {
    uint32_t sender;
    uint32_t type;
    uint32_t length;
    uint8_t payload[IPC_PAYLOAD_MAX];
};

/* Reset all endpoints. Call only during single-threaded initialization. */
void ipc_init(void);

/* Endpoints are allocated once per ipc_init() epoch and are not destroyed. */
ipc_endpoint_t ipc_endpoint_create(void);

/* Non-blocking copied-message operations. These functions never yield. */
bool ipc_send(
    ipc_endpoint_t endpoint,
    const struct ipc_message *message
);
bool ipc_receive(
    ipc_endpoint_t endpoint,
    struct ipc_message *message
);

/* Invalid or unallocated endpoints report zero. */
uint32_t ipc_pending(ipc_endpoint_t endpoint);
uint32_t ipc_rejected_count(ipc_endpoint_t endpoint);

#endif
