from datetime import UTC, datetime

from resilient_field_node.domain.event import (
    EventType,
    NodeEvent,
)
from resilient_field_node.gateway import (
    GatewayUnavailableError,
    SimulatedGateway,
)

event = NodeEvent(
    event_id="event-001",
    device_id="node-001",
    boot_id="boot-001",
    sequence=1,
    event_type=EventType.PROTECTION_TRIP,
    occurred_at=datetime(
        2026,
        8,
        20,
        10,
        35,
        0,
        tzinfo=UTC,
    ),
    recorded_at=datetime(
        2026,
        8,
        20,
        10,
        35,
        1,
        tzinfo=UTC,
    ),
)

gateway = SimulatedGateway(online = False)

assert gateway.is_online() is False

offline_error_raised = False

try:
    gateway.receive_events(event)
except GatewayUnavailableError:
    offline_error_raised = True

assert offline_error_raised is True
assert gateway.event_count() == 0

gateway.set_online(True)

assert gateway.is_online() is True

first_ack_event_id = gateway.receive_events(event)
second_ack_event_id = gateway.receive_events(event)

assert first_ack_event_id == event.event_id
assert second_ack_event_id == event.event_id
assert gateway.event_count() == 1
assert gateway.has_event(event.event_id) is True

print("Test gateway offline and deduplication: OK")