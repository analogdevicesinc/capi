# module.yml

Every CAPI package has a `module.yml` at its root: what it is, what it
needs, and what it provides.

Each file is checked against the JSON schema for its `type`,
`sdk/schemas/<type>.module.schema.v1.json`, which rejects every key that
type does not carry. The schemas are generated from capi's own
`module.yml` by `sdk/tools/generate_module_schema.py`; regenerate them
after changing a list there. Name the schema on the first line so an
editor checks the file as you type:

```yaml
# yaml-language-server: $schema=../capi/sdk/schemas/platform.module.schema.v1.json
```

Check files with `python sdk/tools/module_yaml.py check <module.yml>...`.

The build reads this format: `capi_parse_module()` runs
`module_yaml.py describe` on every configure, which validates the
module.yml against its type's schema and writes it out as CMake variables.

## Fields every module carries

| Field | Meaning |
|---|---|
| `name` | The package name, which is also its directory under `$CAPI_WORKSPACE`. Not `.`, and no `/`, `\`, `..`, `"`, `;` or whitespace. |
| `type` | One of the types below. |
| `version` | Required in every module.yml: this module's own version, `X.Y.Z`. |
| `description` | One line. |
| `capi_version_requirement` | Required in every module.yml but capi's own: a constraint on capi's version (see [Version constraints](#version-constraints)). |
| `repo_refs` | Repositories named once, `{<name>: {git: <url>, checkout: <what to check out>}}`, for depends entries that say `repo: <name>`. |

## Types

| Type | Is | Also carries |
|---|---|---|
| `application` | an end product; the only type that selects a platform | `boards`, `depends` with `platform` |
| `platform` | a platform backend | `depends.packages`, `provides`, `build` |
| `board` | a board, of the `role` below | `role` (required) |
| `collection` | several modules of any types kept in one repository: drivers, the apps that use them, the middleware that ties them together | `modules` (required), `depends.packages`; never `provides` |
| `driver` | the driver for one device, or one family of devices | `depends`, `provides`, `kconfig` |
| `middleware` | a library built on the HAL interfaces, or on none | `depends`, `provides` |
| `utility` | a base library other packages build on | `depends`, `provides` |
| `interface` | capi itself; the only type with no `capi_version_requirement`, being the thing versioned | the catalogs: `module_types`, `interfaces` |

- `provides.interfaces` and `depends.interfaces` name entries of capi's
  interface catalog, one per `inc/capi_<name>.h`. A package's own API is
  not an interface: consumers pick the package by name.
- `boards` (application) lists the boards the application has presets
  for.
- `role` (board) says how the board relates to the build:
  - `host`: a board carrying the MCU a build targets, `boards/<board>/` in
    a platform package, or one core of it, `boards/<board>/<core>/`,
    selected with `BOARD="<board>/<core>"`.
  - `addon`: a board that plugs into a host board through one of its
    connectors.
- `modules` (collection) lists the directories of the modules it holds,
  relative to its module.yml and without `..`. Each has its own module.yml
  saying what it is and what it provides, so the collection itself
  provides nothing. `module_yaml.py check` makes sure every listed
  directory has a module.yml.
- A driver says which interfaces it implements and which it needs. Both
  are optional:

  ```yaml
  kconfig: AD7124                   # the Kconfig symbol that builds it
  depends: {interfaces: [spi, gpio]}
  provides: {interfaces: [adc]}
  ```
- A `utility` and a `middleware` carry the same fields. The difference
  between them is editorial -- a utility is a base other packages build
  on, a middleware is a library built on the HAL interfaces -- and the
  schema does not try to enforce it. A utility may implement an interface:
  a software `crc` or a pool allocator providing `alloc` is a utility, not
  a driver, since there is no device behind it.

## build

A platform's two CMake files, both relative to its module.yml and both
ending in `.cmake`. No other type carries `build`.

```yaml
build:
  cmake:
    toolchain: toolchain/toolchain.cmake      # becomes CMAKE_TOOLCHAIN_FILE
    vendor_sdk: sdk/maxim_platform_sdk.cmake  # wires up MSDK, STM32Cube, ...
```

- `cmake.toolchain` is set as `CMAKE_TOOLCHAIN_FILE` before `project()`
  runs, which is what makes it the compiler selection. It is read by
  `capi_resolve_platform()`, so that call has to come before `project()`.
- `cmake.vendor_sdk` must define `capi_<platform>_config_sdk(TARGET)`.
  `capi_create_application()` `include()`s the file and then calls that
  function by name, after the `CONFIG_*` symbols and `board.cmake` (which
  it may read) and before the platform package is added (which may depend
  on what it sets up).
- They are nested under `cmake:` because that is what they both are.
  CMake reads any file extension as long as the contents are CMake;
  requiring `.cmake` in the name is capi's own rule, so that naming a
  shell script here fails at check time instead of as a CMake parse error.
- There is no `build.sources` or `build.includes`. An application lists
  its sources in the `capi_create_application()` call; a package lists
  them in its own CMakeLists.txt.

## depends

```yaml
repo_refs:
  vendor-drivers: {git: "https://.../vendor-drivers.git", checkout: main}
depends:
  platform:                                  # applications only, keyed by PLATFORM
    maxim:                                   #   no constraint
    stm32: ">=1.0.0"                         #   a plain value is a version constraint
    othersoc: {repo: vendor-drivers, subdir: capi/othersoc}
  packages:
    - capi-util                              # just a name
    - capi-iio: ">=1.0.0"
    - capi-network: ">1.0.1,<1.2.3"          # a range
    - oa_tc6: {path: third_party/oa_tc6}     # inside this module
    - lib: {git: "https://.../lib.git", checkout: v1.2.0}
  interfaces: [uart, spi, time]              # needed on every target
```

An entry may carry:

- `version`: a constraint on that package's own `version`.
- `path`: where the package is. `//<dir>` is relative to
  `$CAPI_WORKSPACE`, anything else to this module.yml's directory. Never
  absolute. With `git`, it is where the clone goes.
- `git`: the repository URL, if the package can be fetched.
- `checkout`: what to check out: a branch, a tag or a commit, anything
  `git checkout` accepts. If a name could mean more than one thing, git's
  own rules decide. It can't start with `-`, which git would read as an
  option; git doesn't allow branch names like that anyway. Needs `git`.
- `repo`: the name of an entry in this file's `repo_refs`, instead of
  `git` and `checkout`.
- `subdir`: where the package sits inside its repository. Relative, and
  no `..`.

The schema only checks a `path`'s characters -- that it is relative, and
has no `"` or `;`. Where it may point is checked when it is resolved, not
by `module_yaml.py check`: a `path` may not leave whichever directory it
is relative to. So `../lib` is refused, and a package outside this module
is reached with `//lib`; `//../lib` is refused in the same way, for
leaving `$CAPI_WORKSPACE`.

The schema covers everything except the rules that compare values or look
at other files, which `module_yaml.py check` checks separately:

- a `repo:` must name an entry of the same file's `repo_refs`;
- a version range must leave some version that matches it (see below);
- each directory a collection lists in `modules` holds a module.yml.

## Version constraints

Used by `capi_version_requirement` and by a depends entry's `version`:

- One comparison: `>=`, `<=`, `=`, `>` or `<`, then `X.Y.Z`, such as
  `">=1.0.0"`.
- A range: a lower bound (`>` or `>=`) and an upper bound (`<` or `<=`)
  joined by a comma, with no spaces, such as `">1.0.1,<1.2.3"` for "newer
  than 1.0.1 and older than 1.2.3". The schema only checks how a range
  is written; `module_yaml.py check` rejects one that no version can
  match, such as `">2.0.0,<1.0.0"`.

Quote them in YAML: a value that starts with `>` would otherwise be read
as a block of text.

## Interfaces not in the catalog yet

A module.yml may name an interface whose header is not in capi yet. Until
it is, set `CAPI_ALLOW_UNKNOWN_INTERFACES=1` for `module_yaml.py check`:
an interface outside the catalog is then a warning instead of an error.
Nothing else is relaxed, and the setting goes away once those interfaces
are upstream.
