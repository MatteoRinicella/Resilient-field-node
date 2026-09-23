from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality
)
from resilient_field_node.domain.protection import OvercurrentProtection

protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0
)

invalid_measurement = CurrentMeasurement(
    time_s=0.0,
    current_a=50.0,
    quality=MeasurementQuality.INVALID,
)

assert protection.process_current_measurement(
    invalid_measurement,
) is False

assert protection.tripped is False
assert protection.overcurrent_started_at_s is None
assert protection.overcurrent_duration_s == 0.0

stale_measurement = CurrentMeasurement(
    time_s=1.0,
    current_a=50.0,
    quality=MeasurementQuality.STALE,
)

assert protection.process_current_measurement(
    stale_measurement
) is False

assert protection.tripped is False
assert protection.overcurrent_started_at_s is None

first_valid_current = CurrentMeasurement(
    time_s=2.0,
    current_a=12.0,
    quality=MeasurementQuality.VALID,
)

assert protection.process_current_measurement(
    first_valid_current,
) is False

assert protection.overcurrent_started_at_s == 2.0

second_valid_overcurrent = CurrentMeasurement(
    time_s=4.0,
    current_a=12.0,
    quality=MeasurementQuality.VALID,
)

assert protection.process_current_measurement(
    second_valid_overcurrent,
) is True

assert protection.tripped is True

print(" Test quality-aware protection: OK")