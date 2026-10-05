from pathlib import Path
from tempfile import TemporaryDirectory

from resilient_field_node.storage.sqlite_gateway_event_store import (
    SQLiteGatewayEventStore,
)


with TemporaryDirectory() as temporary_directory:
    database_path = (
        Path(temporary_directory)
        / "gateway.sqlite3"
    )

    store = SQLiteGatewayEventStore(database_path)

    assert store.load_recent() == []

    first_inserted = store.save(
        event_id="event-1",
        device_id="test-device",
        boot_id="test-boot",
        sequence=1,
        event_type="PROTECTION_TRIP",
        occurred_at="2026-10-05T11:00:00+02:00",
        measured_current_ma=21.5,
        threshold_ma=17.0,
        breaker_state="OPEN",
    )

    second_inserted = store.save(
        event_id="event-2",
        device_id="test-device",
        boot_id="test-boot",
        sequence=2,
        event_type="PROTECTION_TRIP",
        occurred_at="2026-10-05T11:05:00+02:00",
        measured_current_ma=23.4,
        threshold_ma=17.0,
        breaker_state="OPEN",
    )

    assert first_inserted is True
    assert second_inserted is True

    events = store.load_recent(limit=1)

    assert len(events) == 1
    assert events[0]["event_id"] == "event-2"
    assert events[0]["device_id"] == "test-device"
    assert events[0]["sequence"] == 2
    assert events[0]["event_type"] == "PROTECTION_TRIP"
    assert events[0]["occurred_at"] == (
        "2026-10-05T11:05:00+02:00"
    )
    assert events[0]["measured_current_ma"] == 23.4
    assert events[0]["threshold_ma"] == 17.0
    assert events[0]["breaker_state"] == "OPEN"
    assert isinstance(events[0]["received_at"], str)

    try:
        store.load_recent(limit=0)
    except ValueError:
        pass
    else:
        raise AssertionError(
            "Un limite uguale a zero deve essere rifiutato"
        )


print("Test SQLite gateway event store: OK")