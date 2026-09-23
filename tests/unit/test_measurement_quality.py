from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality
)
from resilient_field_node.domain.protection import (
    is_quality_usable_for_protection,
)

assert MeasurementQuality.VALID.value == "VALID"
assert MeasurementQuality.SUSPECT.value == "SUSPECT"
assert MeasurementQuality.INVALID.value == "INVALID"
assert MeasurementQuality.STALE.value == "STALE"

assert len(MeasurementQuality) == 4

assert is_quality_usable_for_protection(
    MeasurementQuality.VALID,
) is True

assert is_quality_usable_for_protection(
    MeasurementQuality.SUSPECT,
) is True

assert is_quality_usable_for_protection(
    MeasurementQuality.INVALID,
) is False

assert is_quality_usable_for_protection(
    MeasurementQuality.STALE,
) is False

print("Test measurement quality: OK")