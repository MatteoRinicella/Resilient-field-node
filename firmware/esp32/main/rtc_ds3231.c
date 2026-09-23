#include "rtc_ds3231.h"

#include <stdbool.h>

#define DS3231_SECONDS_REGISTER 0x00
#define DS3231_TIMEOUT_MS 1000

static uint8_t bcd_to_decimal(
    uint8_t bcd_value
)
{
    return (uint8_t)(
        ((bcd_value >> 4) * 10) + (bcd_value & 0x0F)
    );
}

static uint8_t decimal_to_bcd(uint8_t decimal_value)
{
    return(uint8_t)(((decimal_value /10) <<4) | (decimal_value % 10));
}

esp_err_t ds3231_read_datetime(
    i2c_master_dev_handle_t rtc_device,
    RtcDateTime *datetime
)
{
    if(datetime == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t first_register = DS3231_SECONDS_REGISTER;

    uint8_t register_data[7] = {0};

    const esp_err_t read_result =
        i2c_master_transmit_receive(
            rtc_device,
            &first_register,
            1,
            register_data,
            sizeof(register_data),
            DS3231_TIMEOUT_MS
        );

    if(read_result != ESP_OK)
    {
        return read_result;
    }

    datetime->second = bcd_to_decimal(register_data[0] & 0x7F);
    datetime->minute = bcd_to_decimal(register_data[1] & 0x7F) ;

    const uint8_t raw_hour = register_data[2];

    if ((raw_hour & 0x40) != 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    datetime->hour = bcd_to_decimal(raw_hour & 0x3F);

    datetime->day = bcd_to_decimal(register_data[4] & 0x3F);

    const bool century_bit = (register_data[5] & 0x80) != 0;

    datetime->month = bcd_to_decimal(register_data[5] & 0x1F);

    datetime->year = (uint16_t)(century_bit ? 2100 : 2000) + bcd_to_decimal(register_data[6]);

    return ESP_OK;
}

esp_err_t ds3231_set_datetime(i2c_master_dev_handle_t rtc_device, const RtcDateTime *datetime)
    {
        if(datetime == NULL)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if(datetime->year < 2000 ||
        datetime->year > 2199 ||
        datetime->month < 1 ||
        datetime->month > 12 ||
        datetime->day < 1 ||
        datetime->day > 31 ||
        datetime->hour > 23 ||
        datetime->minute > 59 ||
        datetime->second > 59)
        {
            return ESP_ERR_INVALID_ARG;
        }

        uint8_t month_register = decimal_to_bcd(datetime->month);

        if(datetime->year >= 2100)
        {
            month_register |= 0x80;
        }

        const uint8_t register_data[] = {
            DS3231_SECONDS_REGISTER,
            decimal_to_bcd(datetime->second),
            decimal_to_bcd(datetime->minute),
            decimal_to_bcd(datetime->hour),
            1,
            decimal_to_bcd(datetime->day),
            month_register,
            decimal_to_bcd((uint8_t)(datetime->year % 100))
        };

        return i2c_master_transmit(
            rtc_device,
            register_data,
            sizeof(register_data),
            DS3231_TIMEOUT_MS
        );
    }
