#!/usr/bin/env python3
#
# Copyright (c) 2026 Analog Devices, Inc.
# SPDX-License-Identifier: BSD-3-Clause
#
"""Run with the SDK venv: python sdk/tests/test_module_yaml.py."""

import glob
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest

import yaml

SDK = Path(__file__).resolve().parents[1]
CAPI = SDK.parent
TOOLS = SDK / "tools"
SCHEMAS = str(SDK / "schemas")
CAPI_DIR = str(CAPI)
MODULE_YAML = str(TOOLS / "module_yaml.py")
sys.path.insert(0, str(TOOLS))
from generate_module_schema import _load_catalog, build_all  # noqa: E402
from module_yaml import ModuleSchemas, check_module, cross_field_errors  # noqa: E402


def catalog():
    with open(CAPI / "module.yml") as f:
        return yaml.safe_load(f)


# Two interfaces read from the catalog rather than named. A test that just
# needs a valid interface should not fail because the catalog was trimmed.
AN_INTERFACE, ANOTHER_INTERFACE = catalog()["interfaces"][:2]


class Catalog(unittest.TestCase):
    def test_each_interface_has_its_header(self):
        headers = {Path(h).stem[len("capi_"):] for h in glob.glob(str(CAPI / "inc" / "capi_*.h"))}
        self.assertEqual(set(catalog()["interfaces"]) - headers, set())

    def test_capi_module_yml_is_valid(self):
        self.assertEqual(check_module(str(CAPI / "module.yml"), ModuleSchemas()), [])

    def test_catalog_values_become_the_enums(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "module.yml"
            path.write_text("module_types: [platform]\ninterfaces: [spi]\n")
            schemas = build_all(_load_catalog(path))
            base = schemas["module.base.schema.v1.json"]
            self.assertEqual(base["$defs"]["interface"]["enum"], ["spi"])
            self.assertEqual(base["properties"]["type"]["enum"], ["platform"])
            self.assertEqual(sorted(schemas), ["module.base.schema.v1.json",
                                               "platform.module.schema.v1.json"])

    def test_stale_schemas_are_reported(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "gadget.module.schema.v1.json").write_text("{}\n")
            r = subprocess.run(
                [sys.executable, str(TOOLS / "generate_module_schema.py"),
                 "--catalog", str(CAPI / "module.yml"), "--check", d],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn("gadget.module.schema.v1.json (not generated; remove it)", r.stderr)
            self.assertIn("module.base.schema.v1.json", r.stderr)



# What each type must carry besides name, type and version.
REQUIRED_BY_TYPE = {"board": {"role": "host"}, "collection": {"modules": ["drivers/x"]}}


class Schema(unittest.TestCase):
    """One base schema, extended by one closed schema per module type."""

    def setUp(self):
        self.schemas = ModuleSchemas(SCHEMAS)

    def errors(self, doc):
        """Errors for doc, given the version and the capi constraint every
        module.yml must have unless the test sets its own. capi itself is
        exempt from the constraint: it is the thing being versioned."""
        defaults = {"version": "1.0.0"}
        if doc.get("type") != "interface":
            defaults["capi_version_requirement"] = ">=1.0.0"
        return self.schemas.errors(doc.get("name", "m"), {**defaults, **doc})

    def test_every_module_must_have_a_version(self):
        for kind in sorted(self.schemas.known_types()):
            with self.subTest(kind=kind):
                doc = {"name": "m", "type": kind, **REQUIRED_BY_TYPE.get(kind, {})}
                self.assertIn("'version' is a required property",
                              " ".join(self.schemas.errors("m", doc)))
                self.assertFalse(self.schemas.errors("m", {**doc, "version": "1.0.0",
                                                           "capi_version_requirement": ">=1.0.0"}))

    def test_every_module_but_capi_must_say_which_capi(self):
        """capi itself is exempt: it is the thing being versioned, and its own
        version: is that statement."""
        for kind in sorted(self.schemas.known_types()):
            with self.subTest(kind=kind):
                doc = {"name": "m", "type": kind, "version": "1.0.0"}
                reported = " ".join(self.schemas.errors("m", doc))
                if kind == "interface":
                    self.assertEqual(reported, "")
                else:
                    self.assertIn("'capi_version_requirement' is a required property", reported)

    def test_capi_version_requirement_is_a_version_not_a_flag(self):
        """The name reads like a yes/no question, so every wrong shape has to
        say that a version constraint is what was wanted."""
        for bad in (True, False, "1.0.0", 1.0, "yes", ">=1.0"):
            with self.subTest(value=bad):
                reported = " ".join(self.schemas.errors(
                    "m", {"name": "m", "type": "platform", "version": "1.0.0",
                          "capi_version_requirement": bad}))
                self.assertIn("capi_version_requirement", reported)
                self.assertIn("version constraint", reported)
        for good in (">=1.0.0", ">1.0.1,<1.2.3"):
            with self.subTest(value=good):
                self.assertFalse(self.schemas.errors(
                    "m", {"name": "m", "type": "platform", "version": "1.0.0",
                          "capi_version_requirement": good}))

    def test_every_type_has_a_schema(self):
        with open(os.path.join(CAPI_DIR, "module.yml")) as f:
            types = set(yaml.safe_load(f)["module_types"])
        self.assertEqual(set(self.schemas.known_types()), types)

    def test_schemas_are_2020_12(self):
        for name, schema in self.schemas.schemas.items():
            self.assertEqual(schema["$schema"], "https://json-schema.org/draft/2020-12/schema", name)

    def test_unknown_type_has_no_schema(self):
        self.assertIn("has no schema", " ".join(self.errors({"name": "m", "type": "gadget"})))

    def test_a_field_of_another_type_is_rejected(self):
        self.assertTrue(self.errors({"name": "p", "type": "platform", "kconfig": "X"}))
        self.assertTrue(self.errors({"name": "p", "type": "platform", "boards": ["b"]}))
        self.assertTrue(self.errors({"name": "b", "type": "board", "role": "host", "notes": "x"}))
        # A board carries role: and nothing else of its own.
        for gone in ("provides", "supported", "implements"):
            with self.subTest(field=gone):
                self.assertTrue(self.errors({"name": "b", "type": "board", "role": "host",
                                             gone: {"driver": "x"}}))

    def test_nested_blocks_are_closed_too(self):
        self.assertTrue(self.errors({"name": "p", "type": "platform",
                                     "depends": {"packages": ["x"], "extra": []}}))
        self.assertTrue(self.errors({"name": "p", "type": "platform",
                                     "provides": {"interfaces": ["spi"], "extra": []}}))
        self.assertTrue(self.errors({"name": "a", "type": "application",
                                     "depends": {"platform": {"x": {"env": "X_PATH"}}}}))

    def test_only_an_application_selects_a_platform(self):
        self.assertTrue(self.errors({"name": "p", "type": "platform",
                                     "depends": {"platform": {"x": None}}}))
        self.assertFalse(self.errors({"name": "a", "type": "application",
                                      "depends": {"platform": {"x": None}}}))

    def test_an_enum_outside_a_catalog_names_its_values(self):
        """Most enums are generated from a catalog in capi's own module.yml and
        naming that file is the useful answer. A board's role is not: it lives
        in the schema alone, where module.yml lists nothing to look up."""
        self.assertIn("'nope' is not one of 'host', 'addon'", " ".join(self.errors(
            {"name": "b", "type": "board", "role": "nope"})))
        self.assertIn("is not in capi's catalog (capi/module.yml)", " ".join(self.errors(
            {"name": "d", "type": "driver", "kconfig": "D",
             "provides": {"interfaces": ["not_an_interface"]}})))

    def test_a_rule_about_a_package_entry_states_its_own_terms(self):
        """A dependentSchemas rule fails with `not` or `anyOf`, whose default
        message is the schema itself. Each carries a description, and finding
        it means not descending past it into one alternative's complaint."""
        def why(entry, **extra):
            return " ".join(self.errors(
                {"name": "m", "type": "middleware",
                 "depends": {"packages": [entry]}, **extra}))

        self.assertIn("Set git: and checkout: there, not here",
                      why({"x": {"repo": "r", "git": "https://e.x/o.git"}},
                          repo_refs={"r": {"git": "https://e.x/r.git"}}))
        self.assertIn("either git: or repo:", why({"x": {"subdir": "drivers"}}))
        # No alternative to name, so the plain message is the whole answer.
        self.assertIn("'git' is a required property", why({"x": {"checkout": "main"}}))
        # Neither rule may swallow the descent the other messages rely on.
        self.assertIn("A version constraint", why({"x": ">=1.0"}))
        self.assertIn("Never absolute", why({"x": {"path": "/abs"}}))

    def test_a_field_not_on_this_type_reads_as_a_verdict_on_the_field(self):
        """`not: {}` is the "no such field here" idiom and keeps its prefix;
        a rule about a value states its terms without one."""
        self.assertIn("provides: not allowed -- Provides no interfaces",
                      " ".join(self.errors({"name": "c", "type": "collection",
                                            "modules": ["d"],
                                            "provides": {"interfaces": ["adc"]}})))
        self.assertIn("depends.interfaces: not allowed -- Required by the modules",
                      " ".join(self.errors({"name": "p", "type": "platform",
                                            "depends": {"interfaces": ["spi"]}})))

    def test_interfaces_come_from_the_catalog_both_ways(self):
        doc = self.app({"interfaces": ["not_an_interface"]})
        self.assertIn("not in capi's catalog", " ".join(self.errors(doc)), "application depends")
        for doc in ({"name": "t", "type": "platform"},
                    {"name": "t", "type": "middleware"}):
            kind = doc["type"]
            doc = {**doc, "provides": {"interfaces": ["not_an_interface"]}}
            self.assertIn("not in capi's catalog", " ".join(self.errors(doc)), kind)

    def app(self, depends):
        return {"name": "a", "type": "application", "depends": depends}

    def test_names_are_not_locations(self):
        for bad in ("../outside", "a/b", "a\\b", "x;y", 'a"b', "a b", "a\nb", "..", "."):
            with self.subTest(bad=bad):
                self.assertTrue(self.errors(self.app({"packages": [bad]})), "bare name")
                self.assertTrue(self.errors(self.app({"packages": [{bad: None}]})), "mapping key")
                self.assertTrue(self.errors(self.app({"platform": {bad: None}})), "platform key")
                self.assertTrue(self.errors({"name": bad, "type": "platform"}), "name:")
        for good in ("capi-maxim", "oa_tc6", "freertos_kernel", "capi.v2"):
            with self.subTest(good=good):
                self.assertFalse(self.errors(self.app({"packages": [good, {good: None}],
                                                       "platform": {good: None}})))
                self.assertFalse(self.errors({"name": good, "type": "platform"}))

    def test_checkout_needs_git(self):
        self.assertTrue(self.errors(self.app({"packages": [{"x": {"checkout": "main"}}]})))
        self.assertTrue(self.errors(self.app({"platform": {"x": {"checkout": "main"}}})))
        self.assertFalse(self.errors(self.app({"packages": [{"x": {"git": "u", "checkout": "main"}}]})))
        self.assertTrue(self.errors(self.app({"packages": [{"x": {"git": "u", "ref": "main"}}]})),
                        "ref: was renamed to checkout:")

    def test_checkout_takes_whatever_git_checkout_takes(self):
        # Branches, tags and commit ids, short or full; a name that looks like
        # a commit id is git's to sort out, not the schema's.
        for value in ("main", "v1.2.0", "release/2026", "deadbeef", "1234567", "abc123",
                      "0123456789abcdef0123456789abcdef01234567"):
            with self.subTest(value=value):
                self.assertFalse(self.errors(self.app({"packages": [{"x": {"git": "u",
                                                                           "checkout": value}}]})))
                self.assertFalse(self.errors({"name": "a", "type": "application", "repo_refs": {
                    "r": {"git": "u", "checkout": value}}}))
        for bad in ("a b", 'a"b', "a;b", "", "-b", "--upload-pack=x"):
            with self.subTest(bad=bad):
                self.assertTrue(self.errors(self.app({"packages": [{"x": {"git": "u",
                                                                          "checkout": bad}}]})))
        self.assertTrue(self.errors({"name": "a", "type": "application", "repo_refs": {
            "r": {"git": "u", "ref": "main"}}}), "ref: was renamed to checkout:")

    def test_a_version_constraint_is_one_comparison_or_a_range(self):
        for good in (">=1.0.0", "=2.3.4", "<3.0.0", ">1.0.1,<1.2.3", ">=1.0.1,<=1.2.3",
                     ">1.0.1,<=1.2.3"):
            with self.subTest(good=good):
                self.assertFalse(self.errors(self.app({"packages": [{"x": good}]})))
                self.assertFalse(self.errors({"name": "m", "type": "utility", "capi_version_requirement": good}))
        for bad in ("1.0.0", ">=1", "<1.2.3,>1.0.1", ">1.0.1,>1.2.3", "=1.0.1,<1.2.3",
                    ">1.0.1, <1.2.3", ">1.0.1 <1.2.3", ">1.0.1,<1.2.3,<2.0.0", ">1.0.1,"):
            with self.subTest(bad=bad):
                self.assertTrue(self.errors(self.app({"packages": [{"x": bad}]})))
                self.assertTrue(self.errors({"name": "m", "type": "utility", "capi_version_requirement": bad}))

    def test_middleware_and_utility(self):
        # Same fields: what separates the two is editorial. A utility may
        # implement an interface, no device being needed for a software crc.
        depends = {"packages": ["capi-util"], "interfaces": ["alloc"]}
        for kind in ("middleware", "utility"):
            with self.subTest(kind=kind):
                self.assertFalse(self.errors({"name": "m", "type": kind, "depends": depends,
                                              "provides": {"interfaces": [AN_INTERFACE]}}))
                self.assertFalse(self.errors({"name": "m", "type": kind, "depends": depends}))
                for provides in ({"interfaces": []}, {}):
                    self.assertFalse(self.errors({"name": "m", "type": kind,
                                                  "provides": provides}), provides)
                self.assertTrue(self.errors({"name": "m", "type": kind,
                                             "provides": {"interfaces": ["not_one"]}}))
        for kind in ("middleware", "utility"):
            with self.subTest(kind=kind):
                self.assertTrue(self.errors({"name": "m", "type": kind,
                                             "depends": {"platform": {"maxim": None}}}))
                self.assertTrue(self.errors({"name": "m", "type": kind, "boards": ["b"]}))

    def test_only_a_platform_carries_build_and_only_cmake_files(self):
        def platform(build):
            return {"name": "capi-maxim", "type": "platform", "build": build}
        both = {"cmake": {"toolchain": "toolchain/toolchain.cmake",
                          "vendor_sdk": "sdk/maxim_platform_sdk.cmake"}}
        self.assertFalse(self.errors(platform(both)))
        for ok in ({}, {"cmake": {}}, {"cmake": {"toolchain": "a/b.cmake"}}):
            self.assertFalse(self.errors(platform(ok)), ok)
        # Both levels are closed, so an unknown key is rejected wherever it sits.
        for bad in ({"cmake": {"toolchain": "setup.sh"}},        # not a CMake file
                    {"cmake": {"toolchain": "/abs/tc.cmake"}},   # absolute
                    {"cmake": {"toolchain": "../tc.cmake"}},     # leaves the module
                    {"cmake": {"gadget": "x.cmake"}},
                    {"gadget": {"toolchain": "x.cmake"}}):
            with self.subTest(build=bad):
                self.assertTrue(self.errors(platform(bad)))
        self.assertIn("capi_<platform>_config_sdk", " ".join(self.errors(
            platform({"cmake": {"vendor_sdk": "sdk.sh"}}))))
        # Every other type: build: is not theirs.
        for kind in ("application", "board", "driver", "middleware", "utility",
                     "collection"):
            with self.subTest(kind=kind):
                doc = {"name": "m", "type": kind, "build": both,
                       **REQUIRED_BY_TYPE.get(kind, {})}
                self.assertTrue(self.errors(doc))

    def test_a_board_says_what_role_it_has(self):
        self.assertIn("'role' is a required property",
                      " ".join(self.errors({"name": "b", "type": "board"})))
        for role in ("host", "addon"):
            with self.subTest(role=role):
                self.assertFalse(self.errors({"name": "b", "type": "board", "role": role}))
        # The roles deliberately left out, so none of them creeps back.
        for gone in ("eval", "proto", "platform"):
            with self.subTest(role=gone):
                self.assertTrue(self.errors({"name": "b", "type": "board", "role": gone}))
        self.assertTrue(self.errors({"name": "b", "type": "platform_board"}))

    def test_a_collection_lists_its_modules_and_provides_nothing(self):
        modules = ["drivers/accel/adxl345", "boards/eval-x", "projects/eval-x/basic"]
        self.assertFalse(self.errors({"name": "c", "type": "collection", "modules": modules,
                                      "depends": {"packages": ["capi-util"]}}))
        self.assertIn("'modules' is a required property", " ".join(self.errors(
            {"name": "c", "type": "collection"})))
        for bad in ([], ["a", "a"], ["../outside"], ["a/../../b"], ["/abs"], ['a"b']):
            with self.subTest(modules=bad):
                self.assertTrue(self.errors({"name": "c", "type": "collection",
                                             "modules": bad}))
        self.assertIn("Provides no interfaces", " ".join(self.errors(
            {"name": "c", "type": "collection", "modules": modules,
             "provides": {"interfaces": ["spi"]}})))
        self.assertTrue(self.errors({"name": "c", "type": "collection", "modules": modules,
                                     "depends": {"interfaces": ["spi"]}}))
        self.assertTrue(self.errors({"name": "c", "type": "driver_collection"}))

    def test_a_driver_implements_interfaces_and_says_which_it_needs(self):
        driver = {"name": "adxl345", "type": "driver", "kconfig": "ADXL345",
                  "depends": {"packages": ["capi-util"], "interfaces": ["spi", "i2c"]},
                  "provides": {"interfaces": ["adc"]}}
        self.assertFalse(self.errors(driver))
        for provides in (None, {}, {"interfaces": []}, {"interfaces": ["adc"]}):
            with self.subTest(provides=provides):
                doc = {"name": "d", "type": "driver"}
                self.assertFalse(self.errors(doc if provides is None
                                             else {**doc, "provides": provides}))
        # Not in the format yet: the list of capabilities is not settled.
        for bad in ({"interfaces": ["adc", "adc"]},
                    {"capabilities": [{"name": "input_temperature"}]},
                    {"functions": [{"name": "capi_adc_read"}]},
                    {"interfaces": ["adc"], "extra": []}):
            with self.subTest(provides=bad):
                self.assertTrue(self.errors({**driver, "provides": bad}))
        for bad in ("adxl345", "", "1X", "CONFIG-X"):
            with self.subTest(kconfig=bad):
                self.assertTrue(self.errors({**driver, "kconfig": bad}))
        for field, value in (("depends", {"platform": {"maxim": None}}), ("boards", ["b"]),
                             ("role", "addon"), ("modules", ["a"]),
                             ("build", {"sources": ["a.c"]}),
                             ("category", "accel"), ("metadata", {"vendor": "adi"})):
            with self.subTest(field=field):
                self.assertTrue(self.errors({**driver, field: value}))
        # driver, middleware, platform and utility all describe what they
        # provide the same way; only a collection provides nothing.
        self.assertTrue(self.errors({"name": "c", "type": "collection", "modules": ["d"],
                                     "provides": {"interfaces": ["adc"]}}))

    def test_lists_hold_each_value_once(self):
        self.assertTrue(self.errors(self.app({"interfaces": ["spi", "spi"]})))
        self.assertTrue(self.errors({"name": "p", "type": "platform",
                                     "provides": {"interfaces": ["adc", "adc"]}}))
        self.assertTrue(self.errors({"name": "a", "type": "application", "boards": ["b", "b"]}))
        for key in ("module_types", "interfaces"):
            self.assertTrue(self.errors({"name": "capi", "type": "interface", key: ["x", "x"]}), key)

    def test_a_platform_takes_a_bare_constraint_as_a_package_does(self):
        self.assertFalse(self.errors(self.app({"platform": {"maxim": ">=1.0.0"}})))
        self.assertIn("A version constraint", " ".join(self.errors(self.app({"platform": {"maxim": ">=1"}}))))

    def test_paths_are_never_absolute(self):
        for bad in ("/usr/include", "C:\\sdk", "c:/sdk", "..\\up", "vendor\\sdk"):
            with self.subTest(bad=bad):
                self.assertTrue(self.errors(self.app({"packages": [{"x": {"path": bad}}]})))
        for bad in ("../up", "/abs", "C:/x", "a/../b", "a\\b"):
            with self.subTest(subdir=bad):
                self.assertTrue(self.errors(self.app({"packages": [{"x": {"git": "u", "subdir": bad}}]})))
        for good in ("//capi-maxim", "../sibling", "vendor/sdk"):
            self.assertFalse(self.errors(self.app({"packages": [{"x": {"path": good}}]})), good)
        self.assertFalse(self.errors(self.app({"packages": [{"x": {"git": "u", "subdir": "a/b..c"}}]})))

    def test_a_platform_provides_interfaces_and_requires_none(self):
        self.assertTrue(self.errors({"name": "p", "type": "platform",
                                     "depends": {"interfaces": ["spi"]}}))
        self.assertFalse(self.errors({"name": "p", "type": "platform",
                                      "depends": {"packages": ["vendor_sdk"]},
                                      "provides": {"interfaces": ["spi"]}}))


class Check(unittest.TestCase):
    def check(self, text, env=None):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "module.yml"
            path.write_text(textwrap.dedent(text))
            full_env = {k: v for k, v in os.environ.items() if k != "CAPI_ALLOW_UNKNOWN_INTERFACES"}
            full_env.update(env or {})
            return subprocess.run([sys.executable, MODULE_YAML, "check", str(path)],
                                  capture_output=True, text=True, env=full_env)

    def test_a_provided_interface_outside_the_catalog_fails(self):
        r = self.check("""\
            name: m
            type: platform
            version: "1.0.0"
            capi_version_requirement: ">=1.0.0"
            provides: {interfaces: [not_an_interface]}
            """)
        self.assertEqual(r.returncode, 1)
        self.assertIn("provides.interfaces[0]: 'not_an_interface' is not in capi's catalog", r.stderr)

    def test_unknown_interfaces_can_be_allowed_during_the_transition(self):
        text = """\
            name: m
            type: application
            capi_version_requirement: \">=1.0.0\"
            depends: {interfaces: [uart, not_yet_upstream]}
            boards: [b]
            version: 1.0
            """
        r = self.check(text, env={"CAPI_ALLOW_UNKNOWN_INTERFACES": "1"})
        self.assertEqual(r.returncode, 1)  # only the interfaces are downgraded
        self.assertIn("WARNING: m: depends.interfaces[1]: 'not_yet_upstream'", r.stderr)
        self.assertIn("m: version: 1.0 is not of type 'string'", r.stderr)
        r = self.check(text.replace("version: 1.0\n", 'version: "1.0.0"\n'),
                       env={"CAPI_ALLOW_UNKNOWN_INTERFACES": "1"})
        self.assertEqual(r.returncode, 0, r.stderr)
        r = self.check(text.replace("version: 1.0\n", 'version: "1.0.0"\n'))
        self.assertEqual(r.returncode, 1)
        self.assertNotIn("WARNING", r.stderr)
        r = self.check("""\
            name: p
            type: platform
            version: "1.0.0"
            capi_version_requirement: ">=1.0.0"
            provides: {interfaces: [also_not_yet]}
            """, env={"CAPI_ALLOW_UNKNOWN_INTERFACES": "1"})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("WARNING: p: provides.interfaces[0]: 'also_not_yet'", r.stderr)

    def test_a_collection_module_must_have_a_module_yml(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "drivers/a").mkdir(parents=True)
            (Path(d) / "drivers/a/module.yml").write_text("name: a\n")
            (Path(d) / "drivers/empty").mkdir()
            path = Path(d) / "module.yml"
            for modules, error in ((["drivers/a"], None),
                                   (["drivers/a", "drivers/empty"], "'drivers/empty' has no"),
                                   (["drivers/missing"], "'drivers/missing' has no"),
                                   (["."], "is this collection itself"),
                                   (["drivers/.."], "without '..'")):
                with self.subTest(modules=modules):
                    path.write_text(f'name: c\ntype: collection\nversion: "1.0.0"\n'
                                    f'capi_version_requirement: ">=1.0.0"\n'
                                    f"modules: {json.dumps(modules)}\n")
                    r = subprocess.run([sys.executable, MODULE_YAML, "check", str(path)],
                                       capture_output=True, text=True)
                    self.assertEqual(r.returncode, 0 if error is None else 1, r.stderr)
                    if error:
                        self.assertIn(error, r.stderr)

    def test_a_range_nothing_can_match_fails(self):
        for constraint, matches in ((">2.0.0,<1.0.0", False), (">1.0.0,<1.0.0", False),
                                    (">=1.0.0,<1.0.0", False), (">1.0.0,<=1.0.0", False),
                                    (">=1.0.0,<=1.0.0", True), (">1.0.1,<1.2.3", True),
                                    (">=1.9.0,<1.10.0", True)):
            for field in (f'capi_version_requirement: "{constraint}"',
                          f'depends: {{packages: [{{x: "{constraint}"}}]}}',
                          f'depends: {{packages: [{{x: {{version: "{constraint}"}}}}]}}'):
                capi = "" if field.startswith("capi_version_requirement") else 'capi_version_requirement: ">=1.0.0"'
                with self.subTest(constraint=constraint, field=field):
                    r = self.check(f"""\
                        name: m
                        type: middleware
                        version: "1.0.0"
                        {capi}
                        {field}
                        """)
                    self.assertEqual(r.returncode, 0 if matches else 1, r.stderr)
                    if not matches:
                        self.assertIn("no version can match", r.stderr)

    def test_a_valid_module_passes_silently(self):
        r = self.check(f"""\
            name: m
            type: platform
            version: "1.0.0"
            capi_version_requirement: ">=1.0.0"
            provides: {{interfaces: [{AN_INTERFACE}, {ANOTHER_INTERFACE}]}}
            """)
        self.assertEqual((r.returncode, r.stdout, r.stderr), (0, "", ""))


if __name__ == "__main__":
    unittest.main()
