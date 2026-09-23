from pathlib import Path

from resilient_field_node.configuration import (
    ProtectionConfig,
    is_protection_config_valid,
    load_protection_config,
)

project_directory = Path(__file__).resolve().parents[2]
config_path = project_directory / "config" / "node.toml"

protection_config = load_protection_config(
    config_path
)

assert isinstance(
    protection_config,
    ProtectionConfig,
)

assert protection_config.threshold_a == 8.0
assert protection_config.trip_delay_s == 2.0

assert is_protection_config_valid(
    protection_config,
)

negative_delay_config = ProtectionConfig(
    threshold_a=8.0,
    trip_delay_s=-1.0,
)

assert is_protection_config_valid(
    negative_delay_config,
) is False

print("Test configuration: OK")

