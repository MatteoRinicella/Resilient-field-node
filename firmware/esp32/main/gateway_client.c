#include "gateway_client.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "sdkconfig.h"

#define GATEWAY_RESPONSE_CAPACITY 256U
#define GATEWAY_JSON_CAPACITY 512U
#define GATEWAY_IDENTIFIER_CAPACITY 64U
#define GATEWAY_AUTHORIZATION_CAPACITY 128U
#define GATEWAY_TIMESTAMP_CAPACITY 32U
#define GATEWAY_HTTP_TIMEOUT_MS 15000

typedef struct 
{
    char data[GATEWAY_RESPONSE_CAPACITY];
    size_t length;
    bool truncated;
} GatewayHttpResponse;

static char gateway_device_id[GATEWAY_IDENTIFIER_CAPACITY] = {0};
static char gateway_boot_id[GATEWAY_IDENTIFIER_CAPACITY] = {0};
static bool gateway_identity_initialized = false;

static esp_err_t gateway_http_event_handler(esp_http_client_event_t *event)
{
    if(event == NULL || event->user_data == NULL){
        return ESP_ERR_INVALID_ARG;
    }

    if(event->event_id != HTTP_EVENT_ON_DATA || event->data == NULL || event->data_len <= 0)
    {
        return ESP_OK;
    }

    GatewayHttpResponse *response = (GatewayHttpResponse *)event->user_data;

    if(response->length >= sizeof(response->data) -1U)
    {
        response->truncated = true;
        return ESP_OK;
    }

    const size_t received_length = (size_t)event->data_len;

    const size_t remaining_capacity = sizeof(response->data) - response->length -1U;

    const size_t copy_length = received_length < remaining_capacity ? received_length : remaining_capacity;

    memcpy(&response->data[response->length], event->data, copy_length);

    response->length += copy_length;
    response->data[response->length] = '\0';

    if(copy_length < received_length)
    {
        response->truncated = true;
    }

    return ESP_OK;
}

static esp_err_t gateway_initialize_identity(void)
{
    if(gateway_identity_initialized)
    {
        return ESP_OK;
    }

    uint8_t mac_address[6] = {0};

    const esp_err_t mac_result = esp_read_mac(mac_address, ESP_MAC_WIFI_STA);

    if(mac_result != ESP_OK)
    {
        return mac_result;
    }

    const int device_id_length = snprintf(
        gateway_device_id,
        sizeof(gateway_device_id),
        "esp32-%02x%02x%02x%02x%02x%02x",
        (unsigned int)mac_address[0],
        (unsigned int)mac_address[1],
        (unsigned int)mac_address[2],
        (unsigned int)mac_address[3],
        (unsigned int)mac_address[4],
        (unsigned int)mac_address[5]
    );

    if(device_id_length < 0 || (size_t)device_id_length >= sizeof(gateway_device_id))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint32_t random_high = esp_random();
    const uint32_t random_low = esp_random();

    const int boot_id_length = snprintf(
        gateway_boot_id,
        sizeof(gateway_boot_id),
        "boot-%08lx%08lx",
        (unsigned long)random_high,
        (unsigned long)random_low
    );

    if(boot_id_length < 0 || (size_t)boot_id_length >= sizeof(gateway_boot_id))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    gateway_identity_initialized = true;

    return ESP_OK;
}

