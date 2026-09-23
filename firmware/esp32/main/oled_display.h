#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#include "resilient_field_node.h"




esp_err_t oled_display_initialize(
    i2c_master_dev_handle_t oled_device
);

esp_err_t oled_display_show_status(
    i2c_master_dev_handle_t oled_device,
    double current_ma,
    double threshold_ma,
    bool tripped, 
    BreakerState breaker_state
);

#endif