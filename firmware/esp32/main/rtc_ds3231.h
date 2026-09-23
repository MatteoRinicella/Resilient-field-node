#ifndef RTC_DS3231_h
#define RTC_DS3231_h

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct 
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} RtcDateTime;

esp_err_t ds3231_read_datetime(
    i2c_master_dev_handle_t rtc_device,
    RtcDateTime *datetime
);

esp_err_t ds3231_set_datetime(
    i2c_master_dev_handle_t rtc_device,
    const RtcDateTime *datetime
);

#endif