#!/usr/bin/env python3
#
# Copyright (c) 2026 Analog Devices, Inc.
# SPDX-License-Identifier: BSD-3-Clause
#
"""What capi_resolve_platform() and capi_create_application() promise a
platform package. Run with the SDK venv:
python sdk/tests/test_create_application.py.

Each test configures a throwaway platform and application with the host
toolchain; nothing is built."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest

SDK = Path(__file__).resolve().parents[1]
CAPI = SDK.parent
sys.path.insert(0, str(SDK / "tools"))
from module_yaml import capi_own_version  # noqa: E402

# Every module but capi itself must say which capi it targets. Derived, not
# written out, so a capi release does not fail every fixture here.
RC = f'capi_version_requirement: ">={capi_own_version()}"\n'


class CreateApplicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.platform = self.root / "capi-test"
        self.app = self.root / "app"

    def write(self, path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(text))

    def make_platform(self, module_extra="", board="soc1/core0", defconfig="CONFIG_FOO=y\n"):
        """A platform whose Kconfig has FOO and BAR, with one board."""
        self.write(
            self.platform / "module.yml",
            "name: capi-test\nversion: '1.0.0'\ntype: platform\n"
            f"{RC}" + module_extra,
        )
        self.write(
            self.platform / "Kconfig",
            """\
            config FOO
            \tbool "foo"
            config BAR
            \tbool "bar"
            """,
        )
        self.write(
            self.platform / "CMakeLists.txt",
            "add_library(capi_test INTERFACE)\nadd_library(capi::test ALIAS capi_test)\n",
        )
        board_dir = self.platform / "boards" / board
        leaf = Path(board).name
        if defconfig is not None:
            self.write(board_dir / f"{leaf}_defconfig", defconfig)
        self.write(board_dir / "board.cmake", "")
        return board_dir

    def make_app(self, board="soc1/core0", before="", after="", resolve_only=False):
        self.write(
            self.app / "module.yml",
            "name: app\nversion: '1.0.0'\ntype: application\n"
            f"{RC}"
            "depends: {platform: {test: }}\n",
        )
        self.write(self.app / "main.c", "int main(void) { return 0; }\n")
        body = (
            "cmake_minimum_required(VERSION 3.21)\n"
            f'set(CAPI_DIR "{CAPI.as_posix()}")\n'
            f'set(CAPI_VENV_PYTHON "{Path(sys.executable).as_posix()}")\n'
            f"set(PLATFORM test)\nset(BOARD {board})\n"
            'include("${CAPI_DIR}/sdk/cmake/capi_create_application.cmake")\n'
            "capi_resolve_platform()\n" + before
        )
        if resolve_only:
            body += "project(fixture LANGUAGES NONE)\n"
        else:
            body += (
                "project(fixture LANGUAGES C)\n"
                "capi_create_application(fixture SOURCES main.c)\n"
            )
        self.write(self.app / "CMakeLists.txt", body + after)

    def configure(self, cwd=None):
        return subprocess.run(
            ["cmake", "-S", str(self.app), "-B", str(self.app / "build")],
            capture_output=True, text=True, cwd=cwd,
            env={**os.environ, "CAPI_WORKSPACE": str(self.root)},
        )

    def assertConfigures(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def assertFailsWith(self, text, cwd=None):
        result = self.configure(cwd)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(text, " ".join((result.stdout + result.stderr).split()))

    # -- capi_version_requirement -------------------------------------------------------

    def test_an_application_without_capi_version_requirement_stops_the_configure(self):
        """Mandatory at build time, not only under the check subcommand."""
        self.make_platform()
        self.make_app()
        module = self.app / "module.yml"
        module.write_text(module.read_text().replace(RC, ""))
        self.assertFailsWith("'capi_version_requirement' is a required property")

    def test_a_capi_version_requirement_that_is_not_a_version_stops_the_configure(self):
        self.make_platform()
        self.make_app()
        module = self.app / "module.yml"
        module.write_text(module.read_text().replace(RC, "capi_version_requirement: true\n"))
        self.assertFailsWith("version constraint")

    # -- compound BOARD and its defconfig ------------------------------------

    def test_compound_board_defconfig_is_named_after_its_last_component(self):
        self.make_platform()
        self.make_app(after='if(NOT CONFIG_FOO)\n  message(FATAL_ERROR "defconfig not applied")\nendif()\n')
        self.assertConfigures()

    def test_missing_board_defconfig_stops_the_configure(self):
        self.make_platform(defconfig=None)
        self.make_app()
        self.assertFailsWith("board defconfig not found")

    # -- CAPI_EXTRA_CONF_FILE -----------------------------------------------

    def test_extra_conf_file_is_applied_after_the_board_defconfig(self):
        self.make_platform()
        self.write(self.root / "extra.conf", "CONFIG_BAR=y\n# CONFIG_FOO is not set\n")
        self.make_app(
            before=f'set(CAPI_EXTRA_CONF_FILE "{(self.root / "extra.conf").as_posix()}")\n',
            after='if(NOT CONFIG_BAR OR CONFIG_FOO)\n  message(FATAL_ERROR "extra fragment not layered last")\nendif()\n',
        )
        self.assertConfigures()

    def test_missing_extra_conf_file_stops_the_configure(self):
        self.make_platform()
        self.make_app(before='set(CAPI_EXTRA_CONF_FILE "/nonexistent/extra.conf")\n')
        self.assertFailsWith("CAPI_EXTRA_CONF_FILE names /nonexistent/extra.conf")

    def test_relative_extra_conf_file_is_taken_against_the_application(self):
        # Configured from a directory other than the application's, as a
        # build directory or a CI job usually is.
        self.make_platform()
        self.write(self.app / "conf" / "extra.conf", "CONFIG_BAR=y\n")
        self.make_app(
            before='set(CAPI_EXTRA_CONF_FILE "conf/extra.conf")\n',
            after='if(NOT CONFIG_BAR)\n  message(FATAL_ERROR "relative fragment not applied")\nendif()\n',
        )
        result = self.configure(cwd=self.root)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_relative_extra_conf_file_names_where_it_looked(self):
        self.make_platform()
        self.make_app(before='set(CAPI_EXTRA_CONF_FILE "conf/extra.conf")\n')
        self.assertFailsWith(
            f"CAPI_EXTRA_CONF_FILE names conf/extra.conf ({(self.app / 'conf' / 'extra.conf').as_posix()})",
            cwd=self.root,
        )

    # -- per-board toolchain ------------------------------------------------

    def _toolchain_fixture(self, board_module_dirs):
        self.make_platform(module_extra="build: {cmake: {toolchain: platform.cmake}}\n")
        self.write(self.platform / "platform.cmake", "")
        for rel in board_module_dirs:
            d = self.platform / "boards" / rel
            self.write(d / "module.yml",
                       "name: b\nversion: '1.0.0'\ntype: platform\n"
                       f"{RC}" + "build: {cmake: {toolchain: tc.cmake}}\n")
            self.write(d / "tc.cmake", "")

    def _expect_toolchain(self, path):
        self.make_app(
            resolve_only=True,
            before=(
                f'if(NOT CMAKE_TOOLCHAIN_FILE STREQUAL "{path.as_posix()}")\n'
                '  message(FATAL_ERROR "toolchain is ${CMAKE_TOOLCHAIN_FILE}")\nendif()\n'
            ),
        )
        self.assertConfigures()

    def test_platform_toolchain_without_board_override(self):
        self._toolchain_fixture([])
        self._expect_toolchain(self.platform / "platform.cmake")

    def test_board_toolchain_overrides_the_platform_toolchain(self):
        self._toolchain_fixture(["soc1/core0"])
        self._expect_toolchain(self.platform / "boards/soc1/core0/tc.cmake")

    def test_nearest_board_toolchain_wins(self):
        self._toolchain_fixture(["soc1", "soc1/core0"])
        self._expect_toolchain(self.platform / "boards/soc1/core0/tc.cmake")

    def test_board_toolchain_is_inherited_from_a_parent_directory(self):
        self._toolchain_fixture(["soc1"])
        self._expect_toolchain(self.platform / "boards/soc1/tc.cmake")

    # -- where a path may point ---------------------------------------------

    def test_a_path_may_not_leave_what_it_is_relative_to(self):
        """'//' is relative to CAPI_WORKSPACE and anything else to the module's
        own directory; neither may be escaped with '..'."""
        for path, leaves in (("../capi-test", "app"),
                             ("//../capi-test", "CAPI_WORKSPACE")):
            with self.subTest(path=path):
                self.make_platform()
                self.make_app(before=f"set(PLATFORM test)\nset(BOARD soc1/core0)\n")
                module = self.app / "module.yml"
                module.write_text(module.read_text().replace(
                    "platform: {test: }", "platform: {test: {path: %s}}" % path))
                self.assertFailsWith(f"leaves")
                self.assertIn(leaves, self.configure().stderr)

    def test_a_path_names_a_package_not_the_root_itself(self):
        self.make_platform()
        self.make_app(before="set(PLATFORM test)\nset(BOARD soc1/core0)\n")
        module = self.app / "module.yml"
        module.write_text(module.read_text().replace(
            "platform: {test: }", "platform: {test: {path: //}}"))
        self.assertFailsWith("not a package in it")

    # -- try_compile forwarding --------------------------------------------

    def test_selection_reaches_compiler_check_projects(self):
        self.make_platform()
        self.make_app(
            resolve_only=True,
            before=(
                "foreach(v PLATFORM BOARD TARGET)\n"
                "  if(NOT v IN_LIST CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)\n"
                '    message(FATAL_ERROR "${v} not forwarded")\n'
                "  endif()\nendforeach()\n"
            ),
        )
        self.assertConfigures()

    # -- platform depends ---------------------------------------------------

    def test_platform_depends_resolve_relative_to_the_platform(self):
        self.make_platform(module_extra="depends: {packages: [{vendor-sdk: {path: vendor}}]}\n")
        (self.platform / "vendor").mkdir()
        expected = (self.platform / "vendor").as_posix()
        self.make_app(
            resolve_only=True,
            before=f'if(NOT VENDOR_SDK_DIR STREQUAL "{expected}")\n'
                   '  message(FATAL_ERROR "VENDOR_SDK_DIR=${VENDOR_SDK_DIR}")\nendif()\n',
        )
        self.assertConfigures()

    def test_application_resolution_of_a_shared_dependency_wins(self):
        self.make_platform(module_extra="depends: {packages: [{vendor-sdk: {path: vendor}}]}\n")
        (self.platform / "vendor").mkdir()
        (self.app / "vendor").mkdir(parents=True)
        self.make_app(resolve_only=True)
        module = self.app / "module.yml"
        module.write_text(
            "name: app\nversion: '1.0.0'\ntype: application\n"
            f"{RC}" + "depends:\n"
            "  platform: {test: }\n"
            "  packages: [{vendor-sdk: {path: vendor}}]\n"
        )
        expected = (self.app / "vendor").as_posix()
        text = (self.app / "CMakeLists.txt").read_text().replace(
            "project(fixture",
            f'if(NOT VENDOR_SDK_DIR STREQUAL "{expected}")\n'
            '  message(FATAL_ERROR "VENDOR_SDK_DIR=${VENDOR_SDK_DIR}")\nendif()\n'
            "project(fixture",
        )
        (self.app / "CMakeLists.txt").write_text(text)
        self.assertConfigures()

    # -- CAPI_PLATFORM_DIR / CAPI_BOARD_DIR ----------------------------------

    def test_platform_and_board_dirs_reach_board_cmake_and_the_vendor_sdk(self):
        board_dir = self.make_platform(module_extra="build: {cmake: {vendor_sdk: sdk.cmake}}\n")
        self.write(
            board_dir / "board.cmake",
            f'if(NOT CAPI_BOARD_DIR STREQUAL "{board_dir.as_posix()}")\n'
            '  message(FATAL_ERROR "board.cmake: CAPI_BOARD_DIR=${CAPI_BOARD_DIR}")\nendif()\n',
        )
        self.write(
            self.platform / "sdk.cmake",
            "function(capi_test_config_sdk target)\n"
            f'  if(NOT CAPI_PLATFORM_DIR STREQUAL "{self.platform.as_posix()}")\n'
            '    message(FATAL_ERROR "sdk: CAPI_PLATFORM_DIR=${CAPI_PLATFORM_DIR}")\n'
            "  endif()\nendfunction()\n",
        )
        self.make_app()
        self.assertConfigures()


if __name__ == "__main__":
    unittest.main()
