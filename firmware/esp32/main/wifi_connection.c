#include "wifi_connection.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define WIFI_MAXIMUM_RETRY 10

static EventGroupHandle_t wifi_event_group;
static unsigned int wifi_retry_count = 0;

static void wifi_event_handler(
    void *handler_argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)handler_argument;

    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        return;
    }

    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        const wifi_event_sta_disconnected_t *disconnection_event =
            (const wifi_event_sta_disconnected_t *)event_data;

        printf(
            "Wi-Fi disconnesso - codice motivo: %u\n",
            (unsigned int)disconnection_event->reason
        );

        if(wifi_retry_count < WIFI_MAXIMUM_RETRY)
        {
            wifi_retry_count++;

            printf("Wi-Fi: nuovo tentativo di connessione %u/%u\n", wifi_retry_count,(unsigned int)WIFI_MAXIMUM_RETRY);

            esp_wifi_connect();
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAILED_BIT);
        }

        return;
    }

    if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;

        printf("Wi-Fi connesso - IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));

        wifi_retry_count = 0;

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_connect(void)
{
    esp_err_t result = nvs_flash_init();

    if( result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        result = nvs_flash_erase();

        if(result != ESP_OK)
        {
            return result;
        }

        result = nvs_flash_init();
    }

    if(result != ESP_OK)
        {
            return result;
        }

    wifi_event_group = xEventGroupCreate();

    if(wifi_event_group == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    result = esp_netif_init();

    if(result != ESP_OK)
        {
            return result;
        }
    
    result = esp_event_loop_create_default();

    if(result != ESP_OK && result != ESP_ERR_INVALID_STATE)
    {
        return result;
    }

    esp_netif_t *wifi_station = esp_netif_create_default_wifi_sta();

    if(wifi_station == NULL)
    {
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_initialization = WIFI_INIT_CONFIG_DEFAULT();

    result = esp_wifi_init(&wifi_initialization);

    if(result != ESP_OK)
    {
        return result;
    }

    esp_event_handler_instance_t wifi_event_instance;
    esp_event_handler_instance_t ip_event_instance;

    result = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL,
        &wifi_event_instance
    );

    if(result != ESP_OK)
    {
        return result;
    }

    result = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL,
        &ip_event_instance
    );

    if(result != ESP_OK)
    {
        return result;
    }


    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = CONFIG_RFN_WIFI_SSID,
            .password = CONFIG_RFN_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    result = esp_wifi_set_mode(WIFI_MODE_STA);

    if(result != ESP_OK)
    {
        return result;
    }

    result = esp_wifi_set_config(
        WIFI_IF_STA,
        &wifi_configuration
    );

    if(result != ESP_OK)
    {
        return result;
    }

    result = esp_wifi_start();

    if(result != ESP_OK)
    {
        return result;
    }

    printf("Wi-fi: connessione alla rete %s...\n", CONFIG_RFN_WIFI_SSID);

    const EventBits_t event_bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if ((event_bits & WIFI_CONNECTED_BIT) != 0)
    {
        return ESP_OK;
    }

    if((event_bits & WIFI_FAILED_BIT) != 0)
    {
        printf("Wi-fi: impossibile collegarsi alla rete %s\n", CONFIG_RFN_WIFI_SSID);
        
        return ESP_FAIL;
    }

    return ESP_FAIL;

}
