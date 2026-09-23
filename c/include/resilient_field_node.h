#ifndef RESILIENT_FIELD_NODE_H
#define RESILIENT_FIELD_NODE_H

#include <stdbool.h>


typedef enum
{
    MEASUREMENT_QUALITY_VALID = 0,
    MEASUREMENT_QUALITY_SUSPECT = 1,
    MEASUREMENT_QUALITY_INVALID = 2,
    MEASUREMENT_QUALITY_STALE = 3
} MeasurementQuality;


typedef enum
{
    BREAKER_STATE_OPEN = 0,
    BREAKER_STATE_CLOSED = 1
} BreakerState;


typedef struct
{
    double time_s;
    double current_a;
    MeasurementQuality quality;
} CurrentMeasurement;


const char *measurement_quality_to_string(
    MeasurementQuality quality
);


const char *breaker_state_to_string(
    BreakerState state
);


const char *bool_to_string(
    bool value
);


bool is_overcurrent(
    double current_a,
    double threshold_a
);


bool has_trip_delay_elapsed(
    double elapsed_s,
    double trip_delay_s
);


bool is_quality_usable_for_protection(
    MeasurementQuality quality
);


bool can_reset_trip(
    bool tripped,
    CurrentMeasurement measurement,
    double threshold_a
);


bool can_close_breaker(
    bool tripped,
    MeasurementQuality measurement_quality
);


#endif
