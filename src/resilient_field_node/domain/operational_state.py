from enum import Enum 


class OperationalState(Enum):
    STARTING = "STARTING"
    NORMAL = "NORMAL"
    OFFLINE = "OFFLINE"
    SYNCING = "SYNCING"

class OperationalStateMachine:
    def __init__(self) -> None:
        self._state = OperationalState.STARTING

    @property
    def state(self) -> OperationalState:
        return self._state

    def complete_startup(self, gateway_online: bool) -> None:
        if gateway_online:
            self._state = OperationalState.NORMAL
        else:
            self._state = OperationalState.OFFLINE

    def gateway_connection_lost(self) -> None:
        self._state = OperationalState.OFFLINE

    def gateway_connection_restored(self) -> None:
        self._state = OperationalState.SYNCING

    def synchronization_completed(self) -> None:
        self._state = OperationalState.NORMAL