from resilient_field_node.domain.event import NodeEvent

class GatewayUnavailableError(ConnectionError):
    pass



class SimulatedGateway:
    def __init__(self, online: bool = True) -> None:
        self._online = online
        self._events_by_id: dict[str, NodeEvent] = {}

    def is_online(self) -> bool:
        return self._online

    def set_online(self, online: bool) -> None:
        self._online = online

    def receive_events(self, event: NodeEvent) -> str:
        if not self._online:
            raise GatewayUnavailableError("Gateway is offline")

        if event.event_id not in self._events_by_id:
            self._events_by_id[event.event_id] = event

        return event.event_id

    def event_count(self) -> int:
        return len(self._events_by_id)

    def has_event(self, event_id: str) -> bool:
        return event_id in self._events_by_id
