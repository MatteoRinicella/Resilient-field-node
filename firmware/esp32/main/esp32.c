#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "resilient_field_node.h"
#include "oled_display.h"
#include "rtc_ds3231.h"
#include "network_time.h"
#include "node_event.h"
#include "event_storage.h"
#include "gateway_client.h"
#include "wifi_connection.h"

#define RELAY_GPIO GPIO_NUM_27
#define RELAY_ACTIVE_LEVEL 1
#define RELAY_INACTIVE_LEVEL 0

#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_PRESSED_LEVEL 0
#define GATEWAY_SYNC_PERIOD_MS 5000U

#define I2C_SDA_GPIO GPIO_NUM_21
#define I2C_SCL_GPIO GPIO_NUM_22

#define INA219_I2C_ADDRESS 0x40
#define INA219_CONFIGURATION_REGISTER 0x00
#define INA219_SHUNT_VOLTAGE_REGISTER 0x01
#define INA219_SHUNT_RESISTANCE_OHM 0.1

#define OLED_I2C_ADDRESS_PRIMARY 0x3C
#define OLED_I2C_ADDRESS_SECONDARY 0x3D

#define DS3231_I2C_ADDRESS 0x68

#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_PRESSED_LEVEL 0

static esp_err_t read_ina219_current_ma(
  i2c_master_dev_handle_t ina219_device,
  double *current_ma
)
{
  const uint8_t register_address = INA219_SHUNT_VOLTAGE_REGISTER;

  uint8_t register_data[2] = {0};

  const esp_err_t read_result = 
  i2c_master_transmit_receive(
    ina219_device,
    &register_address,
    1,
    register_data,
    2,
    1000
  );

  if(read_result != ESP_OK)
  {
    return read_result;
  }

  const int16_t raw_shunt_voltage =
    (int16_t)(
      ((uint16_t)register_data[0]<<8)
      | register_data[1]
    );

    const double shunt_voltage_mv = raw_shunt_voltage * 0.01;

  *current_ma = shunt_voltage_mv / INA219_SHUNT_RESISTANCE_OHM;

  if(*current_ma > -1.0 && *current_ma <1.0)
  {
    *current_ma = 0.0;
  }
  
  return ESP_OK;
}

static void print_event_queue_status(void)
{
  uint32_t total_count = 0;
  uint32_t pending_count = 0;

  const esp_err_t count_result =
    event_storage_get_queue_counts(&total_count, &pending_count);

  if(count_result != ESP_OK)
  {
    printf(
      "CODA EVENTI: lettura conteggi fallita: %s\n",
      esp_err_to_name(count_result)
    );
    return;
  }

  printf(
    "CODA EVENTI: totali %lu - pending %lu\n",
    (unsigned long)total_count,
    (unsigned long)pending_count
  );

  NodeEvent next_pending_event = {0};
  bool pending_event_found = false;

  const esp_err_t load_result = event_storage_load_next_pending(
    &next_pending_event,
    &pending_event_found
  );

  if(load_result != ESP_OK)
  {
    printf(
      "CODA EVENTI: recupero prossimo pending fallito: %s\n",
      esp_err_to_name(load_result)
    );
    return;
  }

  if(pending_event_found)
  {
    printf(
      "CODA EVENTI: prossimo evento da inviare #%lu\n",
      (unsigned long)next_pending_event.sequence
    );
  }
  else
  {
    printf("CODA EVENTI: nessun evento da inviare\n");
  }
}

