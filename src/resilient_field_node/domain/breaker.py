from enum import Enum

from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality
)

class BreakerState(Enum):
    OPEN = "OPEN"
    CLOSED = "CLOSED"

class CircuitBreaker:
    def __init__(self, initial_state: BreakerState) -> None:
        self.state = initial_state

    def open(self) -> None:
        self.state= BreakerState.OPEN

    def request_close(
            self,
            protection_tripped: bool,
            measurement_quality: MeasurementQuality,
    ) -> bool:
        if protection_tripped:
            return False

        if measurement_quality != MeasurementQuality.VALID:
            return False

        self.state = BreakerState.CLOSED
        return True