# Epaper Journal

[![Build Firmware](https://github.com/shelbeely/Epaper-Journal/actions/workflows/build.yml/badge.svg)](https://github.com/shelbeely/Epaper-Journal/actions/workflows/build.yml)

Epaper Journal is PlatformIO/Arduino firmware for the **Xteink X4** that turns the device into a private, offline-first journal with an e-paper reading experience, local storage, a web companion surface, OTA update support, and an optional encrypted vault.

## Release status

- Current release: **v0.1.0**
- Changelog: [CHANGELOG.md](CHANGELOG.md)
- Full build and flashing guide: [docs/building.md](docs/building.md)

## What v0.1.0 includes

- On-device journal browsing, reading, and new-entry creation
- Calendar navigation and built-in daily writing prompts
- Sleep-screen oriented journal UX for the X4 display
- Local SD-card backed journal storage
- Optional PIN-unlocked encrypted vault for private entries
- HTTP APIs for display, journal, vault, export, and OTA flows
- OTA health checks, rollback support, and safe-mode recovery behavior

## Hardware target

This project targets the **Xteink X4**, an ESP32-based e-paper device with:

- SSD1677 800×480 display
- ADC-multiplexed buttons
- SD card storage
- Wi-Fi connectivity
- LiPo battery support

### Pin reference

| Signal | GPIO |
|---|---|
| EPD SCLK | 8 |
| EPD MOSI | 10 |
| EPD CS | 21 |
| EPD DC | 4 |
| EPD RST | 5 |
| EPD BUSY | 6 |
| Button ADC 1 | 1 |
| Button ADC 2 | 2 |
| Power button | 3 |

## Quick start

```bash
git clone --recurse-submodules https://github.com/shelbeely/Epaper-Journal.git
cd Epaper-Journal

# Development build
pio run -e dev

# Flash over USB
pio run -e dev --target upload

# Monitor serial logs
pio device monitor --baud 115200
```

## Release build

```bash
pio run -e release

# or inject an explicit release version
RELEASE_VERSION=0.1.0 ./tools/agent_build_release.sh
```

## Development and testing

```bash
# Native unit tests
pio test -e native

# Firmware builds
pio run -e dev
pio run -e release
```

The repository uses:

- `src/` for firmware code
- `test/` for native unit tests
- `tools/` for helper build and OTA scripts
- `open-x4-sdk/` for the community SDK submodule

## Major components

- `src/journal/` — entry creation, storage layout, parsing, prompts
- `src/ui/` — browse, entry, calendar, sleep, and PIN screens
- `src/vault/` — encrypted vault handling
- `src/web/` — HTTP routes and bundled web UI
- `src/ota/` — OTA manifest handling, health checks, rollback flow
- `src/display/`, `src/input/`, `src/storage/` — hardware abstraction wrappers

## Safety and recovery

- OTA validation before marking a new image healthy
- Automatic rollback when health checks fail
- Crash-loop detection with safe-mode boot fallback
- Soft-AP availability for local access
- No Wi-Fi credentials or tokens intentionally exposed in logs or HTTP responses

## Project roadmap snapshot

| Phase | Status | Description |
|---|---|---|
| 0 — Hardware Prototype | ✅ Complete | Display, buttons, SD card, Wi-Fi soft-AP, OTA scaffold |
| 1 — Plaintext Diary | 🚧 In progress | YAML-frontmatter Markdown entries, browse UI, export |
| 2 — Web Editor | 🚧 In progress | Soft-AP web editing workflow |
| 3 — Journal UX | ✅ Complete | Prompt packs, streak calendar, sleep screen |
| 4 — Privacy Mode | ✅ Complete | AES-GCM encrypted vault, PIN entry, vault routes |
| 5 — Companion Tools | 🚧 In progress | PWA/export/backup follow-up work |

## Contributing

- Use the [pull request template](.github/PULL_REQUEST_TEMPLATE.md)
- Use the [issue templates](.github/ISSUE_TEMPLATE/)
- Update [CHANGELOG.md](CHANGELOG.md) for user-visible release changes