static void synchronize_next_pending_event(void)
{
    NodeEvent pending_event = {0};
    bool pending_event_found = false;

    esp_err_t result = event_storage_load_next_pending(
        &pending_event,
        &pending_event_found
    );

    if(result != ESP_OK)
    {
        printf(
            "GATEWAY: lettura evento pending fallita: %s\n",
            esp_err_to_name(result)
        );
        return;
    }

    if(!pending_event_found)
    {
        return;
    }

    printf(
        "GATEWAY: invio evento #%lu...\n",
        (unsigned long)pending_event.sequence
    );

    bool acknowledged = false;

    result = gateway_client_send_event(
        &pending_event,
        &acknowledged
    );

    if(result != ESP_OK)
    {
        printf(
            "GATEWAY: invio evento #%lu fallito: %s\n",
            (unsigned long)pending_event.sequence,
            esp_err_to_name(result)
        );
        return;
    }

    if(!acknowledged)
    {
        printf(
            "GATEWAY: evento #%lu senza ACK\n",
            (unsigned long)pending_event.sequence
        );
        return;
    }

    result = event_storage_acknowledge(
        pending_event.sequence
    );

    if(result != ESP_OK)
    {
        printf(
            "GATEWAY: salvataggio ACK evento #%lu fallito: %s\n",
            (unsigned long)pending_event.sequence,
            esp_err_to_name(result)
        );
        return;
    }

    printf(
        "GATEWAY: evento #%lu ACKNOWLEDGED\n",
        (unsigned long)pending_event.sequence
    );
}

static void wifi_connection_task(void *task_argument)
{
  i2c_master_dev_handle_t rtc_device = (i2c_master_dev_handle_t) task_argument;

  const esp_err_t connection_result = wifi_connect();

  if(connection_result != ESP_OK)
  {
    printf("RETE: connessione wifi non disponbile: %s\n", esp_err_to_name(connection_result));

    vTaskDelete(NULL);
    return;
  }

  printf("RETE: connessione Wi-Fi disponibile\n");

  if(rtc_device == NULL)
  {
    printf("TEMPO: RTC non disponibile, sincronizzazione ignorata\n");

    vTaskDelete(NULL);
    return;
  }

  RtcDateTime network_datetime;

  const esp_err_t ntp_result = network_time_get_local(&network_datetime);

  if(ntp_result != ESP_OK)
  {
    printf("TEMPO: sincronizzazione NTP fallita: %s\n", esp_err_to_name(ntp_result));

    vTaskDelete(NULL);
    return;
  }

  const esp_err_t rtc_set_result = ds3231_set_datetime(rtc_device, &network_datetime);

  if(rtc_set_result == ESP_OK)
  {
    printf("TEMPO: RTC aggiornato da NTP (ora italiana): "
    "%02u/%02u/%04u %02u:%02u:%02u\n",
      (unsigned int)network_datetime.day,
      (unsigned int)network_datetime.month,
      (unsigned int)network_datetime.year,
      (unsigned int)network_datetime.hour,
      (unsigned int)network_datetime.minute,
      (unsigned int)network_datetime.second
    );
  }

  else
  {
    printf("TEMPO: errore aggiornamento RTC: %s\n", esp_err_to_name(rtc_set_result));
  }

    while(true)
  {
    synchronize_next_pending_event();

    vTaskDelay(
      pdMS_TO_TICKS(GATEWAY_SYNC_PERIOD_MS)
    );
  }
}


