from resilient_field_node.domain.breaker import (
    BreakerState,
    CircuitBreaker,
)
from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)
from resilient_field_node.domain.protection import OvercurrentProtection


current_measurements = [
    CurrentMeasurement(
        time_s=0.0,
        current_a=2.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=1.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=2.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=3.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=4.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=5.0,
        current_a=2.0,
        quality=MeasurementQuality.VALID,
    ),
]


protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0,
)

breaker = CircuitBreaker(
    initial_state=BreakerState.CLOSED,
)

trip_event_count = 0


for measurement in current_measurements:
    trip_occurred = protection.process_current_measurement(
        measurement,
    )

    if trip_occurred:
        breaker.open()
        trip_event_count += 1


assert protection.tripped is True
assert breaker.state == BreakerState.OPEN
assert trip_event_count == 1


print("Integration test trip -> breaker: OK")