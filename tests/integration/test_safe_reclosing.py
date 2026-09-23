from resilient_field_node.domain.breaker import (
    BreakerState,
    CircuitBreaker,
)
from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)
from resilient_field_node.domain.protection import OvercurrentProtection

protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0,
)

breaker = CircuitBreaker(
    initial_state=BreakerState.CLOSED,
)

first_overcurrent = CurrentMeasurement(
    time_s=0.0,
    current_a=12.0,
    quality=MeasurementQuality.VALID,
)

trip_overcurrent = CurrentMeasurement(
    time_s=2.0,
    current_a=12.0,
    quality=MeasurementQuality.VALID,
)

assert protection.process_current_measurement(
    first_overcurrent,
) is False

trip_occurred = protection.process_current_measurement(
    trip_overcurrent,
)

assert trip_occurred is True

if trip_occurred:
    breaker.open()

assert protection.tripped is True
assert breaker.state == BreakerState.OPEN


close_before_reset = breaker.request_close(
    protection_tripped=protection.tripped,
    measurement_quality=MeasurementQuality.VALID
)

assert close_before_reset is False
assert breaker.state == BreakerState.OPEN

safe_measurement = CurrentMeasurement(
    time_s=3.0,
    current_a=2.0,
    quality=MeasurementQuality.VALID,
)

reset_completed = protection.reset_trip(
    safe_measurement,
)

assert reset_completed is True
assert protection.tripped is False


close_after_reset = breaker.request_close(
    protection_tripped=protection.tripped,
    measurement_quality=safe_measurement.quality,
)

assert close_after_reset is True
assert breaker.state == BreakerState.CLOSED


print("Integration test safe reclosing: OK")