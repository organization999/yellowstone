"""Canonical repository paths used by the Yellowstone build system."""
from pathlib import Path
from typing import Final

ROOT: Final = Path(__file__).resolve().parents[1]
INCLUDE: Final = ROOT / "include"
PHELPS_INCLUDE: Final = ROOT / "vendor" / "phelps" / "include"
TESTS: Final = ROOT / "tests" / "unit"
PYTHON_TESTS: Final = ROOT / "tests" / "python"
