#!/usr/bin/env python3
#
# Copyright (c) 2026 Analog Devices, Inc.
#
# SPDX-License-Identifier: BSD-3-Clause
#
"""Generates the module.yml JSON schemas under sdk/schemas/ from capi's own
module.yml.

One common base, module.base.schema.v1.json, holds what every module
carries (name, type, version, description, capi_version_requirement, repo_refs) and
every shared definition under $defs. Each module type then has its own
<type>.module.schema.v1.json, which $refs the base, adds the fields that
type may carry, and closes the object with unevaluatedProperties: false --
JSON Schema 2020-12's way of rejecting unknown keys across a $ref, which
draft-07's additionalProperties could not. A module.yml references the one
schema for its type.

module.yml's catalogs (module_types, interfaces) are the single source of
truth for the enums these schemas embed: every interface a module requires
or provides is checked against them by the schema alone.
sdk/tests/test_module_yaml.py runs this script with --check and fails if a
committed schema differs from what it produces.

Usage:
    generate_module_schema.py --catalog <path/to/capi/module.yml> --out-dir <path/to/sdk/schemas>
    generate_module_schema.py --catalog ... --check <path/to/sdk/schemas>   # exit 1 if stale
"""

import argparse
import glob
import json
import os
import sys

import yaml

from module_yaml import SCHEMA_BASE, module_schema_name

DIALECT = "https://json-schema.org/draft/2020-12/schema"

_VERSION_PATTERN = r"^\d+\.\d+\.\d+$"
# One comparison, or a range: a lower bound and an upper bound joined by a
# comma, e.g. ">1.0.1,<1.2.3".
_SEMVER = r"\d+\.\d+\.\d+"
_CONSTRAINT_PATTERN = (rf"^(?:(>=|<=|=|>|<){_SEMVER}"
                       rf"|(>=|>){_SEMVER},(<=|<){_SEMVER})$")
# //workspace-relative or plain relative; never a single leading '/', a
# drive letter or a backslash. A '"' or ';' would corrupt the generated
# CMake, so neither is allowed anywhere.
_PATH_PATTERN = r"^(?!/(?!/))(?![A-Za-z]:)[^\\\";\n]+$"
# Relative, and no '..' path component.
_SUBDIR_PATTERN = r"^(?!/)(?![A-Za-z]:)(?!(.*/)?\.\.(/|$))[^\\\";\n]+$"
# A subdir path ending in .cmake. CMake reads any extension; requiring the
# name to say so is capi's rule, so a .sh named here fails at check time
# rather than as a CMake parse error.
_CMAKE_FILE_PATTERN = (r"^(?!/)(?![A-Za-z]:)(?!(.*/)?\.\.(/|$))"
                       r"[^\\\";\n]+\.cmake$")
# A module or dependency name: also a directory name (//<name>) and a CMake
# list element, so not '.', and no path separator, '..', quote, ';' or
# whitespace.
_NAME_PATTERN = r"^(?!\.$)(?!.*\.\.)[^/\\\";\s]+$"

# The fields each type may carry beyond the base, naming the base $defs
# entry for each. A module_types entry with no row here is an error: a new
# type has to decide what it may carry.
TYPE_FIELDS = {
    "application": {"boards": "boards", "depends": "application_depends"},
    "board": {"role": "board_role"},
    "collection": {"modules": "modules", "depends": "package_depends",
                   "provides": "no_provides"},
    "driver": {"kconfig": "kconfig", "depends": "depends", "provides": "provides"},
    "interface": {"module_types": "catalog", "interfaces": "catalog",
                  },
    "middleware": {"depends": "depends", "provides": "provides"},
    "platform": {"depends": "package_depends", "provides": "provides",
                 "build": "build"},
    "utility": {"depends": "depends", "provides": "provides"},
}

# Fields a type must carry, beyond the base's name, type and version.
TYPE_REQUIRED = {"board": ["role"], "collection": ["modules"]}

# capi itself does not: it is the thing being versioned.
TYPE_REQUIRES_CAPI = {t for t in TYPE_FIELDS if t != "interface"}

