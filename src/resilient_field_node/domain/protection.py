from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)

def is_overcurrent(current_a: float, threshold_a: float) -> bool:
    return current_a > threshold_a

def has_trip_delay_elapsed(elapsed_s: float, trip_delay_s: float) -> bool:
    return elapsed_s >= trip_delay_s

def is_quality_usable_for_protection(
        quality: MeasurementQuality,
) -> bool:
    return quality not in (
        MeasurementQuality.INVALID,
        MeasurementQuality.STALE,
    )

class OvercurrentProtection:
    def __init__(self, threshold_a: float, trip_delay_s: float) -> None:
        self.threshold_a = threshold_a
        self.trip_delay_s = trip_delay_s
        self.overcurrent_started_at_s: float | None = None
        self.overcurrent_duration_s: float = 0.0
        self.tripped: bool = False

    def detects_overcurrent(self, current_a: float) -> bool:
        return is_overcurrent(current_a, self.threshold_a)

    def update_overcurrent_duration(
        self,
        current_time_s: float,
        current_a: float
    ) -> float:
        if not self.detects_overcurrent(current_a):
            self.overcurrent_started_at_s = None
            self.overcurrent_duration_s = 0.0
            return self.overcurrent_duration_s

        if self.overcurrent_started_at_s is None:
            self.overcurrent_started_at_s = current_time_s

        self.overcurrent_duration_s = (
            current_time_s - self.overcurrent_started_at_s
        )
        return self.overcurrent_duration_s

    def process_measurement(
        self,
        current_time_s: float,
        current_a: float,
    ) -> bool:
        overcurrent = self.detects_overcurrent(current_a)
        elapsed_s = self.update_overcurrent_duration(
            current_time_s,
            current_a,
        )

        if self.tripped:
            return False

        if not overcurrent:
            return False

        if not has_trip_delay_elapsed(elapsed_s, self.trip_delay_s):
            return False

        self.tripped = True
        return True

    def process_current_measurement(
            self,
            measurement: CurrentMeasurement,
    ) -> bool:
        if not is_quality_usable_for_protection(
            measurement.quality,
        ):
            self.overcurrent_started_at_s = None
            self.overcurrent_duration_s = 0.0
            return False

        return self.process_measurement(
            current_time_s=measurement.time_s,
            current_a=measurement.current_a,
        )

    def reset_trip(
            self,
            measurement: CurrentMeasurement,
    ) -> bool:
        if not self.tripped:
                return False

        if measurement.quality != MeasurementQuality.VALID:
            return False

        if self.detects_overcurrent(measurement.current_a):
            return False

        self.tripped = False
        self.overcurrent_started_at_s = None
        self.overcurrent_duration_s = 0.0

        return True