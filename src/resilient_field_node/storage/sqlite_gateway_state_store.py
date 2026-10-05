import sqlite3
from contextlib import closing
from datetime import UTC, datetime
from pathlib import Path


class SQLiteGatewayStateStore:
    def __init__(self, database_path: Path) -> None:
        self.database_path = database_path
        self._initialize_database()

    def save(
        self,
        *,
        device_id: str,
        boot_id: str,
        reported_at: str,
        last_event_sequence: int,
        tripped: bool,
        breaker_state: str,
    ) -> None:
        with closing(sqlite3.connect(self.database_path)) as connection:
            connection.execute(
                """
                INSERT INTO current_node_state (
                    device_id,
                    boot_id,
                    reported_at,
                    last_event_sequence,
                    tripped,
                    breaker_state,
                    received_at
                )
                VALUES (?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(device_id) DO UPDATE SET
                    boot_id = excluded.boot_id,
                    reported_at = excluded.reported_at,
                    last_event_sequence = excluded.last_event_sequence,
                    tripped = excluded.tripped,
                    breaker_state = excluded.breaker_state,
                    received_at = excluded.received_at
                """,
                (
                    device_id,
                    boot_id,
                    reported_at,
                    last_event_sequence,
                    int(tripped),
                    breaker_state,
                    datetime.now(UTC).isoformat(),
                ),
            )
            connection.commit()

    def load_latest(self) -> dict[str, object] | None:
        with closing(sqlite3.connect(self.database_path)) as connection:
            connection.row_factory = sqlite3.Row

            row = connection.execute(
                """
                SELECT
                    device_id,
                    boot_id,
                    reported_at,
                    last_event_sequence,
                    tripped,
                    breaker_state,
                    received_at
                FROM current_node_state
                ORDER BY received_at DESC
                LIMIT 1
                """
            ).fetchone()

        if row is None:
            return None

        return {
            "device_id": row["device_id"],
            "boot_id": row["boot_id"],
            "reported_at": row["reported_at"],
            "last_event_sequence": row["last_event_sequence"],
            "tripped": bool(row["tripped"]),
            "breaker_state": row["breaker_state"],
            "received_at": row["received_at"],
        }

    def _initialize_database(self) -> None:
        self.database_path.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        with closing(sqlite3.connect(self.database_path)) as connection:
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS current_node_state (
                    device_id TEXT PRIMARY KEY,
                    boot_id TEXT NOT NULL,
                    reported_at TEXT NOT NULL,
                    last_event_sequence INTEGER NOT NULL,
                    tripped INTEGER NOT NULL
                        CHECK (tripped IN (0, 1)),
                    breaker_state TEXT NOT NULL
                        CHECK (breaker_state IN ('OPEN', 'CLOSED')),
                    received_at TEXT NOT NULL
                )
                """
            )
            connection.commit()
