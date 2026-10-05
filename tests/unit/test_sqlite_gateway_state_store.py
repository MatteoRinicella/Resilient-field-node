from pathlib import Path
from tempfile import TemporaryDirectory

from resilient_field_node.storage.sqlite_gateway_state_store import (
    SQLiteGatewayStateStore,
)


with TemporaryDirectory() as temporary_directory:
    database_path = (
        Path(temporary_directory)
        / "gateway.sqlite3"
    )

    store = SQLiteGatewayStateStore(database_path)

    assert store.load_latest() is None

    store.save(
        device_id="test-device",
        boot_id="test-boot",
        reported_at="2026-10-05T11:50:34+02:00",
        last_event_sequence=11,
        tripped=False,
        breaker_state="CLOSED",
    )

    state = store.load_latest()

    assert state is not None
    assert state["device_id"] == "test-device"
    assert state["boot_id"] == "test-boot"
    assert state["reported_at"] == "2026-10-05T11:50:34+02:00"
    assert state["last_event_sequence"] == 11
    assert state["tripped"] is False
    assert state["breaker_state"] == "CLOSED"
    assert isinstance(state["received_at"], str)


print("Test SQLite gateway state store: OK")