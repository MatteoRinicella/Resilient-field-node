from datetime import UTC, datetime

from resilient_field_node.gateway import (
    GatewayUnavailableError,
    SimulatedGateway
)
from resilient_field_node.storage.sqlite_event_store import ( 
    SQLiteEventStore
)


def synchronize_pending_events(
        event_store: SQLiteEventStore,
        gateway: SimulatedGateway
) -> int:
    pending_events = event_store.load_pending()
    acknowledged_count = 0

    for event in pending_events:
        try:
            acknowledged_event_id = gateway.receive_events(event)
        except GatewayUnavailableError:
            break

        acknowledged_applied = event_store.mark_acknowledged(
            event_id = acknowledged_event_id,
            acknowledged_at=datetime.now(UTC),
        )

        if acknowledged_applied:
            acknowledged_count += 1

    return acknowledged_count