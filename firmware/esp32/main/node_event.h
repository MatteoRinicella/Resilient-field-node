#ifndef NODE_EVENT_H
#define NODE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "resilient_field_node.h"
#include "rtc_ds3231.h"

typedef enum
{
    NODE_EVENT_TYPE_PROTECTION_TRIP = 0
} NodeEventType;

typedef enum
{
    NODE_EVENT_DELIVERY_PENDING = 0,
    NODE_EVENT_DELIVERY_ACKNOWLEDGED = 1
} NodeEventDeliveryState;

typedef struct
{
    uint32_t sequence;
    NodeEventType type;

    bool has_timestamp;
    RtcDateTime occurred_at;

    double measured_current_ma;
    double threshold_ma;
    BreakerState breaker_state;
} NodeEvent;

typedef struct
{
    NodeEvent event;
    NodeEventDeliveryState delivery_state;
} QueuedNodeEvent;


#endif