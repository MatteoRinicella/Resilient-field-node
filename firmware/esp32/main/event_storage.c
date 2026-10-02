#include "event_storage.h"

#include <stddef.h>
#include "nvs.h"
#include "nvs_flash.h"

#define EVENT_STORAGE_NAMESPACE "rfn_events"
#define EVENT_STORAGE_LATEST_KEY "latest"

#define EVENT_STORAGE_TRIPPED_KEY "tripped"
#define EVENT_STORAGE_BREAKER_KEY "breaker"

#define EVENT_STORAGE_QUEUE_KEY "queue"
#define EVENT_STORAGE_QUEUE_VERSION 1U
#define EVENT_STORAGE_QUEUE_CAPACITY 16U

typedef struct
{
    uint32_t version;
    uint32_t count;

    QueuedNodeEvent entries[ EVENT_STORAGE_QUEUE_CAPACITY];
} StoredEventQueue;


esp_err_t event_storage_initialize(void)
{
    esp_err_t result = nvs_flash_init();

    if(result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        result = nvs_flash_erase();

        if(result != ESP_OK)
        {
            return result;
        }

        result = nvs_flash_init();
    }

    return result;
}

esp_err_t event_storage_save(const NodeEvent *event)
{
    if(event == NULL || event->type != NODE_EVENT_TYPE_PROTECTION_TRIP || event->breaker_state != BREAKER_STATE_OPEN)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t storage_handle;

    esp_err_t result = nvs_open(
        EVENT_STORAGE_NAMESPACE,
        NVS_READWRITE,
        &storage_handle
    );

    if(result != ESP_OK)
    {
        return result;
    }

    StoredEventQueue queue = {.version = EVENT_STORAGE_QUEUE_VERSION, .count = 0};

    size_t queue_size = sizeof(queue);

    result = nvs_get_blob(
        storage_handle,
        EVENT_STORAGE_QUEUE_KEY,
        &queue,
        &queue_size
    );

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {   
        result = ESP_OK;
    }
    else if(result != ESP_OK)
    {
        nvs_close(storage_handle);
        return result;
    }
    else if(queue_size != sizeof(queue) || queue.version != EVENT_STORAGE_QUEUE_VERSION || queue.count > EVENT_STORAGE_QUEUE_CAPACITY)
    {
        nvs_close(storage_handle);
        return ESP_ERR_INVALID_STATE;
    }

    if(queue.count >= EVENT_STORAGE_QUEUE_CAPACITY)
    {
        nvs_close(storage_handle);
        return ESP_ERR_NO_MEM;
    }

    queue.entries[queue.count].event = *event;
    queue.entries[queue.count].delivery_state = NODE_EVENT_DELIVERY_PENDING;

    queue.count++;

    result = nvs_set_blob(storage_handle, EVENT_STORAGE_QUEUE_KEY, &queue, sizeof(queue));

    if(result == ESP_OK)
    {
        result = nvs_set_blob(storage_handle, EVENT_STORAGE_LATEST_KEY, event, sizeof(*event));
    }


    if(result == ESP_OK)
    {
        result = nvs_set_u8(
            storage_handle,
            EVENT_STORAGE_TRIPPED_KEY,
            1U
        );
    }

    if(result == ESP_OK)
    {
        result = nvs_set_u8(storage_handle, EVENT_STORAGE_BREAKER_KEY, (uint8_t)event->breaker_state);
    }

    if(result == ESP_OK)
    {
        result = nvs_commit(storage_handle);
    }

    nvs_close(storage_handle);

    return result;
}

esp_err_t event_storage_load_latest( NodeEvent *event, bool *event_found)
{
    if(event == NULL || event_found == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *event_found = false;

    nvs_handle_t storage_handle;

    esp_err_t result = nvs_open(
        EVENT_STORAGE_NAMESPACE,
        NVS_READONLY,
        &storage_handle
    );

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        return result;
    }

    size_t event_size = sizeof(*event);

    result = nvs_get_blob(
        storage_handle,
        EVENT_STORAGE_LATEST_KEY,
        event,
        &event_size
    );

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(storage_handle);
        return ESP_OK;
    }

    if(result == ESP_OK)
    {
        if(event_size != sizeof(*event))
        {
            result = ESP_ERR_INVALID_SIZE;
        }
        else
        {
            *event_found = true;
        }
    }
    
    nvs_close(storage_handle);

    return result;
}

