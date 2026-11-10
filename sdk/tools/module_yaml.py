#!/usr/bin/env python3
#
# Copyright (c) 2026 Analog Devices, Inc.
#
# SPDX-License-Identifier: BSD-3-Clause
#
"""Read a module.yml: validate it, or hand it to the build.

Validation is against the schema for the module's own type,
sdk/schemas/<type>.module.schema.v1.json, generated from capi's catalogs by
generate_module_schema.py. The schema is the whole contract apart from the
rules in cross_field_errors(), which compare values or look at the files
around the module.yml.

Subcommands:
  check     validate module.yml files. Run by hand and by CI.
  describe  write one module.yml out as CMake variables, for the build to
            include(). Run by capi_parse_module(), so on every configure.
"""

import argparse
import glob
import json
import os
import re
import sys

import jsonschema
from referencing import Registry, Resource
from referencing.jsonschema import DRAFT202012
import yaml


SCHEMA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "schemas")
CAPI_ROOT = os.path.dirname(os.path.dirname(SCHEMA_DIR))


def capi_own_version():
    """capi's own version:, read from the module.yml above this tool."""
    with open(os.path.join(CAPI_ROOT, "module.yml"), encoding="utf-8") as f:
        return (yaml.safe_load(f) or {}).get("version", "")
# One common base, and one schema per module type extending it.
SCHEMA_BASE = "module.base.schema.v1.json"


# Set to 1 while the interfaces a downstream module.yml names are still on
# their way into the catalog: an interface outside it is then a warning, not
# an error. Transitional; see docs/module-yml.md.
ALLOW_UNKNOWN_INTERFACES = "CAPI_ALLOW_UNKNOWN_INTERFACES"

# Fields whose values come from a catalog in capi's own module.yml: a miss
# there is worth naming the catalog for. Every other enum lives in the
# schema alone, where the values themselves are the answer.
CATALOG_FIELDS = {"type", "interfaces", "module_types"}


def module_schema_name(module_type):
    return f"{module_type}.module.schema.v1.json"


def _non_string_keys(value, where=""):
    """(location, key) for every mapping key that is not a string."""
    if isinstance(value, dict):
        for key, sub in value.items():
            if not isinstance(key, str):
                yield where, key
            else:
                yield from _non_string_keys(sub, f"{where}{key}.")
    elif isinstance(value, list):
        for i, sub in enumerate(value):
            yield from _non_string_keys(sub, f"{where[:-1]}[{i}]." if where else f"[{i}].")


def explains(error):
    """A rule's own description, when the rule is one that states its terms
    rather than leaving them to a sub-error: a `not` or a combinator whose
    branches are alternatives."""
    if error.validator in ("not", "anyOf", "oneOf"):
        return error.schema.get("description")
    return None