static esp_err_t  gateway_format_timestamp(const RtcDateTime *datetime, char *timestamp, size_t timestamp_capacity)
{
    if(datetime == NULL || timestamp == NULL || timestamp_capacity == 0U)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if(
        datetime->year < 2000U
        || datetime->year > 2199U
        || datetime->month < 1U
        || datetime->month > 12U
        || datetime->day < 1U
        || datetime->day > 31U
        || datetime->hour > 23U
        || datetime->minute > 59U
        || datetime->second > 59U
    )
    {
        return ESP_ERR_INVALID_ARG;
    }

    struct tm local_time = 
    {
        .tm_year = (int)datetime->year - 1900,
        .tm_mon = (int)datetime->month - 1,
        .tm_mday = (int)datetime->day,
        .tm_hour = (int)datetime->hour,
        .tm_min = (int)datetime->minute,
        .tm_sec = (int)datetime->second,
        .tm_isdst = -1
    };

    if(mktime(&local_time) == (time_t)-1)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t formatted_length = strftime(
        timestamp,
        timestamp_capacity,
        "%Y-%m-%dT%H:%M:%S%z",
        &local_time
    );

    if(formatted_length == 0U)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static esp_err_t gateway_build_event_id(
    const NodeEvent *event,
    char *event_id,
    size_t event_id_capacity
)
{
    if(
        event == NULL
        || event_id == NULL
        || event_id_capacity == 0U
        || event->sequence == 0U
    )
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t identity_result =
        gateway_initialize_identity();

    if(identity_result != ESP_OK)
    {
        return identity_result;
    }

    const int event_id_length = snprintf(
        event_id,
        event_id_capacity,
        "%s-event-%lu",
        gateway_device_id,
        (unsigned long)event->sequence
    );

    if(
        event_id_length < 0
        || (size_t)event_id_length >= event_id_capacity
    )
    {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static esp_err_t gateway_build_event_json(
    const NodeEvent *event,
    char *json,
    size_t json_capacity
)
{
    if(
        event == NULL
        || json == NULL
        || json_capacity == 0U
    )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if(
        !event->has_timestamp
        || event->type != NODE_EVENT_TYPE_PROTECTION_TRIP
        || event->breaker_state != BREAKER_STATE_OPEN
        || event->threshold_ma <= 0.0
    )
    {
        return ESP_ERR_INVALID_STATE;
    }

    char timestamp[GATEWAY_TIMESTAMP_CAPACITY] = {0};

    esp_err_t result = gateway_format_timestamp(
        &event->occurred_at,
        timestamp,
        sizeof(timestamp)
    );

    if(result != ESP_OK)
    {
        return result;
    }

    char event_id[GATEWAY_IDENTIFIER_CAPACITY] = {0};

    result = gateway_build_event_id(
        event,
        event_id,
        sizeof(event_id)
    );

    if(result != ESP_OK)
    {
        return result;
    }

    const int json_length = snprintf(
        json,
        json_capacity,
        "{"
        "\"event_id\":\"%s\","
        "\"device_id\":\"%s\","
        "\"boot_id\":\"%s\","
        "\"sequence\":%lu,"
        "\"event_type\":\"PROTECTION_TRIP\","
        "\"occurred_at\":\"%s\","
        "\"measured_current_ma\":%.3f,"
        "\"threshold_ma\":%.3f,"
        "\"breaker_state\":\"OPEN\""
        "}",
        event_id,
        gateway_device_id,
        gateway_boot_id,
        (unsigned long)event->sequence,
        timestamp,
        event->measured_current_ma,
        event->threshold_ma
    );

    if(
        json_length < 0
        || (size_t)json_length >= json_capacity
    )
    {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static esp_err_t gateway_build_state_json(
    const GatewayNodeState *state,
    char *json,
    size_t json_capacity
)
{
    if(
        state == NULL
        || json == NULL
        || json_capacity == 0U
    )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if(
        (
            state->breaker_state != BREAKER_STATE_OPEN
            && state->breaker_state != BREAKER_STATE_CLOSED
        )
        || (
            state->tripped
            && state->breaker_state != BREAKER_STATE_OPEN
        )
    )
    {
        return ESP_ERR_INVALID_STATE;
    }

    char timestamp[GATEWAY_TIMESTAMP_CAPACITY] = {0};

    esp_err_t result = gateway_format_timestamp(
        &state->reported_at,
        timestamp,
        sizeof(timestamp)
    );

    if(result != ESP_OK)
    {
        return result;
    }

    result = gateway_initialize_identity();

    if(result != ESP_OK)
    {
        return result;
    }

    const char *breaker_state_text =
        state->breaker_state == BREAKER_STATE_OPEN
        ? "OPEN"
        : "CLOSED";

    const char *tripped_text =
        state->tripped
        ? "true"
        : "false";

    const int json_length = snprintf(
        json,
        json_capacity,
        "{"
        "\"device_id\":\"%s\","
        "\"boot_id\":\"%s\","
        "\"reported_at\":\"%s\","
        "\"last_event_sequence\":%lu,"
        "\"tripped\":%s,"
        "\"breaker_state\":\"%s\""
        "}",
        gateway_device_id,
        gateway_boot_id,
        timestamp,
        (unsigned long)state->last_event_sequence,
        tripped_text,
        breaker_state_text
    );

    if(
        json_length < 0
        || (size_t)json_length >= json_capacity
    )
    {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static esp_err_t gateway_send_json(
    const char *url,
    const char *json,
    bool *acknowledged
)
{
    if(
        url == NULL
        || json == NULL
        || acknowledged == NULL
    )
    {
        return ESP_ERR_INVALID_ARG;
    }

    *acknowledged = false;

    if(
        url[0] == '\0'
        || CONFIG_RFN_GATEWAY_API_TOKEN[0] == '\0'
    )
    {
        return ESP_ERR_INVALID_STATE;
    }

    char authorization[GATEWAY_AUTHORIZATION_CAPACITY] = {0};

    const int authorization_length = snprintf(
        authorization,
        sizeof(authorization),
        "Bearer %s",
        CONFIG_RFN_GATEWAY_API_TOKEN
    );

    if(
        authorization_length < 0
        || (size_t)authorization_length >= sizeof(authorization)
    )
    {
        return ESP_ERR_INVALID_SIZE;
    }

    GatewayHttpResponse response = {0};

    const esp_http_client_config_t configuration =
    {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = GATEWAY_HTTP_TIMEOUT_MS,
        .event_handler = gateway_http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&configuration);

    if(client == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = esp_http_client_set_header(
        client,
        "Content-Type",
        "application/json"
    );

    if(result == ESP_OK)
    {
        result = esp_http_client_set_header(
            client,
            "Authorization",
            authorization
        );
    }

    if(result == ESP_OK)
    {
        result = esp_http_client_set_post_field(
            client,
            json,
            (int)strlen(json)
        );
    }

    int status_code = 0;

    if(result == ESP_OK)
    {
        result = esp_http_client_perform(client);

        if(result == ESP_OK)
        {
            status_code =
                esp_http_client_get_status_code(client);
        }
    }

    const esp_err_t cleanup_result =
        esp_http_client_cleanup(client);

    if(result != ESP_OK)
    {
        return result;
    }

    if(cleanup_result != ESP_OK)
    {
        return cleanup_result;
    }

    if(status_code != 200)
    {
        return ESP_FAIL;
    }

    if(response.truncated)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    const bool compact_ack =
        strstr(
            response.data,
            "\"acknowledged\":true"
        ) != NULL;

    const bool spaced_ack =
        strstr(
            response.data,
            "\"acknowledged\": true"
        ) != NULL;

    *acknowledged = compact_ack || spaced_ack;

    return *acknowledged ? ESP_OK : ESP_FAIL;
}

esp_err_t gateway_client_send_event(
    const NodeEvent *event,
    bool *acknowledged
)
{
    if(event == NULL || acknowledged == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *acknowledged = false;

    char json[GATEWAY_JSON_CAPACITY] = {0};

    const esp_err_t result = gateway_build_event_json(
        event,
        json,
        sizeof(json)
    );

    if(result != ESP_OK)
    {
        return result;
    }

    return gateway_send_json(
        CONFIG_RFN_GATEWAY_URL,
        json,
        acknowledged
    );
}

esp_err_t gateway_client_send_state(
    const GatewayNodeState *state,
    bool *acknowledged
)
{
    if(state == NULL || acknowledged == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *acknowledged = false;

    char json[GATEWAY_JSON_CAPACITY] = {0};

    const esp_err_t result = gateway_build_state_json(
        state,
        json,
        sizeof(json)
    );

    if(result != ESP_OK)
    {
        return result;
    }

    return gateway_send_json(
        CONFIG_RFN_GATEWAY_STATE_URL,
        json,
        acknowledged
    );
}