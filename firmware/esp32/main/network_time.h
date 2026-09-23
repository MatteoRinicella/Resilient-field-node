#ifndef NETWORK_TIME_H
#define NETWORK_TIME_H

#include "esp_err.h"
#include "rtc_ds3231.h"

esp_err_t network_time_get_utc(RtcDateTime *datetime);

#endif