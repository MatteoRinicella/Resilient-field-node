import sqlite3
from contextlib import closing
from datetime import datetime
from pathlib import Path

from resilient_field_node.domain.event import (
    NodeEvent,
    EventType,
)

class SQLiteEventStore:
    def __init__(self, database_path: Path) -> None:
        self.database_path = database_path
        self._initialize_database()

    def save(self, event: NodeEvent) -> None:
        with closing(sqlite3.connect(self.database_path)) as connection:
            connection.execute(
                """

                INSERT INTO events (
                event_id,
                device_id,
                boot_id,
                sequence,
                event_type,
                occurred_at,
                recorded_at
                )
                VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    event.event_id,
                    event.device_id,
                    event.boot_id,
                    event.sequence,
                    event.event_type.value,
                    event.occurred_at.isoformat(),
                    event.recorded_at.isoformat(),
                ),
            )
            connection.commit()

    def load_pending(self) -> list[NodeEvent]:
        with closing(sqlite3.connect(self.database_path)) as connection:
            connection.row_factory = sqlite3.Row

            rows = connection.execute(
                """
                SELECT
                    event_id,
                    device_id,
                    boot_id, 
                    sequence,
                    event_type,
                    occurred_at,
                    recorded_at
                FROM events
                WHERE delivery_status = 'PENDING'
                ORDER by recorded_at ASC, sequence ASC
                """
            ).fetchall()

        pending_events: list[NodeEvent] = []

        for row in rows:
            event = NodeEvent(
                event_id=row["event_id"],
                device_id=row["device_id"],
                boot_id=row["boot_id"],
                sequence=row["sequence"],
                event_type=EventType(row["event_type"]),
                occurred_at=datetime.fromisoformat(row["occurred_at"]),
                recorded_at=datetime.fromisoformat(row["recorded_at"]),
            )

            pending_events.append(event)

        return pending_events

    def mark_acknowledged(
            self,
            event_id: str,
            acknowledged_at: datetime,
    ) -> bool:
        with closing(sqlite3.connect(self.database_path)) as connection:
            cursor = connection.execute(
                """
                UPDATE events
                SET
                    delivery_status = 'ACKNOWLEDGED',
                    acknowledged_at = ?
                WHERE event_id = ?
                AND delivery_status = 'PENDING'
                """,
                (
                    acknowledged_at.isoformat(),
                    event_id,
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

                CREATE TABLE IF NOT EXISTS events (
                    event_id TEXT PRIMARY KEY,
                    device_id TEXT NOT NULL,
                    boot_id TEXT NOT NULL,
                    sequence INTEGER NOT NULL,
                    event_type TEXT NOT NULL,
                    occurred_at TEXT NOT NULL,
                    recorded_at TEXT NOT NULL,
                    delivery_status TEXT NOT NULL DEFAULT 'PENDING',
                    acknowledged_at TEXT,
                    UNIQUE (device_id, boot_id, sequence)
                )
                """
            )
            connection.commit()