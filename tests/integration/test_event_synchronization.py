from datetime import UTC, datetime 
from pathlib import Path 
from tempfile import TemporaryDirectory

from resilient_field_node.domain.event import (
    EventType,
    create_node_event
)
from resilient_field_node.gateway import SimulatedGateway
from resilient_field_node.storage.sqlite_event_store import (
    SQLiteEventStore,
)
from resilient_field_node.synchronization import (
    synchronize_pending_events,
)

with TemporaryDirectory() as temporary_directory:
    database_path = Path(temporary_directory) / "events.db"

    event_store = SQLiteEventStore(database_path)

    first_event = create_node_event(
        device_id= "node-001",
        boot_id="boot-001",
        sequence=1,
        event_type=EventType.PROTECTION_TRIP,
        occurred_at=datetime(
            2026,
            8,
            21,
            10,
            35,
            0,
            tzinfo=UTC,
        ),
        recorded_at=datetime(
            2026,
            8,
            21,
            10,
            35,
            1,
            tzinfo=UTC,
        ),
    )

    second_event = create_node_event(
        device_id="node-001",
         boot_id="boot-001",
        sequence=2,
        event_type=EventType.PROTECTION_TRIP,
        occurred_at=datetime(
            2026,
            8,
            21,
            10,
            36,
            0,
            tzinfo=UTC,
        ),
        recorded_at=datetime(
            2026,
            8,
            21,
            10,
            36,
            1,
            tzinfo=UTC,
        ),
    )

    event_store.save(first_event)
    event_store.save(second_event)

    gateway = SimulatedGateway(online=False)

    offline_acknowledged_count = synchronize_pending_events(
        event_store,
        gateway,
    )

    assert offline_acknowledged_count == 0
    assert len(event_store.load_pending()) == 2
    assert gateway.event_count() == 0

    gateway.set_online(True)

    online_acknowledged_count = synchronize_pending_events(
        event_store,
        gateway,
    )

    assert online_acknowledged_count ==2
    assert event_store.load_pending() == []
    assert gateway.event_count() == 2
    assert gateway.has_event(first_event.event_id) is True
    assert gateway.has_event(second_event.event_id) is True

    repetead_sys_count = synchronize_pending_events(
        event_store,
        gateway,
    )

    assert repetead_sys_count == 0
    assert gateway.event_count() == 2

print("Integration test event synchronization: OK")