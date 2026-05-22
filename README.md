# eJournal

> Offline-first, e-paper journal firmware for the Xteink X4.

[![Build Firmware](https://github.com/shelbeely/Epaper-Journal/actions/workflows/build.yml/badge.svg)](https://github.com/shelbeely/Epaper-Journal/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/shelbeely/Epaper-Journal)](https://github.com/shelbeely/Epaper-Journal/releases)

---

## Table of contents

1. [What is eJournal?](#what-is-ejournal)
2. [Hardware target](#hardware-target)
3. [Getting started](#getting-started)
4. [Configuration](#configuration)
5. [Feature overview](#feature-overview)
6. [Architecture](#architecture)
7. [Release history & roadmap](#release-history--roadmap)
8. [Security notes](#security-notes)
9. [Contributing](#contributing)
10. [License](#license)

**Quick links:** [CHANGELOG.md](CHANGELOG.md) · [docs/building.md](docs/building.md) · [docs/journal-format.md](docs/journal-format.md)

---

## What is eJournal?

eJournal turns the **Xteink X4** into a private, distraction-free writing device. There is no cloud sync, no account, and no screen that glows at you — just a slow, readable e-ink surface and a journal that lives entirely on the device's SD card.

Key capabilities:

- **Journal UX** — browse, read, and write entries directly on the device; daily writing prompts and a streak calendar built in
- **Encrypted vault** — optional AES-256-GCM vault unlocked by a 4-digit PIN; encrypted entries appear as `[LOCKED]` until unlocked
- **Web companion** — soft-AP web editor and HTTP API for editing entries, export, and OTA management from a browser
- **OTA & recovery** — over-the-air firmware updates with automatic rollback and crash-loop safe-mode

> 📸 _Use `python3 tools/serve_ui.py` to preview the web UI locally. See [Off-device UI preview](docs/building.md#off-device-ui-preview) in the building guide._

---

## Hardware target

This project targets the **Xteink X4**, an ESP32-based e-paper device with:

- SSD1677 800×480 e-paper display
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
| Battery ADC | set `BATTERY_ADC_PIN` in `src/config.h` |

The firmware depends on the [OpenX4 E-Paper Community SDK](https://github.com/open-x4-epaper/community-sdk) included as a git submodule at `open-x4-sdk/`. It provides the SSD1677 display driver, battery monitor, ADC button manager, and SD card manager.

---

## Getting started

### Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (CLI) or the PlatformIO IDE extension for VS Code
- Git (with submodule support)
- Python 3.x

### Clone and build

```bash
# 1. Clone with the community-sdk submodule
git clone --recurse-submodules https://github.com/shelbeely/Epaper-Journal.git
cd Epaper-Journal

# 2. Development build
pio run -e dev

# 3. Flash over USB (first time)
pio run -e dev --target upload

# 4. Monitor serial output
pio device monitor --baud 115200
```

After a successful boot, the serial monitor shows structured markers:

```
[X4:BOOT_START]
[X4:DISPLAY_INIT_OK]
[X4:STORAGE_OK]
[X4:WIFI_OK]
[X4:BOOT_OK]
```

### Run unit tests

```bash
pio test -e native
```

### Release build

```bash
pio run -e release
# or inject an explicit version string:
RELEASE_VERSION=0.1.0 ./tools/agent_build_release.sh
```

For the full guide — Wi-Fi credential setup, OTA flashing, all HTTP API reference tables, and community-SDK details — see **[docs/building.md](docs/building.md)**.

---

## Configuration

The most commonly changed tunables live in `src/config.h`. Override any `#ifndef`-guarded value via `platformio.ini` `build_flags` without editing the header directly.

| Define | Default | Description |
|---|---|---|
| `WIFI_SSID` | `"your-ssid"` | Station-mode SSID. Use a `src/secrets.h` (gitignored) or inject via build flags. |
| `WIFI_PASSWORD` | `"your-password"` | Station-mode password. Same approach as SSID. |
| `WIFI_AUTO_ON` | `1` | Default Wi-Fi state when no persisted preference exists (`1`=enabled, `0`=disabled). |
| `SOFTAP_SSID` | `"eJournal"` | Soft-AP network name for the web editor. |
| `IDLE_SLEEP_TIMEOUT_MS` | `300000` (5 min) | Milliseconds of inactivity before the sleep screen and deep sleep. |
| `BATTERY_ADC_PIN` | `0` (disabled) | ADC GPIO for the battery voltage divider. Set to `0` to disable monitoring. |
| `OTA_MANIFEST_URL` | placeholder URL | HTTPS URL of the OTA manifest JSON. |
| `CRASH_LOOP_THRESHOLD` | `3` | Number of failed-health-check boots before safe-mode is entered. |

Example override in `platformio.ini`:

```ini
build_flags =
  -DIDLE_SLEEP_TIMEOUT_MS=180000
  -DBATTERY_ADC_PIN=34
```

---

## Feature overview

### Journal UX

Entries are YAML-frontmatter Markdown files stored on the SD card under `/journal/YYYY/MM/`. The on-device UI lets you browse by date, read entries, create new ones, and navigate a monthly streak calendar showing days with at least one entry. A daily writing prompt (one of 30, selected by day-of-year) is shown in the browse screen header.

See the [Journal & UX API reference](docs/building.md#phase-3--journal-ux-features) in the building guide.

### Encrypted vault

The vault uses AES-256-GCM with a key derived from the PIN via PBKDF2-HMAC-SHA256 (10 000 iterations). The salt is stored in NVS; the key lives only in RAM and is zeroed on lock. Encrypted entries on the SD card use the `---vault-v1---` format header. `JournalManager` transparently encrypts on save and decrypts on load when the vault is unlocked.

See the [Vault API reference](docs/building.md#vault-http-api) in the building guide.

### Web companion

When connected to Wi-Fi (station mode) or via the soft-AP (`eJournal` network), the device serves a browser-based Markdown editor at `http://<device-ip>/` and a full HTTP API for journal, display, vault, export, and OTA operations. The web UI is gzip-compressed and embedded in flash as a C byte array; regenerate it with `python3 tools/embed_ui.py` after editing `data/index.html`.

See the [HTTP API reference](docs/building.md#dev-http-api-dev-builds-only) in the building guide.

### OTA & recovery

Firmware updates are fetched from `OTA_MANIFEST_URL`. After flashing, the image boots in `PENDING_VERIFY` state; health checks (display, storage, battery, Wi-Fi) must all pass before the image is marked valid. If any check fails, or if ≥ 3 consecutive boots occur without a clean health check, the device automatically rolls back to the previous OTA slot.

See the [OTA reference](docs/building.md#ota-rollback) in the building guide.

---

## Architecture

```
Epaper-Journal/
├── src/
│   ├── config.h              — Hardware pins & project-wide defaults
│   ├── main.cpp              — Boot sequence, task loop
│   ├── journal/              — Entry creation, storage layout, YAML parsing, prompts
│   ├── ui/                   — Browse, entry, calendar, sleep, and PIN screens
│   ├── vault/                — AES-GCM encrypted vault (VaultManager)
│   ├── web/                  — HTTP routes & embedded web UI bundle
│   ├── ota/                  — OTA manifest handling, health checks, rollback
│   ├── display/              — EInkDisplay hardware abstraction wrapper
│   ├── input/                — InputManager (ADC-multiplexed buttons) wrapper
│   └── storage/              — SDCardManager wrapper
├── test/                     — Native (host) unit tests (PlatformIO native env)
├── tools/                    — Build, OTA, health-check helper scripts
├── docs/                     — Deep-dive build & API reference
├── data/                     — Web UI source (index.html → embedded bundle)
├── open-x4-sdk/              — Community SDK git submodule (display, input, storage, battery)
├── platformio.ini            — Build environments: dev · release · native (test)
└── partitions_ota.csv        — Custom partition table (two OTA slots + FAT data)
```

The `open-x4-sdk/` submodule provides board-level drivers. Everything in `src/` builds on top of those drivers. Unit tests under `test/` run on the host (`native` env) using lightweight stubs in `test/mocks/`.

---

## Release history & roadmap

| Phase | Status | Description |
|---|---|---|
| 0 — Hardware Prototype | ✅ v0.1.0 | Display, buttons, SD card, Wi-Fi soft-AP, OTA scaffold |
| 1 — Plaintext Diary | ✅ v0.1.0 | YAML-frontmatter Markdown entries, browse UI, export |
| 2 — Web Editor | ✅ v0.1.0 | Soft-AP web editing workflow |
| 3 — Journal UX | ✅ v0.1.0 | Prompt packs, streak calendar, sleep screen |
| 4 — Privacy Mode | ✅ v0.1.0 | AES-GCM encrypted vault, PIN entry, vault routes |
| 5 — Companion Tools | 🚧 In progress | PWA follow-up work; web ZIP backup export is now available |

Full release notes: [CHANGELOG.md](CHANGELOG.md)

---

## Security notes

**PIN over plain HTTP.** The vault unlock PIN is sent as JSON over plain HTTP on the soft-AP network. Do not reuse this PIN for anything sensitive, and treat the `eJournal` soft-AP as untrusted if other devices are in range.

**OTA rollback.** A flashed image is not marked healthy until all boot-time health checks pass. If a bad image ships, the device automatically reverts to the previous slot. A manual rollback is also available via `POST /api/dev/ota/rollback` (dev builds) or by holding the power button during boot to enter safe-mode.

**No credentials in logs.** Wi-Fi credentials and vault PINs are never written to serial logs or included in HTTP responses.

---

## Contributing

- Open a PR using the [pull request template](.github/PULL_REQUEST_TEMPLATE.md)
- File bugs and feature requests using the [issue templates](.github/ISSUE_TEMPLATE/)
- **Branch naming:** `feature/<short-description>` or `fix/<short-description>`
- **Tests:** run `pio test -e native` before pushing; all tests must pass
- **Changelog:** add a line under `[Unreleased]` in [CHANGELOG.md](CHANGELOG.md) for every user-visible change
- **Build:** verify `pio run -e dev` and `pio run -e release` both succeed

---

## License

MIT License — see [LICENSE](LICENSE)