TYPE_TITLES = {
    "application": "an end product; the only type that selects a platform",
    "board": "a board: one carrying an MCU, or one core of it, that a build targets, or "
             "an add-on board that plugs into one",
    "collection": "several modules of any types kept in one repository, each with its "
                  "own module.yml",
    "driver": "the driver for one device, or one family of devices",
    "interface": "capi itself: the catalogs",
    "middleware": "a library built on the HAL interfaces, or on none",
    "platform": "a platform backend",
    "utility": "a base library other packages build on",
}


def _load_catalog(catalog_path):
    with open(catalog_path, "r") as f:
        doc = yaml.safe_load(f) or {}
    return {
        "module_types": sorted(doc.get("module_types") or []),
        "interfaces": sorted(doc.get("interfaces") or []),
    }


def _enum(values):
    return {"type": "string", "enum": values} if values else {"type": "string"}


def _local(name):
    return {"$ref": f"#/$defs/{name}"}


def build_base(catalog):
    defs = {
        "version": {"type": "string", "pattern": _VERSION_PATTERN},
        "constraint": {"type": "string", "pattern": _CONSTRAINT_PATTERN,
                       "description": "A version constraint: (>=|<=|=|>|<)X.Y.Z, or a range "
                                      "(>|>=)X.Y.Z,(<|<=)X.Y.Z such as >1.0.1,<1.2.3."},
        "interface": _enum(catalog["interfaces"]),
        "name": {"type": "string", "pattern": _NAME_PATTERN,
                 "description": "A name, not a location: not '.', and no '/', '\\', '..', '\"', "
                                "';' or whitespace. Locations go in path:."},
        "interfaces": {"type": "array", "uniqueItems": True, "items": _local("interface")},
        "checkout": {"type": "string", "pattern": r"^(?!-)[^\s\";]+$",
                     "description": "A branch, tag or commit, whatever git checkout accepts. "
                                    "If a name could mean more than one thing, git's own "
                                    "rules decide. Never starts with '-', which git would "
                                    "read as an option, and never contains whitespace, '\"' "
                                    "or ';'."},

        # One depends entry, for a platform or a package.
        "entry": {
            "type": "object",
            "properties": {
                "version": {**_local("constraint"),
                            "description": "Constraint on the package's own module.yml version."},
                "path": {"type": "string", "pattern": _PATH_PATTERN,
                         "description": "Where the package is, or, with git:, where it is "
                                        "cloned. //<dir> is relative to CAPI_WORKSPACE, any "
                                        "other path to this module.yml's directory. The "
                                        "resolver refuses a path that leaves whichever of "
                                        "the two it is relative to, so reach a package "
                                        "outside this module with //<dir>, not with '..'. "
                                        "Never absolute (no leading '/', drive letter or "
                                        "backslash), and never contains '\"', ';' or a "
                                        "newline."},
                "git": {"type": "string", "description": "Repository URL, if it can be fetched."},
                "checkout": {**_local("checkout"), "description": "What to check out after "
                                                                  "cloning; needs git:."},
                "subdir": {"type": "string", "pattern": _SUBDIR_PATTERN,
                           "description": "Location of the package inside its repository; "
                                          "relative, without '..', and never contains '\"', "
                                          "';' or a newline."},
                "repo": {"type": "string",
                         "description": "Name of an entry in this module's repo_refs:."},
            },
            "dependentSchemas": {
                "repo": {
                    "not": {"anyOf": [{"required": ["git"]}, {"required": ["checkout"]}]},
                    "description": "repo: names an entry in this module's repo_refs:, "
                                   "which already carries the git URL and what to check "
                                   "out. Set git: and checkout: there, not here.",
                },
                "subdir": {
                    "anyOf": [{"required": ["git"]}, {"required": ["repo"]}],
                    "description": "subdir: locates the package inside a repository, so "
                                   "it needs one: either git: or repo:.",
                },
                # No description: "'git' is a required property" is the
                # whole answer, there being no other way to satisfy it.
                "checkout": {"required": ["git"]},
            },
            "additionalProperties": False,
        },
        # A bare constraint is shorthand for {version: <constraint>}.
        "entry_value": {"oneOf": [{"type": "null"}, _local("constraint"), _local("entry")]},
        "package": {
            "oneOf": [
                {**_local("name"), "description": "Bare package name, found at //<name>."},
                {
                    "type": "object",
                    "minProperties": 1,
                    "maxProperties": 1,
                    "propertyNames": _local("name"),
                    "additionalProperties": _local("entry_value"),
                },
            ]
        },

        # depends: as each type may carry it. depends_fields is open; every
        # variant closes itself.
        "depends_fields": {
            "type": "object",
            "properties": {
                "packages": {"type": "array", "items": _local("package")},
                "interfaces": {
                    **_local("interfaces"),
                    "description": "Interfaces this module REQUIRES. Every declared target "
                                   "must provide them.",
                },
            },
        },
        "application_depends": {
            "$ref": "#/$defs/depends_fields",
            "properties": {
                "platform": {
                    "type": "object",
                    "description": "Keyed by the preset's PLATFORM value; one entry is chosen "
                                   "per build. The package is capi-<key>.",
                    "propertyNames": _local("name"),
                    "additionalProperties": _local("entry_value"),
                },
            },
            "unevaluatedProperties": False,
        },
        # Packages only: a platform provides interfaces rather than
        # requiring them.
        "depends": {"$ref": "#/$defs/depends_fields", "unevaluatedProperties": False},
        "package_depends": {
            "$ref": "#/$defs/depends_fields",
            "properties": {"interfaces": {"not": {}, "description": "Required by the modules "
                                                                     "built from this one, "
                                                                     "not by it."}},
            "unevaluatedProperties": False,
        },
        "provides": {
            "type": "object",
            "properties": {"interfaces": _local("interfaces")},
            "additionalProperties": False,
        },

        # Nested under cmake: because that is what both values are, and so
        # another build system can have its own key.
        "build": {
            "type": "object",
            "properties": {
                "cmake": {
                    "type": "object",
                    "properties": {
                        "toolchain": {
                            "type": "string", "pattern": _CMAKE_FILE_PATTERN,
                            "description": "A CMake toolchain file, relative to this "
                                           "module.yml's directory and ending in "
                                           ".cmake. It becomes CMAKE_TOOLCHAIN_FILE, "
                                           "set before project() runs, so it is what "
                                           "selects the compiler.",
                        },
                        "vendor_sdk": {
                            "type": "string", "pattern": _CMAKE_FILE_PATTERN,
                            "description": "A CMake file that wires up the vendor SDK "
                                           "(MSDK, STM32Cube, ...), relative to this "
                                           "module.yml's directory and ending in "
                                           ".cmake. It must define "
                                           "capi_<platform>_config_sdk(TARGET): "
                                           "capi_create_application() include()s the "
                                           "file and then calls that function.",
                        },
                    },
                    "additionalProperties": False,
                },
            },
            "additionalProperties": False,
        },
        "kconfig": {"type": "string", "pattern": r"^[A-Z][A-Z0-9_]*$",
                    "description": "The Kconfig symbol that builds this driver. Its "
                                   "depends.interfaces apply to a target only where it "
                                   "is enabled."},
        "no_provides": {"not": {}, "description": "Provides no interfaces: each module a "
                                                  "collection holds has its own module.yml, "
                                                  "which is where it says what it provides."},

        "boards": {"type": "array", "uniqueItems": True, "items": {"type": "string"},
                   "description": "The boards this app has presets for, and the targets its "
                                  "interfaces are checked on."},
        "modules": {"type": "array", "minItems": 1, "uniqueItems": True,
                    "items": {"type": "string", "pattern": _SUBDIR_PATTERN,
                              "description": "A directory relative to this module.yml, "
                                             "without '..'."},
                    "description": "The directories of the modules this collection holds, "
                                   "relative to this module.yml and without '..'. Each "
                                   "has its own module.yml."},
        # Not "platform": that word already names a module type, an
        # interface and depends.platform.
        "board_role": {"type": "string", "enum": ["host", "addon"],
                       "description": "host: a board carrying the MCU a build targets, "
                                      "boards/<board>/ in a platform package, or one "
                                      "core of it. addon: a board that plugs into a host "
                                      "board through one of its connectors."},

        # capi's own catalogs, only in capi's root module.yml.
        "catalog": {"type": "array", "uniqueItems": True, "items": {"type": "string"}},
    }

    return {
        "$schema": DIALECT,
        "title": "CAPI module.yml: common base",
        "description": (
            "What every module.yml carries, whatever its type. Not used on its own: a "
            "module.yml references the <type>.module.schema.v1.json for its type, which "
            "extends this one. Generated from capi/module.yml's catalogs -- do not "
            "hand-edit, run generate_module_schema.py instead."
        ),
        "type": "object",
        "required": ["name", "type", "version"],
        "properties": {
            "name": _local("name"),
            "type": _enum(catalog["module_types"]),
            "version": {**_local("version"),
                        "description": "This module's own version, X.Y.Z. Every module.yml "
                                       "must have one."},
            "description": {"type": "string", "minLength": 1},
            # Spelled out, not $ref'd: a $ref would report the referenced
            # definition's description instead of this field's.
            "capi_version_requirement": {
                "type": "string",
                "pattern": _CONSTRAINT_PATTERN,
                "description": "The capi version this module is written against, as a "
                               "version constraint: one of >=, <=, =, > or < followed "
                               "by X.Y.Z, so \">=1.0.0\" or \"=1.0.0\" to pin one "
                               "version, or a range like \">1.0.1,<1.2.3\". It names "
                               "which capi, not whether capi is needed, so it is never "
                               "a boolean or a bare version.",
            },
            "repo_refs": {
                "type": "object",
                "description": "Repositories named once, referenced by name from depends "
                               "entries (repo:).",
                "additionalProperties": {
                    "type": "object",
                    "required": ["git"],
                    "additionalProperties": False,
                    "properties": {"git": {"type": "string"}, "checkout": _local("checkout")},
                },
            },
        },
        "$defs": defs,
    }


