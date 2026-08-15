"""Custom setuptools commands for Yellowstone."""
from __future__ import annotations

from pathlib import Path
import subprocess
import sys

from pybind11.setup_helpers import build_ext as pybind11_build_ext
from setuptools import Command

from .compiler import compile_args, link_args, upgrade_msvc_warning_level
from .paths import INCLUDE, PHELPS_INCLUDE, PYTHON_TESTS, ROOT, TESTS


class BuildExt(pybind11_build_ext):
    def build_extensions(self) -> None:
        kind = self.compiler.compiler_type
        if kind == "msvc":
            upgrade_msvc_warning_level(self.compiler)
        for extension in self.extensions:
            extension.extra_compile_args = [*getattr(extension, "extra_compile_args", []), *compile_args(kind)]
            extension.extra_link_args = [*getattr(extension, "extra_link_args", []), *link_args(kind)]
        super().build_extensions()


class _NoOptions(Command):
    user_options: list[tuple[str, str | None, str]] = []
    def initialize_options(self) -> None: pass
    def finalize_options(self) -> None: pass


class NativeTest(_NoOptions):
    description = "build and run native Yellowstone unit tests"

    def run(self) -> None:
        build_ext = self.distribution.get_command_obj("build_ext")
        build_ext.ensure_finalized()
        build_ext.run()
        compiler = build_ext.compiler
        kind = compiler.compiler_type
        if kind == "msvc":
            upgrade_msvc_warning_level(compiler)

        sources = sorted(TESTS.rglob("*.cc"))
        if not sources:
            raise RuntimeError("no native tests found")

        out = ROOT / "build" / "native-tests"
        out.mkdir(parents=True, exist_ok=True)
        for source in sources:
            name = "_".join(source.relative_to(TESTS).with_suffix("").parts)
            objects = compiler.compile(
                [str(source), str(ROOT / "vendor/phelps/source/manager.cc")],
                output_dir=str(out / "obj" / name),
                include_dirs=[str(INCLUDE), str(PHELPS_INCLUDE), str(ROOT)],
                extra_postargs=compile_args(kind),
            )
            compiler.link_executable(objects, name, output_dir=str(out / "bin"), extra_postargs=link_args(kind))
            exe = Path(compiler.executable_filename(name, output_dir=str(out / "bin")))
            subprocess.run([str(exe)], cwd=ROOT, check=True)


class PythonTest(_NoOptions):
    description = "build Yellowstone in place and run Python tests"

    def run(self) -> None:
        command = self.distribution.get_command_obj("build_ext")
        command.inplace = True
        command.ensure_finalized()
        command.run()
        subprocess.run(
            [sys.executable, "-m", "unittest", "discover", "-s", str(PYTHON_TESTS), "-p", "test*.py", "-v"],
            cwd=ROOT,
            check=True,
        )


class TestAll(_NoOptions):
    description = "run native and Python Yellowstone tests"
    def run(self) -> None:
        self.run_command("test_native")
        self.run_command("test_python")


def command_classes() -> dict[str, type[Command]]:
    return {"build_ext": BuildExt, "test_native": NativeTest, "test_python": PythonTest, "test_all": TestAll}
