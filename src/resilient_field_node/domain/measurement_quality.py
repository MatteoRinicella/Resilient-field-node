from enum import Enum

class MeasurementQuality(Enum):
    VALID = "VALID"
    SUSPECT = "SUSPECT"
    INVALID = "INVALID"
    STALE = "STALE"

    