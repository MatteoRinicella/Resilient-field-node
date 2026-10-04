#ifndef GATEWAY_CLIENT_H
#define GATEWAY_CLIENT_H

#include <stdbool.h>
#include "esp_err.h"
#include "node_event.h"

esp_err_t gateway_client_send_event(
    const NodeEvent *event,
    bool *acknowledged
);

#endif