#ifndef GATEWAY_CLIENT_H
#define GATEWAY_CLIENT_H

#include <stdbool.h>
#include "esp_err.h"
#include "node_event.h"

typedef struct
{
    RtcDateTime reported_at;
    uint32_t last_event_sequence;
    bool tripped;
    BreakerState breaker_state;
} GatewayNodeState;

esp_err_t gateway_client_send_event(
    const NodeEvent *event,
    bool *acknowledged
);

esp_err_t gateway_client_send_state(
    const GatewayNodeState *state,
    bool *acknowledged
);

#endif