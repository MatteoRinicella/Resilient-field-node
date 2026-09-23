#include <assert.h>
#include <stdio.h>

#include "resilient_field_node.h"


int main(void)
{
    const double threshold_a = 8.0;
    const double trip_delay_s = 2.0;

    assert(
        is_overcurrent(8.1, threshold_a)
    );

    assert(
        !is_overcurrent(8.0, threshold_a)
    );

    assert(
        !is_overcurrent(2.0, threshold_a)
    );

    printf("Test soglia: OK\n");


    assert(
        !has_trip_delay_elapsed(1.9, trip_delay_s)
    );

    assert(
        has_trip_delay_elapsed(2.0, trip_delay_s)
    );

    assert(
        has_trip_delay_elapsed(3.0, trip_delay_s)
    );

    printf("Test ritardo trip: OK\n");


    assert(
        is_quality_usable_for_protection(
            MEASUREMENT_QUALITY_VALID
        )
    );

    assert(
        is_quality_usable_for_protection(
            MEASUREMENT_QUALITY_SUSPECT
        )
    );

    assert(
        !is_quality_usable_for_protection(
            MEASUREMENT_QUALITY_INVALID
        )
    );

    assert(
        !is_quality_usable_for_protection(
            MEASUREMENT_QUALITY_STALE
        )
    );

    printf("Test qualita misura: OK\n");


    CurrentMeasurement safe_measurement = {
        .time_s = 1.0,
        .current_a = 2.0,
        .quality = MEASUREMENT_QUALITY_VALID
    };

    CurrentMeasurement invalid_measurement = {
        .time_s = 1.0,
        .current_a = 2.0,
        .quality = MEASUREMENT_QUALITY_INVALID
    };

    CurrentMeasurement high_current_measurement = {
        .time_s = 1.0,
        .current_a = 12.0,
        .quality = MEASUREMENT_QUALITY_VALID
    };

    assert(
        !can_reset_trip(
            false,
            safe_measurement,
            threshold_a
        )
    );

    assert(
        !can_reset_trip(
            true,
            invalid_measurement,
            threshold_a
        )
    );

    assert(
        !can_reset_trip(
            true,
            high_current_measurement,
            threshold_a
        )
    );

    assert(
        can_reset_trip(
            true,
            safe_measurement,
            threshold_a
        )
    );

    printf("Test reset protezione: OK\n");


    assert(
        !can_close_breaker(
            true,
            MEASUREMENT_QUALITY_VALID
        )
    );

    assert(
        !can_close_breaker(
            false,
            MEASUREMENT_QUALITY_INVALID
        )
    );

    assert(
        !can_close_breaker(
            false,
            MEASUREMENT_QUALITY_SUSPECT
        )
    );

    assert(
        can_close_breaker(
            false,
            MEASUREMENT_QUALITY_VALID
        )
    );

    printf("Test richiusura interruttore: OK\n");


    printf("Tutti i test C sono stati superati\n");

    return 0;
}
