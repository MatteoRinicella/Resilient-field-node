from pathlib import Path
from tempfile import TemporaryDirectory

from resilient_field_node.domain.breaker import (
    BreakerState,
    CircuitBreaker,
)
from resilient_field_node.domain.event import EventType
from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)
from resilient_field_node.domain.protection import OvercurrentProtection
from resilient_field_node.node import ResilientFieldNode
from resilient_field_node.storage.sqlite_event_store import SQLiteEventStore

with TemporaryDirectory() as temporary_directory:
    database_path = (
        Path(temporary_directory)
        / "test-node-events.db"
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

    first_overcurrent = CurrentMeasurement(
        time_s=0.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    )

    first_result = node.process_measurement(
        first_overcurrent,
    )

    assert first_result is None
    assert breaker.state == BreakerState.CLOSED

    trip_ovrecurrent = CurrentMeasurement(
        time_s=2.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    )

    trip_event = node.process_measurement(
        trip_ovrecurrent,
    )

    assert trip_event is not None
    assert trip_event.event_type == EventType.PROTECTION_TRIP
    assert trip_event.device_id == "test-node"
    assert trip_event.sequence == 1

    assert protection.tripped is True
    assert breaker.state == BreakerState.OPEN

    pending_events = event_store.load_pending()

    assert len(pending_events) == 1
    assert pending_events[0].event_id == trip_event.event_id


print("Integration test node trip: OK")