class ModuleSchemas:
    """Validates a module.yml against the schema for its type.

    Every schema in schema_dir is registered under its file name, which is
    what the type schemas' relative $refs to the base resolve to -- so
    nothing is ever fetched, and the files work unchanged in an editor,
    which resolves the same $refs relative to the file."""

    def __init__(self, schema_dir=SCHEMA_DIR, allow_unknown_interfaces=None):
        self.schema_dir = schema_dir
        if allow_unknown_interfaces is None:
            allow_unknown_interfaces = os.environ.get(ALLOW_UNKNOWN_INTERFACES) == "1"
        self.allow_unknown_interfaces = allow_unknown_interfaces
        self.warnings = []
        self.schemas = {}
        for path in sorted(glob.glob(os.path.join(schema_dir, "*.schema.v1.json"))):
            with open(path) as f:
                self.schemas[os.path.basename(path)] = json.load(f)
        self.registry = Registry().with_resources(
            (name, Resource.from_contents(schema, default_specification=DRAFT202012))
            for name, schema in self.schemas.items()
        )
        self.validators = {}

    def known_types(self):
        suffix = module_schema_name("")
        return sorted(n[: -len(suffix)] for n in self.schemas
                      if n.endswith(suffix) and n != SCHEMA_BASE)

    def _validator(self, schema_name):
        validator = self.validators.get(schema_name)
        if validator is None:
            validator = self.validators[schema_name] = jsonschema.Draft202012Validator(
                self.schemas[schema_name], registry=self.registry)
        return validator

    def errors(self, name, doc):
        if not isinstance(doc, dict):
            return [f"{name}: a module.yml must be a mapping"]
        # YAML allows keys JSON (and so the schema) has no notion of.
        bad_keys = list(_non_string_keys(doc))
        if bad_keys:
            return [f"{name}: {where.rstrip('.') + ': ' if where else ''}key {key!r} "
                    f"must be a string -- quote it" for where, key in bad_keys]
        module_type = doc.get("type")
        schema_name = module_schema_name(module_type) if isinstance(module_type, str) else None
        if schema_name not in self.schemas:
            return [f"{name}: type {module_type!r} has no schema in {self.schema_dir} "
                    f"(known: {', '.join(self.known_types())})"]
        found = []
        for e in self._validator(schema_name).iter_errors(doc):
            # The branch that got furthest into the value says more than "is
            # not valid under any of the given schemas"; a wrong-type branch
            # loses ties. A rule that explains itself outranks both and ends
            # the descent, which would otherwise report one alternative's
            # complaint as if the others did not exist.
            while e.context and not explains(e):
                e = max(e.context, key=lambda sub: (bool(explains(sub)),
                                                    len(sub.absolute_path),
                                                    sub.validator != "type"))
            explained = explains(e)
            where = "" if e.json_path == "$" else f"{e.json_path[2:]}: "
            message = e.message
            if explained:
                # `not: {}` means "no such field here", so it reads as a
                # verdict on the field; any other rule is about the value.
                message = (f"not allowed -- {explained}" if e.validator_value == {}
                           else explained)
            elif e.validator == "enum":
                if CATALOG_FIELDS.intersection(map(str, e.absolute_path)):
                    message = f"{e.instance!r} is not in capi's catalog (capi/module.yml)"
                else:
                    allowed = ", ".join(repr(v) for v in e.validator_value)
                    message = f"{e.instance!r} is not one of {allowed}"
                path = list(e.absolute_path)
                if (self.allow_unknown_interfaces and len(path) >= 2
                        and path[-2] == "interfaces" and isinstance(path[-1], int)):
                    self.warnings.append(f"{name}: {where}{message} "
                                         f"(allowed by {ALLOW_UNKNOWN_INTERFACES}=1)")
                    continue
            elif e.validator in ("pattern", "type") and e.schema.get("description"):
                message = f"{e.instance!r} is not valid here -- {e.schema['description']}"
            elif e.validator == "required":
                # The missing field's description, from the base schema.
                missing = re.findall(r"'([^']+)' is a required property", e.message)
                base = self.schemas.get(SCHEMA_BASE) or {}
                described = (base.get("properties") or {}).get(missing[0]) if missing else None
                if described and described.get("description"):
                    message = f"{e.message} -- {described['description']}"
            found.append(f"{name}: {where}{message}")
        return found


def read_yaml(path):
    """(document, None), or (None, why it can't be read or parsed)."""
    try:
        with open(path, encoding="utf-8") as f:
            return (yaml.safe_load(f) or {}), None
    except (OSError, UnicodeDecodeError, yaml.YAMLError) as e:
        return None, str(e)


def load_yaml(path):
    """The parsed document, or None when the file can't be read or parsed."""
    return read_yaml(path)[0]


def cross_field_errors(doc, module_yml, schemas):
    """The rules a JSON schema cannot express, because they compare values or
    look at other files. Each has a test in sdk/tests showing the schema
    alone accepts what it rejects."""
    name = doc.get("name", "<unnamed>")
    errors = []

    # A depends: entry's `repo:` names an entry of this file's own repo_refs:.
    repos = doc.get("repo_refs") or {}
    depends = doc.get("depends") or {}
    entries = list((depends.get("platform") or {}).items())
    for item in depends.get("packages") or []:
        if isinstance(item, dict):
            entries.extend(item.items())
    for dep_name, fields in entries:
        if isinstance(fields, dict) and "repo" in fields and fields["repo"] not in repos:
            errors.append(f"{name}: {dep_name}: repo {fields['repo']!r} names no entry in "
                          f"this module's repo_refs")

    # Each module a collection lists is a directory holding its own module.yml.
    if doc.get("type") == "collection":
        here = os.path.dirname(os.path.abspath(module_yml))
        for entry in doc.get("modules") or []:
            directory = os.path.normpath(os.path.join(here, entry))
            if directory == here:
                errors.append(f"{name}: modules: {entry!r} is this collection itself")
            elif not os.path.isfile(os.path.join(directory, "module.yml")):
                errors.append(f"{name}: modules: {entry!r} has no module.yml")

    # A range must leave at least one version that matches it.
    constraints = [("capi_version_requirement", doc.get("capi_version_requirement"))]
    for dep_name, fields in entries:
        constraints.append((dep_name, fields.get("version") if isinstance(fields, dict)
                            else fields))
    for where, value in constraints:
        if isinstance(value, str) and "," in value and _range_is_empty(value):
            errors.append(f"{name}: {where}: no version can match {value!r} -- the lower "
                          f"bound is above the upper one")

    return errors


def _range_is_empty(constraint):
    """True for a range "(>|>=)X.Y.Z,(<|<=)X.Y.Z" that no version satisfies."""
    low, high = constraint.split(",")
    low_inclusive, high_inclusive = low.startswith(">="), high.startswith("<=")
    low_version = tuple(int(n) for n in low.lstrip(">=").split("."))
    high_version = tuple(int(n) for n in high.lstrip("<=").split("."))
    if low_version == high_version:
        return not (low_inclusive and high_inclusive)
    return low_version > high_version


