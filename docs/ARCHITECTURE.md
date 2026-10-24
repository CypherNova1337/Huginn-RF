# Architecture

Huginn-RF is a small cooperative-multitasking app: one super-loop, a menu that
walks a registry of modules, and shared services (SD, config, logger, display,
input, radios) that modules borrow through a `Context`.

```
            +--------------------------------------------------+
  main.cpp  |  App: owns Context, drives the active screen     |
            +----------------------+---------------------------+
                                   |
                   Context  { Display, Input, SdCard,
                              Config, Logger, Lora, Gps }
                                   |
            +----------------------+---------------------------+
            |                 ModuleRegistry                   |
            |  [ wardrive ][ messenger ][ ir ][ wifi ] ...     |
            +--------------------------------------------------+
                   each Module: name(), category(),
                   enter(ctx), loop(ctx), exit(ctx), onKey()
```

## Core pieces (`src/core/`)

- **`Module`** — the interface every feature implements. A module gets the
  shared `Context` and runs its own screen loop until the user backs out.
- **`ModuleRegistry`** — modules self-register at startup (`REGISTER_MODULE`),
  grouped by `Category` (WiFi, Bluetooth, SubGHz, LoRa, IR, RFID, USB, Tools).
- **`Menu`** — renders categories → modules, routes keys, and hands control to
  the selected module's `enter/loop/exit`.
- **`Config`** — a small **YAML-lite** loader (see below) reading
  `/huginn/config.yaml`; exposes typed getters (`getString/getInt/getBool`) and
  loads auxiliary payload-list files on demand.
- **`SdCard`** — mounts the microSD, path helpers, capture-file rotation.
- **`Logger`** — timestamped (and GPS-stamped) log to serial + SD.
- **`Display` / `Input`** — thin wrappers over `M5GFX` and the 56-key keyboard.

## HAL (`src/hal/`)

- **`board.h`** — all pin maps for the ADV + LoRa cap in one place.
- **`Lora`** — RadioLib SX1262 wrapper: raw LoRa TX/RX, RSSI/SNR, and a
  promiscuous "listen" used by wardrive + messenger.
- **`Gps`** — ATGM336H reader (TinyGPSPlus) feeding a cached `Fix`.

## Modules (`src/modules/`)

Each module lives in its own folder with a `.h`/`.cpp` and a static registrar.
Benign modules are fully implemented; dual-use modules expose a complete
interface and leave the actual transmit/attack step as a marked hook.

## YAML-lite config

To avoid a heavy YAML dependency on a microcontroller, `Config` parses a
**practical subset** of YAML sufficient for this firmware:

- two-space-indented nested maps,
- `key: value` scalars (string/int/float/bool),
- `- item` sequences of scalars (for payload/target lists),
- `#` comments, blank lines.

It does **not** support flow syntax (`{}`/`[]`), anchors, or multi-line block
scalars. Anything richer (e.g. structured payload mutation profiles) is loaded
as a separate file the module parses itself. This keeps field-editing simple
(edit on a laptop, drop on the SD) while matching the "config, not firmware"
goal. See `sd_card/huginn/config.yaml` for the canonical example.

## Adding a module

1. `src/modules/foo/Foo.h` + `Foo.cpp` implementing `Module`.
2. Add `REGISTER_MODULE(FooModule, Category::Tools);` at file scope in the cpp.
3. `#include` it from `src/modules/modules.h` so the linker keeps the registrar.
4. Add any config keys under a `foo:` block in `config.yaml`.

That's it — it appears in the menu automatically.
