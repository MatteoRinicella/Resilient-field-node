#include <stdio.h>

#include "resilient_field_node.h"


int main(void)
{
    const double threshold_a = 8.0;
    const double trip_delay_s = 2.0;
    const int measurement_count = 9;

    CurrentMeasurement measurements[9] = {
        {
            .time_s = 0.0,
            .current_a = 2.0,
            .quality = MEASUREMENT_QUALITY_VALID
        },
        {
            .time_s = 1.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_VALID
        },
        {
            .time_s = 2.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_INVALID
        },
        {
            .time_s = 3.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_VALID
        },
        {
            .time_s = 4.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_STALE
        },
        {
            .time_s = 5.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_SUSPECT
        },
        {
            .time_s = 6.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_VALID
        },
        {
            .time_s = 7.0,
            .current_a = 12.0,
            .quality = MEASUREMENT_QUALITY_VALID
        },
        {
            .time_s = 8.0,
            .current_a = 2.0,
            .quality = MEASUREMENT_QUALITY_VALID
        }
    };

    bool overcurrent_active = false;
    double overcurrent_started_at_s = 0.0;
    double overcurrent_duration_s = 0.0;
    bool tripped = false;

    BreakerState breaker_state = BREAKER_STATE_CLOSED;

    printf(
        "Soglia: %.1f A - Ritardo trip: %.1f s\n",
        threshold_a,
        trip_delay_s
    );

    for (int index = 0; index < measurement_count; index++)
    {
        CurrentMeasurement measurement = measurements[index];

        bool quality_usable = is_quality_usable_for_protection(
            measurement.quality
        );

        bool overcurrent = false;

        if (quality_usable)
        {
            overcurrent = is_overcurrent(
                measurement.current_a,
                threshold_a
            );
        }

        bool trip_occurred = false;

        if (!quality_usable || !overcurrent)
        {
            overcurrent_active = false;
            overcurrent_started_at_s = 0.0;
            overcurrent_duration_s = 0.0;
        }
        else
        {
            if (!overcurrent_active)
            {
                overcurrent_active = true;
                overcurrent_started_at_s = measurement.time_s;
            }

            overcurrent_duration_s = (
                measurement.time_s
                - overcurrent_started_at_s
            );
        }

        if (
            !tripped
            && quality_usable
            && overcurrent
            && has_trip_delay_elapsed(
                overcurrent_duration_s,
                trip_delay_s
            )
        )
        {
            tripped = true;
            trip_occurred = true;
        }

        if (trip_occurred)
        {
            breaker_state = BREAKER_STATE_OPEN;
            printf("EVENTO: PROTECTION_TRIP\n");
        }

        printf(
            "Tempo: %.1f s - Corrente: %.1f A"
            " - Qualita misura: %s"
            " - Utilizzabile: %s"
            " - Sovracorrente: %s"
            " - Durata: %.1f s"
            " - Trip: %s"
            " - Interruttore: %s\n",
            measurement.time_s,
            measurement.current_a,
            measurement_quality_to_string(measurement.quality),
            bool_to_string(quality_usable),
            bool_to_string(overcurrent),
            overcurrent_duration_s,
            bool_to_string(tripped),
            breaker_state_to_string(breaker_state)
        );
    }

    printf(
        "\nCOMANDO MANUALE: richiusura con trip attivo\n"
    );

    bool close_executed = can_close_breaker(
        tripped,
        MEASUREMENT_QUALITY_VALID
    );

    if (close_executed)
    {
        breaker_state = BREAKER_STATE_CLOSED;
    }

    printf(
        "Richiusura eseguita: %s"
        " - Trip: %s"
        " - Interruttore: %s\n",
        bool_to_string(close_executed),
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
    );

    CurrentMeasurement invalid_reset_measurement = {
        .time_s = 9.0,
        .current_a = 2.0,
        .quality = MEASUREMENT_QUALITY_INVALID
    };

    printf(
        "\nCOMANDO MANUALE: reset con misura INVALID\n"
    );

    bool reset_executed = can_reset_trip(
        tripped,
        invalid_reset_measurement,
        threshold_a
    );

    if (reset_executed)
    {
        tripped = false;
        overcurrent_active = false;
        overcurrent_started_at_s = 0.0;
        overcurrent_duration_s = 0.0;
    }

    printf(
        "Reset eseguito: %s - Trip: %s"
        " - Interruttore: %s\n",
        bool_to_string(reset_executed),
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
    );

    CurrentMeasurement high_current_reset_measurement = {
        .time_s = 10.0,
        .current_a = 12.0,
        .quality = MEASUREMENT_QUALITY_VALID
    };

    printf(
        "\nCOMANDO MANUALE: reset con corrente sopra soglia\n"
    );

    reset_executed = can_reset_trip(
        tripped,
        high_current_reset_measurement,
        threshold_a
    );

    if (reset_executed)
    {
        tripped = false;
        overcurrent_active = false;
        overcurrent_started_at_s = 0.0;
        overcurrent_duration_s = 0.0;
    }

    printf(
        "Reset eseguito: %s - Trip: %s"
        " - Interruttore: %s\n",
        bool_to_string(reset_executed),
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
    );

    CurrentMeasurement safe_reset_measurement = {
        .time_s = 11.0,
        .current_a = 2.0,
        .quality = MEASUREMENT_QUALITY_VALID
    };

    printf(
        "\nCOMANDO MANUALE: reset con misura valida e sicura\n"
    );

    reset_executed = can_reset_trip(
        tripped,
        safe_reset_measurement,
        threshold_a
    );

    if (reset_executed)
    {
        tripped = false;
        overcurrent_active = false;
        overcurrent_started_at_s = 0.0;
        overcurrent_duration_s = 0.0;
    }

    printf(
        "Reset eseguito: %s - Trip: %s"
        " - Interruttore: %s\n",
        bool_to_string(reset_executed),
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
    );

    printf(
        "\nCOMANDO MANUALE: richiusura con misura INVALID\n"
    );

    close_executed = can_close_breaker(
        tripped,
        MEASUREMENT_QUALITY_INVALID
    );

    if (close_executed)
    {
        breaker_state = BREAKER_STATE_CLOSED;
    }

    printf(
        "Richiusura eseguita: %s"
        " - Trip: %s"
        " - Interruttore: %s\n",
        bool_to_string(close_executed),
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
    );

    printf(
        "\nCOMANDO MANUALE: richiusura sicura\n"
    );

    close_executed = can_close_breaker(
        tripped,
        MEASUREMENT_QUALITY_VALID
    );

    if (close_executed)
    {
        breaker_state = BREAKER_STATE_CLOSED;
    }

    printf(
        "Richiusura eseguita: %s"
        " - Trip: %s"
        " - Interruttore: %s\n",
        bool_to_string(close_executed),
        bool_to_string(tripped),
        breaker_state_to_string(breaker_state)
    );

    return 0;
}