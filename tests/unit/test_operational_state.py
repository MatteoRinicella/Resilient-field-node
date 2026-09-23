from resilient_field_node.domain.operational_state import (
    OperationalState,
    OperationalStateMachine,
)


state_machine = OperationalStateMachine()

assert state_machine.state == OperationalState.STARTING

state_machine.complete_startup(
    gateway_online=True,
)

assert state_machine.state == OperationalState.NORMAL

state_machine.gateway_connection_lost()

assert state_machine.state == OperationalState.OFFLINE

state_machine.gateway_connection_restored()

assert state_machine.state == OperationalState.SYNCING

state_machine.synchronization_completed()

assert state_machine.state == OperationalState.NORMAL

offline_at_startup = OperationalStateMachine()

offline_at_startup.complete_startup(
    gateway_online=False,
)

assert offline_at_startup.state == OperationalState.OFFLINE

print("Test operational state: OK")