esp_err_t event_storage_save_protection_state(bool tripped , BreakerState breaker_state)
{
    if(breaker_state != BREAKER_STATE_OPEN && breaker_state != BREAKER_STATE_CLOSED)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t storage_handle;
    
    esp_err_t result = nvs_open(
        EVENT_STORAGE_NAMESPACE,
        NVS_READWRITE,
        &storage_handle
    );

    if(result != ESP_OK)
    {
        return result;
    }

    result = nvs_set_u8(
        storage_handle,
        EVENT_STORAGE_TRIPPED_KEY,
        tripped ? 1U : 0U
    );

    if(result == ESP_OK)
    {
        result = nvs_set_u8(
            storage_handle,
            EVENT_STORAGE_BREAKER_KEY,
            (uint8_t)breaker_state
        );
    }

    if(result == ESP_OK)
    {
        result = nvs_commit(storage_handle);
    }

    nvs_close(storage_handle);

    return result;
}

esp_err_t event_storage_load_protection_state(bool *tripped, BreakerState *breaker_state, bool *state_found)
{
    if(tripped == NULL || breaker_state == NULL || state_found == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *tripped = false;
    *breaker_state = BREAKER_STATE_OPEN;
    *state_found = false;

    nvs_handle_t storage_handle;

    esp_err_t result = nvs_open(
        EVENT_STORAGE_NAMESPACE,
        NVS_READONLY,
        &storage_handle
    );

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        return result;
    }

    uint8_t stored_tripped = 0;
    uint8_t stored_breaker_state = 0;
    
    result = nvs_get_u8(
        storage_handle,
        EVENT_STORAGE_TRIPPED_KEY,
        &stored_tripped
    );

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(storage_handle);
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        nvs_close(storage_handle);
        return result;
    }

    result = nvs_get_u8(
        storage_handle,
        EVENT_STORAGE_BREAKER_KEY,
        &stored_breaker_state
    );

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(storage_handle);
        return ESP_ERR_INVALID_STATE;
    }

    if(result != ESP_OK)
    {
        nvs_close(storage_handle);
        return result;
    }

    if(stored_tripped > 1U ||(stored_breaker_state != (uint8_t)BREAKER_STATE_OPEN && stored_breaker_state != (uint8_t)BREAKER_STATE_CLOSED))
    {
        nvs_close(storage_handle);
        return ESP_ERR_INVALID_STATE;
    }

    *tripped = stored_tripped == 1U;
    *breaker_state = (BreakerState)stored_breaker_state;
    *state_found= true;

    nvs_close(storage_handle);

    return ESP_OK;
}

esp_err_t event_storage_get_queue_counts(uint32_t *total_count, uint32_t *pending_count)
{
    if(total_count == NULL || pending_count == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *total_count = 0;
    *pending_count = 0;

    nvs_handle_t storage_handle;

    esp_err_t result = nvs_open( EVENT_STORAGE_NAMESPACE, NVS_READONLY, &storage_handle);

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        return result;
    }

    StoredEventQueue queue = {0};
    size_t queue_size = sizeof(queue);

    result = nvs_get_blob(storage_handle, EVENT_STORAGE_QUEUE_KEY, &queue, &queue_size);

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(storage_handle);
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        nvs_close(storage_handle);
        return result;
    }

    if(queue_size != sizeof(queue) || queue.version != EVENT_STORAGE_QUEUE_VERSION || queue.count > EVENT_STORAGE_QUEUE_CAPACITY)
    {
        nvs_close(storage_handle);
        return ESP_ERR_INVALID_STATE;
    }

    for(uint32_t index = 0; index < queue.count; index ++)
    {
        const NodeEventDeliveryState delivery_state = queue.entries[index].delivery_state;

        if(delivery_state == NODE_EVENT_DELIVERY_PENDING)
        {
            (*pending_count)++;
        }
        else if(delivery_state != NODE_EVENT_DELIVERY_ACKNOWLEDGED)
        {
            nvs_close(storage_handle);
            return ESP_ERR_INVALID_STATE;
        }
    }

    *total_count = queue.count;

    nvs_close(storage_handle);

    return ESP_OK;
}

