from datetime import UTC, datetime
from uuid import uuid4

from resilient_field_node.domain.breaker import CircuitBreaker
from resilient_field_node.domain.event import (
    EventType,
    NodeEvent,
    create_node_event,
)

from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.protection import OvercurrentProtection
from resilient_field_node.storage.sqlite_event_store import SQLiteEventStore

class ResilientFieldNode:
    def __init__(
        self,
        device_id: str,
        protection: OvercurrentProtection,
        breaker: CircuitBreaker,
        event_store : SQLiteEventStore,
    ) -> None:
        self.device_id = device_id
        self.boot_id = str(uuid4())
        self.sequence = 0

        self.protection = protection
        self.breaker = breaker
        self.event_store = event_store

    def process_measurement(
            self,
            measurement: CurrentMeasurement,
    ) -> NodeEvent | None:
        trip_occurred = self.protection.process_current_measurement(
            measurement,
        )

        if not trip_occurred:
            return None

        self.breaker.open()
        self.sequence += 1

        occured_at = datetime.now(UTC)

        trip_event = create_node_event(
            device_id=self.device_id,
            boot_id=self.boot_id,
            sequence=self.sequence,
            event_type=EventType.PROTECTION_TRIP,
            occurred_at=occured_at,
            recorded_at=datetime.now(UTC),
        )

        self.event_store.save(
            trip_event,
        )

        return trip_event