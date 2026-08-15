"""Compiler-family policy for Yellowstone's C++23 native extension."""
from typing import Any


def upgrade_msvc_warning_level(compiler: Any) -> None:
    for attribute in ("compile_options", "compile_options_debug"):
        options = getattr(compiler, attribute, None)
        if options is not None:
            setattr(compiler, attribute, ["/W4" if option == "/W3" else option for option in options])


def compile_args(compiler_type: str) -> list[str]:
    if compiler_type == "msvc":
        return ["/std:c++latest", "/WX", "/permissive-", "/Zc:__cplusplus"]
    return ["-std=c++23", "-Wall", "-Wextra", "-Werror", "-pthread"]


def link_args(compiler_type: str) -> list[str]:
    return [] if compiler_type == "msvc" else ["-pthread"]