esp_err_t event_storage_load_next_pending(NodeEvent *event, bool *event_found)
{
    if(event == NULL || event_found == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *event_found = false;

    nvs_handle_t storage_handle;

    esp_err_t result = nvs_open( EVENT_STORAGE_NAMESPACE, NVS_READONLY, &storage_handle);

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        return result;
    }

    StoredEventQueue queue = {0};
    size_t queue_size = sizeof(queue);

    result = nvs_get_blob(storage_handle, EVENT_STORAGE_QUEUE_KEY, &queue, &queue_size);

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(storage_handle);
        return ESP_OK;
    }

    if(result != ESP_OK)
    {
        nvs_close(storage_handle);
        return result;
    }

    if(queue_size != sizeof(queue) ||
       queue.version != EVENT_STORAGE_QUEUE_VERSION ||
       queue.count > EVENT_STORAGE_QUEUE_CAPACITY)
    {
        nvs_close(storage_handle);
        return ESP_ERR_INVALID_STATE;
    }

    bool pending_found = false;
    uint32_t pending_index = 0U;

    for(uint32_t index = 0; index < queue.count; index ++)
    {
        const NodeEventDeliveryState delivery_state = queue.entries[index].delivery_state;

        if(delivery_state == NODE_EVENT_DELIVERY_PENDING)
        {
            if(!pending_found)
            {
                pending_index = index;
                pending_found = true;
            }
        }
        else if(delivery_state != NODE_EVENT_DELIVERY_ACKNOWLEDGED)
        {
            nvs_close(storage_handle);
            return ESP_ERR_INVALID_STATE;
        }
    }

    if(pending_found)
    {
        *event = queue.entries[pending_index].event;
        *event_found = true;
    }

    nvs_close(storage_handle);

    return ESP_OK;

}

esp_err_t event_storage_acknowledge(uint32_t sequence)
    {
        if(sequence == 0U)
        {
            return ESP_ERR_INVALID_ARG;
        }

        nvs_handle_t storage_handle;

        esp_err_t result = nvs_open(EVENT_STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if(result != ESP_OK)
    {
        return result;
    }

    StoredEventQueue queue = {0};
    size_t queue_size = sizeof(queue);

    result = nvs_get_blob(storage_handle, EVENT_STORAGE_QUEUE_KEY, &queue, &queue_size);

    if(result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(storage_handle);
        return ESP_ERR_NOT_FOUND;
    }

    if(result != ESP_OK)
    {
        nvs_close(storage_handle);
        return result;
    }

    if(queue_size != sizeof(queue) ||
       queue.version != EVENT_STORAGE_QUEUE_VERSION ||
       queue.count > EVENT_STORAGE_QUEUE_CAPACITY)
    {
        nvs_close(storage_handle);
        return ESP_ERR_INVALID_STATE;
    }

    bool sequence_found = false;
    uint32_t sequence_index = 0;

    for(uint32_t index = 0; index < queue.count; index ++)
    {
        const NodeEventDeliveryState delivery_state = queue.entries[index].delivery_state;

        if(delivery_state != NODE_EVENT_DELIVERY_PENDING &&
           delivery_state != NODE_EVENT_DELIVERY_ACKNOWLEDGED)
        {
            nvs_close(storage_handle);
            return ESP_ERR_INVALID_STATE;
        }

        if(queue.entries[index].event.sequence == sequence)
        {
            if(sequence_found)
            {
                nvs_close(storage_handle);
                return ESP_ERR_INVALID_STATE;
            }

            sequence_index = index;
            sequence_found = true;
        }
    }

    if(!sequence_found)
    {
        nvs_close(storage_handle);
        return ESP_ERR_NOT_FOUND;
    }

    if(queue.entries[sequence_index].delivery_state == NODE_EVENT_DELIVERY_ACKNOWLEDGED)
    {
        nvs_close(storage_handle);
        return ESP_OK;
    }

    queue.entries[sequence_index].delivery_state = NODE_EVENT_DELIVERY_ACKNOWLEDGED;

    result = nvs_set_blob(storage_handle, EVENT_STORAGE_QUEUE_KEY, &queue, sizeof(queue));

    if(result == ESP_OK)
    {
        result = nvs_commit(storage_handle);
    }

    nvs_close(storage_handle);

    return result;
}