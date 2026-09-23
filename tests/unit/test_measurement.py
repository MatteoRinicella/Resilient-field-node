from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality
)

measurement = CurrentMeasurement(
    time_s=3.0,
    current_a=12.0,
    quality=MeasurementQuality.VALID,
)

assert measurement.time_s == 3.0
assert measurement.current_a == 12.0
assert measurement.quality == MeasurementQuality.VALID

print("Test measurement: OK")