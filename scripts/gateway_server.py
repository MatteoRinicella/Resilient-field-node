import os
import secrets
import sqlite3
from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify, request

from resilient_field_node.storage.sqlite_gateway_event_store import (
    SQLiteGatewayEventStore,
)

GATEWAY_HOST = "0.0.0.0"
GATEWAY_PORT = 8080
GATEWAY_API_TOKEN = os.getenv("RFN_GATEWAY_API_TOKEN")
GATEWAY_DATABASE_PATH = (
    Path.home()
    / ".resilient-field-node"
    / "gateway-events.sqlite3"
)

REQUIRED_EVENT_FIELDS = (
    "event_id",
    "device_id",
    "boot_id",
    "sequence",
    "event_type",
    "occurred_at",
    "measured_current_ma",
    "threshold_ma",
    "breaker_state",
)

app = Flask(__name__)

gateway_event_store = SQLiteGatewayEventStore(
    GATEWAY_DATABASE_PATH
)

@app.get("/health")
def health():
    return jsonify(
        {
            "status": "ok",
            "service": "resilient-field-node-gateway"
        }
    )

@app.post("/events")
def receive_event():
    if not GATEWAY_API_TOKEN:
        return jsonify(
            {"error": "gateway_token_not_configured"}
        ), 503
    authorization_header = request.headers.get(
        "Authorization",
        ""
    )
    expected_authorization = f"Bearer {GATEWAY_API_TOKEN}"

    if not secrets.compare_digest(
        authorization_header,
        expected_authorization
    ):
        return jsonify({"error": "unauthorized" }), 401

    event_data = request.get_json(silent=True)

    if not isinstance(event_data, dict):
        return jsonify({"error": "invalid_json"}), 400

    missing_fields = [
        field
        for field in REQUIRED_EVENT_FIELDS
        if field not in event_data or event_data[field] is None
    ]

    if missing_fields:
        return jsonify(
            {
                "error": "missing_fields",
                "fields": missing_fields,
            }
        ), 400

    try:
        event_id = str(event_data["event_id"]).strip()
        device_id = str(event_data["device_id"]).strip()
        boot_id = str(event_data["boot_id"]).strip()
        sequence = int(event_data["sequence"])
        event_type = str(event_data["event_type"]).strip()
        occurred_at = datetime.fromisoformat(
            str(event_data["occurred_at"])
        )
        measured_current_ma = float(
            event_data["measured_current_ma"]
        )
        threshold_ma = float(event_data["threshold_ma"])
        breaker_state = str(event_data["breaker_state"]).strip()

        if not event_id or not device_id or not boot_id:
            raise ValueError

        if sequence < 1 or threshold_ma <= 0:
            raise ValueError

        if occurred_at.tzinfo is None:
            raise ValueError

        if event_type != "PROTECTION_TRIP":
            raise ValueError

        if breaker_state != "OPEN":
            raise ValueError

    except (TypeError, ValueError):
        return jsonify({"error": "invalid_event_data"}), 400

    try:
        inserted = gateway_event_store.save(
            event_id=event_id,
            device_id=device_id,
            boot_id=boot_id,
            sequence=sequence,
            event_type=event_type,
            occurred_at=occurred_at.isoformat(),
            measured_current_ma=measured_current_ma,
            threshold_ma=threshold_ma,
            breaker_state=breaker_state,
        )
    except sqlite3.IntegrityError:
        return jsonify(
            {"error": "event_identity_conflict"}
        ), 409

    return jsonify(
        {
            "acknowledged": True,
            "duplicate": not inserted,
            "event_id": event_id,
        }
    )


def main() -> None:
    print(
        f"Gateway Flask in ascolto su "
        f"http://localhost:{GATEWAY_PORT}"
    )

    app.run(
        host=GATEWAY_HOST,
        port=GATEWAY_PORT,
        debug=False,
    )

if __name__ == "__main__":
    main()