void app_main(void)
{
    ESP_ERROR_CHECK(gpio_reset_pin(RELAY_GPIO));

    ESP_ERROR_CHECK(
      gpio_set_direction(
        RELAY_GPIO,
        GPIO_MODE_OUTPUT
      )
    );

    ESP_ERROR_CHECK(
      gpio_set_level(
        RELAY_GPIO,
        RELAY_INACTIVE_LEVEL
      )
    );

    ESP_ERROR_CHECK(
      gpio_reset_pin(BOOT_BUTTON_GPIO)
    );

    ESP_ERROR_CHECK(
      gpio_set_direction(
        BOOT_BUTTON_GPIO,
        GPIO_MODE_INPUT
      )
    );

    ESP_ERROR_CHECK(
      gpio_set_pull_mode(
        BOOT_BUTTON_GPIO,
        GPIO_PULLUP_ONLY
      )
    );

    const i2c_master_bus_config_t i2c_bus_config = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = I2C_SDA_GPIO,
      .scl_io_num = I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true
    };

    i2c_master_bus_handle_t i2c_bus;

    ESP_ERROR_CHECK(
      i2c_new_master_bus(
        &i2c_bus_config,
        &i2c_bus
      )
    );

    printf("Resilient Field Node - firmware ESP32 operativo\n");
    printf("GPIO rele': %d - Interruttore iniziale: OPEN (fail-safe)\n", RELAY_GPIO);
    
    const esp_err_t event_storage_result = event_storage_initialize();

    if(event_storage_result != ESP_OK)
    {
      printf("MEMORIA: inizializzazione NVS fallita: %s\n", esp_err_to_name(event_storage_result));

      return;
    }
    printf("MEMORIA: NVS inizializzata correttamente\n");

    NodeEvent latest_event = {0};
    bool latest_event_found = false;

    const esp_err_t event_load_result = event_storage_load_latest(&latest_event, &latest_event_found);

    if(event_load_result != ESP_OK)
    {
      printf("MEMORIA: lettura evento fallita: %s\n", esp_err_to_name(event_load_result));

      return;
    }

    uint32_t next_event_sequence = 1;

    if(latest_event_found)
    {
      next_event_sequence = latest_event.sequence + 1;

      printf(
        "MEMORIA: ultimo evento #%lu recuperato - "
        "prossima sequenza: #%lu\n",
        (unsigned long)latest_event.sequence,
        (unsigned long)next_event_sequence
      );
    }
    else
    {
      printf("MEMORIA: nessun evento persistente trovato\n");
    }

    bool persisted_tripped = false;
    BreakerState persisted_breaker_state = BREAKER_STATE_OPEN;
    bool protection_state_found = false;

    const esp_err_t protection_state_result =
    event_storage_load_protection_state(&persisted_tripped, &persisted_breaker_state, &protection_state_found);

    if(protection_state_result != ESP_OK)
    {
      printf(
        "MEMORIA: lettura stato protezione fallita: %s\n",
        esp_err_to_name(protection_state_result)
      );

      return;
    }

    if(protection_state_found)
    {
      if(persisted_tripped &&
         persisted_breaker_state != BREAKER_STATE_OPEN)
      {
        printf(
          "MEMORIA: stato incoerente, trip attivo "
          "con interruttore non aperto\n"
        );

        return;
    }

      printf(
          "MEMORIA: stato recuperato - Trip: %s - "
          "Interruttore: %s\n",
          bool_to_string(persisted_tripped),
          breaker_state_to_string(
            persisted_breaker_state
          )
        );
    }
    else
    {
      printf(
        "MEMORIA: nessuno stato della protezione salvato\n"
      );
    }

    print_event_queue_status();

    
    const esp_err_t probe_result = 
      i2c_master_probe(
        i2c_bus,
        INA219_I2C_ADDRESS,
        1000
      );


    if(probe_result != ESP_OK)
    {
      printf("INA219 non rilevato: %s\n", esp_err_to_name(probe_result));

      gpio_set_level(
        RELAY_GPIO,
        RELAY_INACTIVE_LEVEL
      );

      return;
    }

      printf("INA219 rilevato all'indirizzo I2C 0x%02X\n", INA219_I2C_ADDRESS);

      uint8_t oled_i2c_address = 0;

      if( i2c_master_probe(i2c_bus, OLED_I2C_ADDRESS_PRIMARY, 1000) == ESP_OK)
      {
        oled_i2c_address = OLED_I2C_ADDRESS_PRIMARY;
      }
      else if( i2c_master_probe(i2c_bus, OLED_I2C_ADDRESS_SECONDARY, 1000) == ESP_OK)
      {
        oled_i2c_address = OLED_I2C_ADDRESS_SECONDARY;
      }

      if(oled_i2c_address != 0)
      {
        printf("SCHERMO OLED rilevato all'indirizzo I2C 0x%02X\n", oled_i2c_address);
      }
      else
      {
        printf("SCHERMO OLED non rilevato: protezione always on\n");
      }

      i2c_master_dev_handle_t oled_device = NULL;

      if(oled_i2c_address != 0)
      {
        const i2c_device_config_t oled_device_config ={
          .dev_addr_length = I2C_ADDR_BIT_LEN_7,
          .device_address = oled_i2c_address,
          .scl_speed_hz = 100000 
        };

        const esp_err_t oled_add_result = 
        i2c_master_bus_add_device(
          i2c_bus,
          &oled_device_config,
          &oled_device
        );


        if(oled_add_result == ESP_OK)
        {
          const esp_err_t oled_initialization_result = oled_display_initialize(oled_device);

          if(oled_initialization_result == ESP_OK)
          {
            printf("Schermo OLED inizializzato\n");
          }
          else
          {
            printf("Errore inizializzazione OLED: %s\n",
            esp_err_to_name(oled_initialization_result));
            oled_device = NULL;
          }
        }
        else
        {
          printf("Errore registrazione OLED: %s\n", esp_err_to_name(oled_add_result));
          oled_device = NULL;
        }
      }

      const esp_err_t rtc_probe_result = 
        i2c_master_probe(
          i2c_bus,
          DS3231_I2C_ADDRESS,
          1000
        );

        i2c_master_dev_handle_t rtc_device = NULL;

        if(rtc_probe_result == ESP_OK)
        {
          printf("RTC DS3231 rilevato all'indirizzo I2C 0x%02X\n", DS3231_I2C_ADDRESS);
        
        
        const i2c_device_config_t rtc_device_config = 
        {
          .dev_addr_length =I2C_ADDR_BIT_LEN_7,
          .device_address = DS3231_I2C_ADDRESS,
          .scl_speed_hz = 100000
        };

        const esp_err_t rtc_add_result =
          i2c_master_bus_add_device(
            i2c_bus,
            &rtc_device_config,
            &rtc_device
          );

        if(rtc_add_result == ESP_OK)
        {
          printf("RTC DS3231 registrato correttamente\n");
        }
        else
        {
          printf("Errore di registrazione RTC: %s\n", esp_err_to_name(rtc_add_result));
          rtc_device = NULL;
        }
      }
      else
      {
        printf("RTC DS3231 non rilevato: eventi senza data e ora\n");
      }

      if(rtc_device != NULL)
      {
        RtcDateTime current_datetime;

        const esp_err_t rtc_read_result =
          ds3231_read_datetime(
            rtc_device,
            &current_datetime
          );

        if( rtc_read_result == ESP_OK)
        {
          printf(
            "Data e ora RTC: "
            "%02u/%02u/%04u "
            "%02u:%02u:%02u\n",
            (unsigned int)current_datetime.day,
            (unsigned int)current_datetime.month,
            (unsigned int)current_datetime.year,
            (unsigned int)current_datetime.hour,
            (unsigned int)current_datetime.minute,
            (unsigned int)current_datetime.second
          );
        }
        else
        {
          printf("Errore lettura RTC: %s\n", esp_err_to_name(rtc_read_result));
        }
      }

      const i2c_device_config_t ina219_device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA219_I2C_ADDRESS,
        .scl_speed_hz = 100000
      };

      i2c_master_dev_handle_t ina219_device;

      ESP_ERROR_CHECK(
        i2c_master_bus_add_device(
          i2c_bus,
          &ina219_device_config,
          &ina219_device
        )

      );

      const uint8_t register_address = INA219_CONFIGURATION_REGISTER;

      uint8_t register_data[2] = {0};

      ESP_ERROR_CHECK(
        i2c_master_transmit_receive(
          ina219_device,
          &register_address,1,
          register_data,
          2,
          1000
        )

      );

      const uint16_t configuration_value =
      ((uint16_t)register_data[0] << 8) | register_data[1];

      printf("Registro configurazione INA219: 0x%04X\n", (unsigned int)configuration_value);

      bool tripped = protection_state_found ? persisted_tripped : latest_event_found;

      BreakerState breaker_state;

      if(protection_state_found)
      {
        breaker_state =persisted_breaker_state;
      }
      else if(latest_event_found)
      {
        breaker_state = BREAKER_STATE_OPEN;
      }
      else
      {
        breaker_state = BREAKER_STATE_CLOSED;
      }

            if(!protection_state_found)
      {
        const esp_err_t initial_state_save_result =
          event_storage_save_protection_state(
            tripped,
            breaker_state
          );

        if(initial_state_save_result != ESP_OK)
        {
          printf(
            "MEMORIA: salvataggio stato iniziale fallito: %s\n",
            esp_err_to_name(initial_state_save_result)
          );

          return;
        }
      }

      ESP_ERROR_CHECK(
        gpio_set_level(
          RELAY_GPIO,
          breaker_state == BREAKER_STATE_CLOSED
            ? RELAY_ACTIVE_LEVEL
            : RELAY_INACTIVE_LEVEL
        )
      );

      printf(
        "INA219 verificato - Trip: %s - "
        "Interruttore: %s\n",
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
      );

      const double threshold_a = 0.017;
      const double sample_period_s = 1.0;
      const double trip_delay_s = 2.0;

      bool overcurrent_active = false;
      double overcurrent_duration_s = 0.0;

      bool previous_boot_button_pressed = false;
      
      unsigned int cycle_number = 1;

      printf("Protezione: soglia %.1f mA - ritardo %.1f s\n", threshold_a * 1000.0, trip_delay_s);

      const BaseType_t wifi_task_result = xTaskCreate(
        wifi_connection_task,
        "wifi_connection",
                8192,
        rtc_device,
        5,
        NULL
      );

      if(wifi_task_result != pdPASS)
      {
        printf("RETE: impossibile avviare l'attivita' Wi-Fi\n");
      }

      while(1)
      {
        double measured_current_ma = 0.0;

        const esp_err_t read_result =
        read_ina219_current_ma(
          ina219_device, 
          &measured_current_ma
        );

        if(read_result != ESP_OK)
        {
          printf("Errore lettura INA219: %s\n", esp_err_to_name(read_result));

          gpio_set_level(
            RELAY_GPIO,
            RELAY_INACTIVE_LEVEL
          );

          return;

        }

        const double measured_current_a = measured_current_ma / 1000.0;

        const bool overcurrent = is_overcurrent(measured_current_a, threshold_a);

        if(overcurrent)
        {
          if(overcurrent_active)
          {
            overcurrent_duration_s += sample_period_s;
          }
          else
          {
            overcurrent_active = true;
            overcurrent_duration_s = 0.0;
          }
        }
        else
        {
          overcurrent_active = false;
          overcurrent_duration_s = 0.0;
        }

        if( !tripped && overcurrent && has_trip_delay_elapsed(overcurrent_duration_s, trip_delay_s))
        {
          tripped = true;
          breaker_state = BREAKER_STATE_OPEN;

          ESP_ERROR_CHECK(
            gpio_set_level(
              RELAY_GPIO,
              RELAY_INACTIVE_LEVEL
            )
          );

          NodeEvent trip_event =
          {
            .sequence = next_event_sequence,
            .type = NODE_EVENT_TYPE_PROTECTION_TRIP,
            .has_timestamp = false,
            .occurred_at = {0},
            .measured_current_ma = measured_current_ma,
            .threshold_ma = threshold_a * 1000.0,
            .breaker_state = breaker_state
          };

          next_event_sequence++;

          if(rtc_device != NULL)
          {
            const esp_err_t trip_time_result = ds3231_read_datetime(rtc_device, &trip_event.occurred_at);

            if(trip_time_result == ESP_OK)
            {
              trip_event.has_timestamp = true;
            }

            else 
            {
              printf("Errore lettura data evento: %s\n",
              esp_err_to_name(trip_time_result));
            }
          }

          const esp_err_t event_save_result = event_storage_save(&trip_event);

          if(event_save_result == ESP_OK)
          {
            printf(
              "MEMORIA: evento #%lu salvato nella NVS\n",
              (unsigned long)trip_event.sequence
            );

            print_event_queue_status();
          }
          else
          {
            printf(
              "MEMORIA: salvataggio evento #%lu fallito: %s\n",
              (unsigned long)trip_event.sequence,
              esp_err_to_name(event_save_result)
            );
          }

          if(trip_event.has_timestamp)
          {
            printf(
              "EVENTO #%lu: PROTECTION_TRIP - "
              "%02u/%02u/%04u %02u:%02u:%02u - "
              "Corrente: %.3f mA - Soglia: %.1f mA - "
              "Interruttore: %s\n",
              (unsigned long)trip_event.sequence,
              (unsigned int)trip_event.occurred_at.day,
              (unsigned int)trip_event.occurred_at.month,
              (unsigned int)trip_event.occurred_at.year,
              (unsigned int)trip_event.occurred_at.hour,
              (unsigned int)trip_event.occurred_at.minute,
              (unsigned int)trip_event.occurred_at.second,
              trip_event.measured_current_ma,
              trip_event.threshold_ma,
              breaker_state_to_string(trip_event.breaker_state)
            );
          }
          else
          {
            printf(
              "EVENTO #%lu: PROTECTION_TRIP - "
              "timestamp non disponibile - "
              "Corrente: %.3f mA - Soglia: %.1f mA - "
              "Interruttore: %s\n",
              (unsigned long)trip_event.sequence,
              trip_event.measured_current_ma,
              trip_event.threshold_ma,
              breaker_state_to_string(trip_event.breaker_state)
            );
          }
        }

        const bool boot_button_pressed = gpio_get_level(BOOT_BUTTON_GPIO) == BOOT_BUTTON_PRESSED_LEVEL;

        if(boot_button_pressed && !previous_boot_button_pressed)
        {
                    if(tripped && !overcurrent)
          {
            const esp_err_t reset_state_save_result =
              event_storage_save_protection_state(
                false,
                BREAKER_STATE_OPEN
              );

            if(reset_state_save_result == ESP_OK)
            {
              tripped = false;
              overcurrent_active = false;
              overcurrent_duration_s = 0.0;

              printf(
                "COMANDO MANUALE: trip resettato - "
                "Interruttore ancora OPEN\n"
              );
            }
            else
            {
              printf(
                "COMANDO MANUALE: reset annullato - "
                "errore memoria: %s\n",
                esp_err_to_name(reset_state_save_result)
              );
            }
          }
          else if(!tripped &&
                  breaker_state == BREAKER_STATE_OPEN)
          {
            const esp_err_t close_state_save_result =
              event_storage_save_protection_state(
                false,
                BREAKER_STATE_CLOSED
              );

            if(close_state_save_result == ESP_OK)
            {
              breaker_state = BREAKER_STATE_CLOSED;

              ESP_ERROR_CHECK(
                gpio_set_level(
                  RELAY_GPIO,
                  RELAY_ACTIVE_LEVEL
                )
              );

              printf(
                "COMANDO MANUALE: Interruttore richiuso\n"
              );
            }
            else
            {
              printf(
                "COMANDO MANUALE: richiusura annullata - "
                "errore memoria: %s\n",
                esp_err_to_name(close_state_save_result)
              );
            }
          }
        }

        previous_boot_button_pressed = boot_button_pressed;

        if(oled_device != NULL)
        {
          const esp_err_t display_result =
          oled_display_show_status(
            oled_device,
            measured_current_ma,
            threshold_a * 1000.0,
            tripped,
            breaker_state
          );

          if(display_result != ESP_OK)
          {
            printf("Errore aggiornamento OLED: %s\n", esp_err_to_name(display_result));

            oled_device = NULL;
          }
        }

        printf(
          "ciclo: %u - Corrente: %.3f mA - "
          " Soglia: %.1f mA - Sovracorrente: %s -"
          "Durata: %.1f s - Trip: %s - "
          "Interruttore: %s\n",
          cycle_number,
          measured_current_ma,
          threshold_a * 1000.0,
          bool_to_string(overcurrent),
          overcurrent_duration_s,
          bool_to_string(tripped),
          breaker_state_to_string(breaker_state)
        );

        cycle_number++;

        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    
}
