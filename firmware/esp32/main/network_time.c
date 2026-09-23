#include "network_time.h"

#include <time.h>

#include "freertos/FreeRTOS.h"
#include "esp_netif_sntp.h"

#define NETWORK_TIME_SERVER "pool.ntp.org"
#define NETWORK_TIME_TIMEOUT_MS 15000

esp_err_t network_time_get_utc( RtcDateTime *datetime)
{
    if(datetime == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_sntp_config_t sntp_configuration = ESP_NETIF_SNTP_DEFAULT_CONFIG(NETWORK_TIME_SERVER);

    esp_err_t result = esp_netif_sntp_init(&sntp_configuration);

    if(result != ESP_OK)
    {
        return result;
    }

    result = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(NETWORK_TIME_TIMEOUT_MS));

    if(result != ESP_OK)
    {
        esp_netif_sntp_deinit();
        return result;
    }

    time_t unix_time = 0;

    if(time(&unix_time) == (time_t)-1)
    {
        esp_netif_sntp_deinit();
        return ESP_ERR_INVALID_STATE;
    }

    struct tm utc_time;

    if(gmtime_r(&unix_time, &utc_time) == NULL)
    {
        esp_netif_sntp_deinit();
        return ESP_ERR_INVALID_STATE;
    }

    const int full_year = utc_time.tm_year + 1900;

    if(full_year < 2000 || full_year >2199)
    {
        esp_netif_sntp_deinit();
        return ESP_ERR_INVALID_STATE;
    }

    datetime->year =
        (uint16_t)full_year;

    datetime->month =
        (uint8_t)(utc_time.tm_mon + 1);

    datetime->day =
        (uint8_t)utc_time.tm_mday;

    datetime->hour =
        (uint8_t)utc_time.tm_hour;

    datetime->minute =
        (uint8_t)utc_time.tm_min;

    datetime->second =
        (uint8_t)utc_time.tm_sec;

    esp_netif_sntp_deinit();

    return ESP_OK;
}