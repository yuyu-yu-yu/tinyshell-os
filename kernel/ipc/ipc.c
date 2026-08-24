#include "ipc/ipc.h"

#include <stdbool.h>
#include <stdint.h>

struct ipc_endpoint_state {
    bool allocated;
    uint32_t read_sequence;
    uint32_t write_sequence;
    uint32_t rejected_messages;
    struct ipc_message queue[IPC_QUEUE_DEPTH];
};

static struct ipc_endpoint_state endpoints[IPC_MAX_ENDPOINTS];

static void message_clear(struct ipc_message *message)
{
    uint32_t index;

    message->sender = 0U;
    message->type = 0U;
    message->length = 0U;
    for (index = 0U; index < IPC_PAYLOAD_MAX; ++index) {
        message->payload[index] = 0U;
    }
}

static bool endpoint_is_valid(ipc_endpoint_t endpoint)
{
    return endpoint < IPC_MAX_ENDPOINTS && endpoints[endpoint].allocated;
}

void ipc_init(void)
{
    uint32_t endpoint;

    for (endpoint = 0U; endpoint < IPC_MAX_ENDPOINTS; ++endpoint) {
        struct ipc_endpoint_state *state = &endpoints[endpoint];
        uint32_t slot;

        state->allocated = false;
        state->read_sequence = 0U;
        state->write_sequence = 0U;
        state->rejected_messages = 0U;
        for (slot = 0U; slot < IPC_QUEUE_DEPTH; ++slot) {
            message_clear(&state->queue[slot]);
        }
    }
}

ipc_endpoint_t ipc_endpoint_create(void)
{
    uint32_t endpoint;

    for (endpoint = 0U; endpoint < IPC_MAX_ENDPOINTS; ++endpoint) {
        struct ipc_endpoint_state *state = &endpoints[endpoint];

        if (state->allocated) {
            continue;
        }

        state->allocated = true;
        state->read_sequence = 0U;
        state->write_sequence = 0U;
        state->rejected_messages = 0U;
        return endpoint;
    }

    return IPC_INVALID_ENDPOINT;
}

bool ipc_send(
    ipc_endpoint_t endpoint,
    const struct ipc_message *message
)
{
    struct ipc_endpoint_state *state;
    struct ipc_message *destination;
    uint32_t index;

    if (!endpoint_is_valid(endpoint) || message == 0 ||
        message->length > IPC_PAYLOAD_MAX) {
        return false;
    }

    state = &endpoints[endpoint];
    if ((uint32_t)(state->write_sequence - state->read_sequence) >=
        IPC_QUEUE_DEPTH) {
        state->rejected_messages += 1U;
        return false;
    }

    destination = &state->queue[state->write_sequence % IPC_QUEUE_DEPTH];
    destination->sender = message->sender;
    destination->type = message->type;
    destination->length = message->length;
    for (index = 0U; index < IPC_PAYLOAD_MAX; ++index) {
        destination->payload[index] =
            index < message->length ? message->payload[index] : 0U;
    }

    state->write_sequence += 1U;
    return true;
}

bool ipc_receive(
    ipc_endpoint_t endpoint,
    struct ipc_message *message
)
{
    struct ipc_endpoint_state *state;
    const struct ipc_message *source;
    uint32_t index;

    if (!endpoint_is_valid(endpoint) || message == 0) {
        return false;
    }

    state = &endpoints[endpoint];
    if (state->read_sequence == state->write_sequence) {
        return false;
    }

    source = &state->queue[state->read_sequence % IPC_QUEUE_DEPTH];
    message->sender = source->sender;
    message->type = source->type;
    message->length = source->length;
    for (index = 0U; index < IPC_PAYLOAD_MAX; ++index) {
        message->payload[index] = source->payload[index];
    }

    state->read_sequence += 1U;
    return true;
}

uint32_t ipc_pending(ipc_endpoint_t endpoint)
{
    if (!endpoint_is_valid(endpoint)) {
        return 0U;
    }

    return (uint32_t)(
        endpoints[endpoint].write_sequence - endpoints[endpoint].read_sequence
    );
}

uint32_t ipc_rejected_count(ipc_endpoint_t endpoint)
{
    if (!endpoint_is_valid(endpoint)) {
        return 0U;
    }

    return endpoints[endpoint].rejected_messages;
}
