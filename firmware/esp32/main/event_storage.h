#ifndef EVENT_STORAGE_H
#define EVENT_STORAGE_H

#include <stdbool.h>
#include "esp_err.h"
#include "node_event.h"

esp_err_t event_storage_initialize(void);

esp_err_t event_storage_save(const NodeEvent *event);

esp_err_t event_storage_load_latest(NodeEvent *event, bool *event_found);

esp_err_t event_storage_save_protection_state(bool tripped, BreakerState breaker_state);

esp_err_t event_storage_load_protection_state(bool *tripped, BreakerState *breaker_state, bool *state_found);

esp_err_t event_storage_get_queue_counts(uint32_t *total_count, uint32_t *pending_count);

esp_err_t event_storage_load_next_pending(NodeEvent * event, bool *event_found);

esp_err_t event_storage_acknowledge(uint32_t sequence);

#endif
