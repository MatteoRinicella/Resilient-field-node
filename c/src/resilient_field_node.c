#include "resilient_field_node.h"


const char *measurement_quality_to_string(
    MeasurementQuality quality
)
{
    switch (quality)
    {
        case MEASUREMENT_QUALITY_VALID:
            return "VALID";

        case MEASUREMENT_QUALITY_SUSPECT:
            return "SUSPECT";

        case MEASUREMENT_QUALITY_INVALID:
            return "INVALID";

        case MEASUREMENT_QUALITY_STALE:
            return "STALE";

        default:
            return "UNKNOWN";
    }
}


const char *breaker_state_to_string(
    BreakerState state
)
{
    switch (state)
    {
        case BREAKER_STATE_OPEN:
            return "OPEN";

        case BREAKER_STATE_CLOSED:
            return "CLOSED";

        default:
            return "UNKNOWN";
    }
}


const char *bool_to_string(bool value)
{
    if (value)
    {
        return "TRUE";
    }

    return "FALSE";
}


bool is_overcurrent(
    double current_a,
    double threshold_a
)
{
    return current_a > threshold_a;
}


bool has_trip_delay_elapsed(
    double elapsed_s,
    double trip_delay_s
)
{
    return elapsed_s >= trip_delay_s;
}


bool is_quality_usable_for_protection(
    MeasurementQuality quality
)
{
    return (
        quality != MEASUREMENT_QUALITY_INVALID
        && quality != MEASUREMENT_QUALITY_STALE
    );
}


bool can_reset_trip(
    bool tripped,
    CurrentMeasurement measurement,
    double threshold_a
)
{
    if (!tripped)
    {
        return false;
    }

    if (measurement.quality != MEASUREMENT_QUALITY_VALID)
    {
        return false;
    }

    if (is_overcurrent(
        measurement.current_a,
        threshold_a
    ))
    {
        return false;
    }

    return true;
}


bool can_close_breaker(
    bool tripped,
    MeasurementQuality measurement_quality
)
{
    if (tripped)
    {
        return false;
    }

    if (measurement_quality != MEASUREMENT_QUALITY_VALID)
    {
        return false;
    }

    return true;
}
