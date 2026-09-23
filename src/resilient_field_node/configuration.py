from dataclasses import dataclass
from pathlib import Path
import tomllib

@dataclass(frozen=True)
class ProtectionConfig:
    threshold_a: float
    trip_delay_s: float

def is_protection_config_valid(
        config: ProtectionConfig,
) -> bool:
    return (
        config.threshold_a > 0.0
        and config.trip_delay_s >= 0.0
    )

def load_protection_config(
        config_path: Path
) -> ProtectionConfig:
    with config_path.open("rb") as config_file:
        config_data = tomllib.load(config_file)

    protection_data = config_data["protection"]

    protection_config = ProtectionConfig(
            threshold_a=float(
                protection_data["threshold_a"],
            ),
            trip_delay_s=float(
                protection_data["trip_delay_s"],
            ),
        )

    if not is_protection_config_valid(
        protection_config,
    ):
        raise ValueError(
            "Invalid protection configuration",
        )

    return protection_config