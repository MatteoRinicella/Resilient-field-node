import sqlite3
from contextlib import closing 
from datetime import UTC, datetime
from pathlib import Path
from tempfile import TemporaryDirectory

from resilient_field_node.domain.event import (
    EventType,
    NodeEvent,
)

from resilient_field_node.storage.sqlite_event_store import (
    SQLiteEventStore,
)

with TemporaryDirectory() as temporany_directory:
    database_path = Path(temporany_directory) / "events.db"

    event_store = SQLiteEventStore(database_path)

    assert event_store.database_path == database_path
    assert database_path.exists()

    event = NodeEvent(
        event_id="event-001",
        device_id="node-001",
        boot_id="boot-001",
        sequence=1,
        event_type=EventType.PROTECTION_TRIP,
        occurred_at=datetime(
            2026,
            8,
            19,
            10,
            35,
            0,
            tzinfo=UTC,
        ),

        recorded_at=datetime(
            2026,
            8,
            19,
            10,
            35,
            1,
            tzinfo=UTC,
        )
    )

    event_store.save(event)

    with closing(sqlite3.connect(database_path)) as connection:
        connection.row_factory = sqlite3.Row


        table_row = connection.execute(
            """
            SELECT name
            FROM sqlite_master
            WHERE type = 'table'
            AND name = 'events'
            """
        ).fetchone()

        event_row = connection.execute(
            """
            SELECT
                event_id,
                device_id,
                boot_id,
                sequence,
                event_type,
                occurred_at,
                recorded_at,
                delivery_status,
                acknowledged_at
            FROM events
            WHERE event_id = ?
            """,
            (event.event_id,),
        ).fetchone()

    assert table_row is not None
    assert table_row["name"] == "events"

    assert event_row is not None
    assert event_row["event_id"] == event.event_id
    assert event_row["device_id"] == event.device_id
    assert event_row["boot_id"] == event.boot_id
    assert event_row["sequence"] == event.sequence
    assert event_row["event_type"] == event.event_type.value
    assert event_row["occurred_at"] == event.occurred_at.isoformat()
    assert event_row["recorded_at"] == event.recorded_at.isoformat()
    assert event_row["delivery_status"] == "PENDING"
    assert event_row["acknowledged_at"] is None

    restarted_event_store = SQLiteEventStore(database_path)

    pending_events = restarted_event_store.load_pending()

    assert len(pending_events) == 1

    loaded_event = pending_events[0]

    assert loaded_event is not event
    assert loaded_event == event

    acknowledged_at = datetime(
        2026,
        8,
        19,
        10,
        36,
        0,
        tzinfo=UTC
    )

    first_ack_applied = restarted_event_store.mark_acknowledged(
        event_id= event.event_id,
        acknowledged_at=acknowledged_at,
    )

    assert first_ack_applied is True

    pending_after_ack = restarted_event_store.load_pending()

    assert pending_after_ack == []

    with closing(sqlite3.connect(database_path)) as connection:
        connection.row_factory = sqlite3.Row

        acknowledged_row = connection.execute(
            """
            SELECT
                delivery_status,
                acknowledged_at
            FROM events
            WHERE event_id = ?
            """,
            (event.event_id,),
        ).fetchone()

    assert acknowledged_row is not None
    assert acknowledged_row["delivery_status"] == 'ACKNOWLEDGED'
    assert acknowledged_row["acknowledged_at"] == acknowledged_at.isoformat()

    second_ack_applied = restarted_event_store.mark_acknowledged(
        event_id=event.event_id,
        acknowledged_at=acknowledged_at,
    )

    assert second_ack_applied is False


    

print("Test SQLite event store: OK")