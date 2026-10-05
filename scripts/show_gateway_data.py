import argparse
import time
from datetime import datetime
from pathlib import Path

from resilient_field_node.storage.sqlite_gateway_event_store import (
    SQLiteGatewayEventStore,
)
from resilient_field_node.storage.sqlite_gateway_state_store import (
    SQLiteGatewayStateStore,
)


GATEWAY_DATABASE_PATH = (
    Path.home()
    / ".resilient-field-node"
    / "gateway-events.sqlite3"
)


def format_timestamp(timestamp: object) -> str:
    timestamp_text = str(timestamp)

    try:
        parsed_timestamp = datetime.fromisoformat(
            timestamp_text
        )
    except ValueError:
        return timestamp_text

    return parsed_timestamp.strftime(
        "%d/%m/%Y %H:%M:%S"
    )


def show_gateway_data() -> None:
    state_store = SQLiteGatewayStateStore(
        GATEWAY_DATABASE_PATH
    )
    event_store = SQLiteGatewayEventStore(
        GATEWAY_DATABASE_PATH
    )

    current_state = state_store.load_latest()
    recent_events = event_store.load_recent(limit=10)

    print()
    print("RESILIENT FIELD NODE")
    print("=" * 50)
    print()

    print("STATO CORRENTE")
    print("-" * 50)

    if current_state is None:
        print("Nessuno stato ricevuto dal nodo.")
    else:
        protection_status = (
            "TRIP ATTIVO"
            if current_state["tripped"]
            else "NORMALE"
        )

        print(
            f'Dispositivo:       '
            f'{current_state["device_id"]}'
        )
        print(
            f'Protezione:        {protection_status}'
        )
        print(
            f'Interruttore:      '
            f'{current_state["breaker_state"]}'
        )
        print(
            f'Ultima sequenza:   '
            f'{current_state["last_event_sequence"]}'
        )
        print(
            f'Ultimo stato:      '
            f'{format_timestamp(current_state["reported_at"])}'
        )

    print()
    print("ULTIMI EVENTI")
    print("-" * 50)

    if not recent_events:
        print("Nessun evento di protezione registrato.")
    else:
        for event in recent_events:
            print(
                f'#{event["sequence"]} | '
                f'{format_timestamp(event["occurred_at"])} | '
                f'{float(event["measured_current_ma"]):.1f} mA | '
                f'soglia '
                f'{float(event["threshold_ma"]):.1f} mA | '
                f'{event["breaker_state"]}'
            )


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Mostra lo stato e gli eventi del "
            "Resilient Field Node"
        )
    )
    parser.add_argument(
        "--watch",
        action="store_true",
        help="aggiorna automaticamente i dati ogni 5 secondi",
    )

    arguments = parser.parse_args()

    try:
        while True:
            if arguments.watch:
                print(
                    "\033[2J\033[H",
                    end="",
                )

            show_gateway_data()

            if not arguments.watch:
                break

            time.sleep(5)

    except KeyboardInterrupt:
        print()
        print("Visualizzazione terminata.")


if __name__ == "__main__":
    main()
