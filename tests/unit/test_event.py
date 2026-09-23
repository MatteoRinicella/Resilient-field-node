from datetime import UTC, datetime
from uuid import UUID

from resilient_field_node.domain.event import (
    EventType,
    NodeEvent,
    create_node_event
)


occurred_at = datetime(
   2026,
    8,
    19,
    10,
    35,
    0,
    tzinfo=UTC,  
)

recorded_at = datetime(
    2026,
    8,
    19,
    10,
    35,
    1,
    tzinfo=UTC,
)

event = NodeEvent(
    event_id="event-001",
    device_id="node-001",
    boot_id="boot-001",
    sequence=1,
    event_type=EventType.PROTECTION_TRIP,
    occurred_at=occurred_at,
    recorded_at=recorded_at,
)

assert event.event_id == "event-001"
assert event.device_id == "node-001"
assert event.boot_id == "boot-001"
assert event.sequence == 1
assert event.event_type is EventType.PROTECTION_TRIP
assert event.occurred_at == occurred_at
assert event.recorded_at == recorded_at
assert event.recorded_at >= occurred_at

generated_event = create_node_event(
    device_id="node-001",
    boot_id="boot-001",
    sequence=2,
    event_type=EventType.PROTECTION_TRIP,
    occurred_at=occurred_at,
    recorded_at=recorded_at,
)

second_generated_event = create_node_event(
    device_id="node-001",
    boot_id="boot-001",
    sequence=3,
    event_type=EventType.PROTECTION_TRIP,
    occurred_at=occurred_at,
    recorded_at=recorded_at,
)

assert UUID(generated_event.event_id).version == 4
assert generated_event.event_id != second_generated_event.event_id
assert generated_event.sequence == 2
assert second_generated_event.sequence == 3

print("Test event: OK")
