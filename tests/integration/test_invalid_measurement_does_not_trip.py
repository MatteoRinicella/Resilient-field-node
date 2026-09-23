from pathlib import Path
from tempfile import TemporaryDirectory

from resilient_field_node.domain.breaker import (
    BreakerState,
    CircuitBreaker,
)
from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)
from resilient_field_node.domain.protection import OvercurrentProtection
from resilient_field_node.node import ResilientFieldNode
from resilient_field_node.storage.sqlite_event_store import SQLiteEventStore


with TemporaryDirectory() as temporary_diretory:
    database_path= (
        Path(temporary_diretory)
        / "test-invalid-measurement.db"
    )

    event_store = SQLiteEventStore(
        database_path,
    )

    protection = OvercurrentProtection(
        threshold_a=8.0,
        trip_delay_s=2.0,
    )

    breaker = CircuitBreaker(
        initial_state=BreakerState.CLOSED
    )

    node = ResilientFieldNode(
        device_id="test-node",
        protection=protection,
        breaker=breaker,
        event_store=event_store,
    )

    invalid_measurement = CurrentMeasurement(
        time_s=0.0,
        current_a=50.0,
        quality=MeasurementQuality.INVALID,
    )

    invalid_result = node.process_measurement(
        invalid_measurement
    )

    assert invalid_result is None
    assert protection.tripped is False
    assert breaker.state == BreakerState.CLOSED

    stale_measurement = CurrentMeasurement(
        time_s=10.0,
        current_a=50.0,
        quality=MeasurementQuality.STALE,
    )

    stale_result = node.process_measurement(
        stale_measurement,
    )

    assert stale_result is None
    assert protection.tripped is False
    assert breaker.state == BreakerState.CLOSED

    pending_events = event_store.load_pending()

    assert len(pending_events) == 0

print("Integration test invalid measurement: OK")