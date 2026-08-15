"""Native extension definitions for Yellowstone."""
from pybind11.setup_helpers import Pybind11Extension


def extensions() -> list[Pybind11Extension]:
    return [
        Pybind11Extension(
            "yellowstone._yellowstone",
            [
                "source/bindings/python.cc",
                "vendor/phelps/source/manager.cc",
            ],
            include_dirs=["include", "vendor/phelps/include"],
            cxx_std=None,
        )
    ]
