"""Assemble setuptools metadata for Yellowstone."""
from __future__ import annotations

import os
from typing import Any
from setuptools import find_packages

from .commands import command_classes
from .extensions import extensions
from .paths import ROOT


def configuration() -> dict[str, Any]:
    readme = ROOT / "README.md"
    return {
        "name": "yellowstone",
        "version": os.environ.get("YELLOWSTONE_VERSION", "0.1.0"),
        "description": "Generic standalone event bus backed by Phelps workers.",
        "long_description": readme.read_text(encoding="utf-8") if readme.exists() else "",
        "long_description_content_type": "text/markdown",
        "license": "Apache-2.0",
        "packages": find_packages(include=["yellowstone", "yellowstone.*"]),
        "ext_modules": extensions(),
        "package_data": {"yellowstone": ["py.typed", "*.pyi", "typing/*.pyi", "typing/stubs/*.pyi"]},
        "include_package_data": True,
        "zip_safe": False,
        "cmdclass": command_classes(),
    }