def build_type(module_type):
    fields = TYPE_FIELDS[module_type]
    required = TYPE_REQUIRED.get(module_type, []) + (
        ["capi_version_requirement"] if module_type in TYPE_REQUIRES_CAPI else [])
    return {
        "$schema": DIALECT,
        "title": f"CAPI {module_type} module.yml",
        "description": (
            f"A {module_type} module: {TYPE_TITLES.get(module_type, module_type)}. "
            f"Extends {SCHEMA_BASE}. Generated from capi/module.yml's catalogs -- do not "
            f"hand-edit, run generate_module_schema.py instead."
        ),
        "$ref": SCHEMA_BASE,
        # The base requires name and type; this adds what only some types owe.
        **({"required": required} if required else {}),
        "properties": {
            "type": {"const": module_type},
            **{field: {"$ref": f"{SCHEMA_BASE}#/$defs/{d}"} for field, d in fields.items()},
        },
        "unevaluatedProperties": False,
    }


def build_all(catalog):
    """{file name: schema}: the base plus one schema per module type."""
    missing = [t for t in catalog["module_types"] if t not in TYPE_FIELDS]
    if missing:
        raise SystemExit(f"generate_module_schema.py: module_types {missing} have no "
                         f"TYPE_FIELDS entry -- decide which fields they may carry")
    out = {SCHEMA_BASE: build_base(catalog)}
    for module_type in catalog["module_types"]:
        out[module_schema_name(module_type)] = build_type(module_type)
    return out


