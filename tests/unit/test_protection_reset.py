from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)
from resilient_field_node.domain.protection import OvercurrentProtection


protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0
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

assert protection.process_current_measurement(
    trip_overcurrent,
) is True

assert protection.tripped is True

unsafe_current = CurrentMeasurement(
    time_s=3.0,
    current_a=12.0,
    quality=MeasurementQuality.VALID,
)

assert protection.reset_trip(
    unsafe_current,
) is False

assert protection.tripped is True
invalid_measurement = CurrentMeasurement(
    time_s=4.0,
    current_a=2.0,
    quality=MeasurementQuality.INVALID,
)

assert protection.reset_trip(
    invalid_measurement,
) is False

assert protection.tripped is True


safe_measurement = CurrentMeasurement(
    time_s=5.0,
    current_a=2.0,
    quality=MeasurementQuality.VALID,
)

assert protection.reset_trip(
    safe_measurement,
) is True

assert protection.tripped is False
assert protection.overcurrent_started_at_s is None
assert protection.overcurrent_duration_s == 0.0


print("Test protection reset: OK")