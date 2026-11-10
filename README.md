# capi

The Common API (CAPI): a portable hardware abstraction layer for ADI
software projects, and the SDK that builds applications against it.

An interface is one header, `inc/capi_<name>.h`, describing what a driver
must provide. `src/capi_<name>.c` is the thin dispatch layer over it. A
*platform package* implements the interfaces for one vendor's silicon; an
*application* names the platform it wants and is built by the SDK.

## Interfaces

| Interface | |
|---|---|
| `adc` | analog-to-digital converter |
| `alloc` | allocation hooks a platform overrides |
| `can` | CAN bus |
| `clk` | clock tree and rates |
| `coprocessor` | coprocessor lifecycle |
| `dac` | digital-to-analog converter |
| `dma` | DMA controller |
| `eth_mac` | Ethernet MAC |
| `eth_phy` | Ethernet PHY |
| `gpio` | general-purpose I/O |
| `i2c` | I2C controller and target |
| `i2s` | I2S audio |
| `i3c` | I3C controller |
| `io` | memory-mapped register access (macros, no dispatch layer) |
| `irq` | interrupt controller |
| `log` | log output a platform supplies |
| `mailbox` | inter-core mailbox |
| `mdio` | MDIO bus |
| `mutex` | mutual exclusion |
| `pinctrl` | pin multiplexing |
| `platform` | init, clocks and the tick hooks a platform supplies |
| `regulator` | voltage regulator |
| `rtc` | real-time clock |
| `sem` | semaphores |
| `spi` | SPI controller |
| `time` | delays and monotonic time |
| `timer` | hardware timers |
| `uart` | UART, and the stdio backend behind printf() |
| `wdt` | watchdog timer |

`capi/module.yml` is the catalog: its `interfaces:` list is what a
`module.yml` may name, and the SDK's schemas are generated from it.

## Platform packages

| Package | |
|---|---|
| [capi-maxim](https://github.com/analogdevicesinc/capi-maxim) | Maxim platform backend |
| [capi-alif](https://github.com/analogdevicesinc/capi-alif) | Alif platform backend |
| [capi-stm32](https://github.com/analogdevicesinc/capi-stm32) | STM32 platform backend |

Each package's own README lists the parts and boards it covers, and its
`PLATFORM` key is the one an application names under `depends.platform`.

## Using it

An application's own `module.yml` names the capi it was written against
and the platforms it can target:

```yaml
name: hello_capi
type: application
version: "0.9.0"
capi_version_requirement: ">=0.9.0"

depends:
  platform:
    maxim: {git: "https://github.com/analogdevicesinc/capi-maxim.git", checkout: main}
    alif:  {git: "https://github.com/analogdevicesinc/capi-alif.git", checkout: main}
```

Its `CMakeLists.txt` hands the rest to the SDK:

```cmake
include("${CAPI_DIR}/sdk/cmake/capi_create_application.cmake")
capi_resolve_platform()            # must precede project()
project(hello_capi LANGUAGES C ASM)
capi_create_application(hello_capi SOURCES src/main.c)
```

`PLATFORM` and `BOARD` come from the active CMakePresets.json preset, and
only the selected platform is resolved. See
[capi-hello-world](https://github.com/analogdevicesinc/capi-hello-world)
for a complete application, and [docs/module-yml.md](docs/module-yml.md)
for the manifest format.

## Versioning

Every module states which capi it is written against, as a version
constraint, and the build refuses to configure if the capi it resolves
does not satisfy it:

```yaml
capi_version_requirement: ">=0.9.0"
```

A constraint is one comparison (`>=`, `<=`, `=`, `>`, `<`) against
`X.Y.Z`, or a comma-joined range such as `">=0.9.0,<1.0.0"`. The same
grammar applies to a `depends` entry's `version:`; see
[docs/module-yml.md](docs/module-yml.md).

## License

BSD-3-Clause. See [LICENSE](LICENSE).