def _render(schema):
    return json.dumps(schema, indent=2, sort_keys=False) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--catalog", required=True, help="path to capi's own module.yml")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--out-dir", dest="out_dir", help="write the generated schemas here")
    group.add_argument("--check", help="exit 1 if the schemas in this directory differ from "
                                       "freshly generated ones")
    args = parser.parse_args()

    schemas = build_all(_load_catalog(args.catalog))

    if args.out_dir:
        for name, schema in schemas.items():
            with open(os.path.join(args.out_dir, name), "w") as f:
                f.write(_render(schema))
        return 0

    stale = []
    for name, schema in schemas.items():
        path = os.path.join(args.check, name)
        try:
            with open(path, "r") as f:
                existing = f.read()
        except OSError:
            existing = None
        if existing != _render(schema):
            stale.append(name)
    # A schema this catalog no longer generates would still be picked up.
    for path in glob.glob(os.path.join(args.check, "*module*.schema.v1.json")):
        if os.path.basename(path) not in schemas:
            stale.append(f"{os.path.basename(path)} (not generated; remove it)")
    if stale:
        print(
            f"generate_module_schema.py: stale or missing in {args.check}: {', '.join(stale)} "
            f"-- regenerate with: generate_module_schema.py --catalog {args.catalog} "
            f"--out-dir {args.check}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
