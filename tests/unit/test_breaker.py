from resilient_field_node.domain.breaker import(
    BreakerState,
    CircuitBreaker
)

from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality
)

breaker = CircuitBreaker( initial_state=BreakerState.CLOSED)

assert breaker.state is BreakerState.CLOSED

breaker.open()

assert breaker.state is BreakerState.OPEN

breaker.open()

assert breaker.state is BreakerState.OPEN

close_while_tripped = breaker.request_close(
    protection_tripped=True,
    measurement_quality=MeasurementQuality.VALID,
)

assert close_while_tripped is False
assert breaker.state == BreakerState.OPEN

close_whith_invalid_measurement = breaker.request_close(
    protection_tripped=False,
    measurement_quality=MeasurementQuality.INVALID,
)

assert close_whith_invalid_measurement is False
assert breaker.state == BreakerState.OPEN

close_with_stale_measurement = breaker.request_close(
    protection_tripped=False,
    measurement_quality=MeasurementQuality.STALE,
)

assert close_with_stale_measurement is False
assert breaker.state == BreakerState.OPEN


safe_close = breaker.request_close(
    protection_tripped=False,
    measurement_quality=MeasurementQuality.VALID,
)

assert safe_close is True
assert breaker.state == BreakerState.CLOSED

print("Test breaker: OK")