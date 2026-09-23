from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from uuid import uuid4

class EventType(Enum):
    PROTECTION_TRIP = "PROTECTION_TRIP"

@dataclass(frozen=True)
class NodeEvent:
    event_id:str
    device_id: str
    boot_id: str
    sequence: int
    event_type: EventType
    occurred_at: datetime
    recorded_at: datetime

def create_node_event(
    device_id: str,
    boot_id: str,
    sequence: int,
    event_type: EventType,
    occurred_at: datetime,
    recorded_at: datetime,
) -> NodeEvent:
    return NodeEvent(
        event_id=str(uuid4()),
        device_id=device_id,
        boot_id=boot_id,
        sequence=sequence,
        event_type=event_type,
        occurred_at=occurred_at,
        recorded_at=recorded_at,
    )