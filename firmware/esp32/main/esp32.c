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

#include "wifi_connection.h"

#define RELAY_GPIO GPIO_NUM_27
#define RELAY_ACTIVE_LEVEL 1
#define RELAY_INACTIVE_LEVEL 0

#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_PRESSED_LEVEL 0

#define I2C_SDA_GPIO GPIO_NUM_21
#define I2C_SCL_GPIO GPIO_NUM_22

#define INA219_I2C_ADDRESS 0x40
#define INA219_CONFIGURATION_REGISTER 0x00
#define INA219_SHUNT_VOLTAGE_REGISTER 0x01
#define INA219_SHUNT_RESISTANCE_OHM 0.1

#define OLED_I2C_ADDRESS_PRIMARY 0x3C
#define OLED_I2C_ADDRESS_SECONDARY 0x3D

#define DS3231_I2C_ADDRESS 0x68

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

static void wifi_connection_task(void *task_argument)
{
  (void)task_argument;

  const esp_err_t connection_result = wifi_connect();

  if(connection_result == ESP_OK)
  {
    printf("RETE: connessione Wi-fi disponibile\n");
  }
  else
  {
    printf("Rete: connessione Wi-Fi non disponibile: %s\n", esp_err_to_name(connection_result));
  }

  vTaskDelete(NULL);

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

      ESP_ERROR_CHECK(
        gpio_set_level(
          RELAY_GPIO,
          RELAY_ACTIVE_LEVEL
        )
      );

      printf("INA219 verificato - Interruttore: CLOSED\n");

      const double threshold_a = 0.017;
      const double sample_period_s = 1.0;
      const double trip_delay_s = 2.0;

      bool overcurrent_active = false;
      double overcurrent_duration_s = 0.0;
      bool tripped = false;
      BreakerState breaker_state = BREAKER_STATE_CLOSED;

      bool previous_boot_button_pressed = false;
      
      unsigned int cycle_number = 1;

      printf("Protezione: soglia %.1f mA - ritardo %.1f s\n", threshold_a * 1000.0, trip_delay_s);

      const BaseType_t wifi_task_result = xTaskCreate(
        wifi_connection_task,
        "wifi_connection",
        4096,
        NULL,
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

          printf("EVENTO: PROTECTION_TRIP\n");
        }

        const bool boot_button_pressed = gpio_get_level(BOOT_BUTTON_GPIO) == BOOT_BUTTON_PRESSED_LEVEL;

        if(boot_button_pressed && !previous_boot_button_pressed)
        {
          if(tripped && !overcurrent)
          {
            tripped = false;
            overcurrent_active = false;
            overcurrent_duration_s = 0.0;

            printf("COMANDO MANUALE: trip resettato - "
            "Interruttore ancora OPEN\n"
            );
          }
          else if(!tripped && breaker_state == BREAKER_STATE_OPEN)
          {
            breaker_state = BREAKER_STATE_CLOSED;

            ESP_ERROR_CHECK(
              gpio_set_level(
                RELAY_GPIO,
                RELAY_ACTIVE_LEVEL
              )
            );

            printf("COMANDO MANUALE: Interruttore richiuso\n");
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
