from dataclasses import dataclass

from resilient_field_node.domain.measurement_quality import(
    MeasurementQuality
)

@dataclass(frozen=True)
class CurrentMeasurement:
    time_s: float
    current_a: float
    quality: MeasurementQuality