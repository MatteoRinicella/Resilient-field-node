from resilient_field_node.domain.protection import (
    OvercurrentProtection,
    has_trip_delay_elapsed,
    is_overcurrent
)

assert is_overcurrent(7.0, 8.0) is False
assert is_overcurrent(8.0, 8.0) is False
assert is_overcurrent(9.0, 8.0) is True

assert has_trip_delay_elapsed(1.9, 2.0) is False
assert has_trip_delay_elapsed(2.0, 2.0) is True
assert has_trip_delay_elapsed(2.1, 2.0) is True

protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0
)

assert protection.threshold_a == 8.0
assert protection.trip_delay_s == 2.0

assert protection.overcurrent_started_at_s is None
assert protection.overcurrent_duration_s == 0.0
assert protection.tripped is False

assert protection.detects_overcurrent(7.0) is False
assert protection.detects_overcurrent(8.0) is False
assert protection.detects_overcurrent(9.0) is True

timer_protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0
)

assert timer_protection.update_overcurrent_duration(0.0, 2.0) == 0.0
assert timer_protection.overcurrent_started_at_s is None

assert timer_protection.update_overcurrent_duration(1.0, 12.0) == 0.0
assert timer_protection.overcurrent_started_at_s == 1.0

assert timer_protection.update_overcurrent_duration(2.0, 12.0) == 1.0
assert timer_protection.overcurrent_duration_s == 1.0
assert timer_protection.update_overcurrent_duration(3.0, 12.0) == 2.0

assert timer_protection.update_overcurrent_duration(4.0, 2.0) == 0.0
assert timer_protection.overcurrent_started_at_s is None
assert timer_protection.overcurrent_duration_s == 0.0

trip_protection = OvercurrentProtection(
    threshold_a=8.0,
    trip_delay_s=2.0,
)

assert trip_protection.process_measurement(0.0, 2.0) is False
assert trip_protection.tripped is False

assert trip_protection.process_measurement(1.0, 12.0) is False
assert trip_protection.process_measurement(2.0, 12.0) is False

assert trip_protection.process_measurement(3.0, 12.0) is True
assert trip_protection.tripped is True

print("Test protection: OK")