def check_module(module_yml, schemas):
    """Every error in one module.yml, schema first."""
    doc, why = read_yaml(module_yml)
    if doc is None:
        return [f"{module_yml}: cannot read or parse: {why}"]
    name = doc.get("name", module_yml) if isinstance(doc, dict) else module_yml
    errors = schemas.errors(name, doc)
    if errors:
        return errors
    return cross_field_errors(doc, module_yml, schemas)

def dependency_id(name):
    identifier = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not identifier or identifier[0].isdigit():
        identifier = "_" + identifier
    return identifier.upper()


def describe(doc):
    lines = ["# Generated by module_yaml.py -- do not edit."]

    def emit(name, value):
        lines.append(f'set({name} "{cmake_quote(value)}")')

    for key in ("name", "type", "version"):
        emit(f"CAPI_MODULE_{key.upper()}", doc.get(key, ""))
    # What the build needs to enforce the constraints it already parses.
    emit("CAPI_MODULE_CAPI_VERSION_REQUIREMENT", doc.get("capi_version_requirement", ""))
    emit("CAPI_OWN_VERSION", capi_own_version())
    cmake_build = (doc.get("build") or {}).get("cmake") or {}
    for key in ("toolchain", "vendor_sdk"):
        emit(f"CAPI_MODULE_CMAKE_{key.upper()}", cmake_build.get(key, ""))

    depends = doc.get("depends") or {}
    repo_refs = doc.get("repo_refs") or {}

    def entry_fields(value):
        """One depends entry as {version, path, git, checkout, subdir}. A bare
        string is a constraint; repo: is expanded from this file's repo_refs."""
        if value is None:
            value = {}
        elif isinstance(value, str):
            value = {"version": value}
        fields = dict(value)
        ref = repo_refs.get(fields.pop("repo", "")) or {}
        for key in ("git", "checkout"):
            if key in ref:
                fields.setdefault(key, ref[key])
        return fields

    def emit_entries(entries, label):
        """entries is {package name: value}. Returns the names, in order."""
        names = []
        for name, value in entries.items():
            if not name or ";" in name:
                raise ValueError(f"invalid {label} name: {name!r}")
            identifier = dependency_id(name)
            if identifier in seen:
                raise ValueError(f"duplicate or colliding dependency name: {name}")
            seen.add(identifier)
            names.append(name)
            fields = entry_fields(value)
            prefix = f"CAPI_DEP_{identifier}_"
            for key in ("version", "path", "git", "checkout", "subdir"):
                emit(prefix + key.upper(), fields.get(key, ""))
        return names

    seen = set()
    # A platform entry is keyed by the PLATFORM value its preset sets; the
    # package is capi-<key>. Only the selected one is resolved, by
    # capi_resolve_platform().
    platforms = depends.get("platform") or {}
    emit_entries({f"capi-{k}": v for k, v in platforms.items()}, "platform")
    emit("CAPI_MODULE_PLATFORMS", ";".join(platforms))

    def as_mapping(package):
        return package if isinstance(package, dict) else {package: None}

    packages = {}
    for package in depends.get("packages") or []:
        packages.update(as_mapping(package))
    emit("CAPI_MODULE_PACKAGES", ";".join(emit_entries(packages, "package")))
    emit("CAPI_MODULE_INTERFACES", ";".join(depends.get("interfaces") or []))
    return "\n".join(lines) + "\n"


def cmake_quote(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$")


def _cmd_describe(args):
    schemas = ModuleSchemas(args.schema_dir)
    errors = check_module(args.module_yml, schemas)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    try:
        with open(args.out, "w") as f:
            f.write(describe(load_yaml(args.module_yml)))
    except (OSError, ValueError, yaml.YAMLError) as error:
        print(f"{args.module_yml}: {error}", file=sys.stderr)
        return 1
    return 0

# -- subcommands -------------------------------------------------------------

def _cmd_check(args):
    schemas = ModuleSchemas(args.schema_dir)
    failed = False
    for module_yml in args.module_yml:
        for error in check_module(module_yml, schemas):
            print(error, file=sys.stderr)
            failed = True
    for warning in schemas.warnings:
        print(f"WARNING: {warning}", file=sys.stderr)
    return 1 if failed else 0

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    check = sub.add_parser("check", help="validate module.yml files")
    check.add_argument("module_yml", nargs="+")
    check.add_argument("--schema-dir", default=SCHEMA_DIR)
    check.set_defaults(func=_cmd_check)

    describe = sub.add_parser("describe", help="write one module.yml out as CMake variables")
    describe.add_argument("module_yml")
    describe.add_argument("--out", required=True)
    describe.add_argument("--schema-dir", default=SCHEMA_DIR)
    describe.set_defaults(func=_cmd_describe)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
