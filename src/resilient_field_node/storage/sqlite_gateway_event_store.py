import sqlite3
from contextlib import closing
from datetime import UTC, datetime
from pathlib import Path


class SQLiteGatewayEventStore:
    def __init__(self, database_path: Path) -> None:
        self.database_path = database_path
        self._initialize_database()

    def save(
        self,
        *,
        event_id: str,
        device_id: str,
        boot_id: str,
        sequence: int,
        event_type: str,
        occurred_at: str,
        measured_current_ma: float,
        threshold_ma: float,
        breaker_state: str,
    ) -> bool:
        with closing(sqlite3.connect(self.database_path)) as connection:
            cursor = connection.execute(
                """
                INSERT INTO received_events (
                    event_id,
                    device_id,
                    boot_id,
                    sequence,
                    event_type,
                    occurred_at,
                    measured_current_ma,
                    threshold_ma,
                    breaker_state,
                    received_at
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(event_id) DO NOTHING
                """,
                (
                    event_id,
                    device_id,
                    boot_id,
                    sequence,
                    event_type,
                    occurred_at,
                    measured_current_ma,
                    threshold_ma,
                    breaker_state,
                    datetime.now(UTC).isoformat(),
                ),
            )
            connection.commit()

        return cursor.rowcount == 1

    def _initialize_database(self) -> None:
        self.database_path.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        with closing(sqlite3.connect(self.database_path)) as connection:
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS received_events (
                    event_id TEXT PRIMARY KEY,
                    device_id TEXT NOT NULL,
                    boot_id TEXT NOT NULL,
                    sequence INTEGER NOT NULL,
                    event_type TEXT NOT NULL,
                    occurred_at TEXT NOT NULL,
                    measured_current_ma REAL NOT NULL,
                    threshold_ma REAL NOT NULL,
                    breaker_state TEXT NOT NULL,
                    received_at TEXT NOT NULL,
                    UNIQUE (device_id, boot_id, sequence)
                )
                """
            )
            connection.commit()
