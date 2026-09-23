from pathlib import Path

from resilient_field_node.configuration import load_protection_config
from resilient_field_node.domain.breaker import (
    BreakerState,
    CircuitBreaker,
)
from resilient_field_node.domain.measurement import CurrentMeasurement
from resilient_field_node.domain.measurement_quality import (
    MeasurementQuality,
)
from resilient_field_node.domain.operational_state import (
    OperationalStateMachine,
)
from resilient_field_node.domain.protection import OvercurrentProtection
from resilient_field_node.gateway import SimulatedGateway
from resilient_field_node.node import ResilientFieldNode
from resilient_field_node.storage.sqlite_event_store import (
    SQLiteEventStore,
)
from resilient_field_node.synchronization import (
    synchronize_pending_events,
)


current_measurements = [
    CurrentMeasurement(
        time_s=0.0,
        current_a=2.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=1.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=2.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=3.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=4.0,
        current_a=12.0,
        quality=MeasurementQuality.VALID,
    ),
    CurrentMeasurement(
        time_s=5.0,
        current_a=2.0,
        quality=MeasurementQuality.VALID,
    ),
]


project_directory = Path(__file__).resolve().parents[1]
config_path = project_directory / "config" / "node.toml"
database_path = project_directory / "data" / "node-events.db"

protection_config = load_protection_config(
    config_path,
)

event_store = SQLiteEventStore(
    database_path,
)

gateway = SimulatedGateway(
    online=False,
)

state_machine = OperationalStateMachine()

protection = OvercurrentProtection(
    threshold_a=protection_config.threshold_a,
    trip_delay_s=protection_config.trip_delay_s,
)

breaker = CircuitBreaker(
    initial_state=BreakerState.CLOSED,
)

node = ResilientFieldNode(
    device_id="node-001",
    protection=protection,
    breaker=breaker,
    event_store=event_store,
)


print(
    "Configurazione protezione:",
    "soglia", protection_config.threshold_a,
    "A - ritardo", protection_config.trip_delay_s,
    "s",
)

state_machine.complete_startup(
    gateway_online=gateway.is_online(),
)

print(
    "Stato operativo del nodo:",
    state_machine.state.value,
)


for measurement in current_measurements:
    trip_event = node.process_measurement(
        measurement,
    )

    if trip_event is not None:
        print(
            "EVENTO:",
            trip_event.event_type.value,
            "- ID:",
            trip_event.event_id,
        )

    print(
        "Tempo:", measurement.time_s,
        "s - Corrente:", measurement.current_a,
        "A - Qualità:", measurement.quality.value,
        "- Durata:", protection.overcurrent_duration_s,
        "s - Trip:", protection.tripped,
        "- Stato interruttore:", breaker.state.value,
    )


offline_synchronized_count = synchronize_pending_events(
    event_store,
    gateway,
)

pending_while_offline = event_store.load_pending()

print(
    "Sincronizzati mentre offline:",
    offline_synchronized_count,
)
print(
    "Eventi pending persistenti:",
    len(pending_while_offline),
)


gateway.set_online(True)
state_machine.gateway_connection_restored()

print(
    "Stato operativo del nodo:",
    state_machine.state.value,
)


online_synchronized_count = synchronize_pending_events(
    event_store,
    gateway,
)

state_machine.synchronization_completed()

pending_after_reconnection = event_store.load_pending()

print(
    "Sincronizzati dopo la riconnessione:",
    online_synchronized_count,
)
print(
    "Eventi ancora pending:",
    len(pending_after_reconnection),
)
print(
    "Eventi presenti nel gateway:",
    gateway.event_count(),
)
print(
    "Stato operativo del nodo:",
    state_machine.state.value,
)


latest_measurement = current_measurements[-1]

print("COMANDO MANUALE: reset della protezione")

reset_completed = protection.reset_trip(
    latest_measurement,
)

print(
    "Reset eseguito:",
    reset_completed,
)

print("COMANDO MANUALE: richiesta di richiusura")

close_completed = breaker.request_close(
    protection_tripped=protection.tripped,
    measurement_quality=latest_measurement.quality,
)

print(
    "Richiusura eseguita:",
    close_completed,
)
print(
    "Trip attivo:",
    protection.tripped,
)
print(
    "Stato finale interruttore:",
    breaker.state.value,
)