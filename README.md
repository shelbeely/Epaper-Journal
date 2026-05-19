# Epaper Journal — Xteink X4 Firmware

[![Build Firmware](https://github.com/shelbeely/Epaper-Journal/actions/workflows/build.yml/badge.svg)](https://github.com/shelbeely/Epaper-Journal/actions/workflows/build.yml)

An Arduino/PlatformIO firmware for the **Xteink X4** e-paper device that turns it into a private, offline-first personal journal. Built on the [open-x4-epaper/community-sdk](https://github.com/open-x4-epaper/community-sdk).

---

## Features (by phase)

| Phase | Status | Description |
|---|---|---|
| **0 — Hardware Prototype** | 🚧 In progress | Display, buttons, SD card, Wi-Fi soft-AP, OTA scaffold |
| **1 — Plaintext Diary** | 📋 Planned | YAML-frontmatter Markdown entries, browse UI, export |
| **2 — Web Editor** | 📋 Planned | Soft-AP "Pocket Shrine" with in-browser Markdown editor |
| **3 — Journal UX** | 📋 Planned | Prompt packs, streak calendar, sleep screen |
| **4 — Privacy Mode** | 📋 Planned | AES-GCM encrypted vault via mbedTLS |
| **5 — Companion Tools** | 📋 Planned | Android PWA, GitHub backup, Obsidian export |

---

## Hardware

**Xteink X4** — ESP32-based e-paper device with SSD1677 display (800×480), ADC-multiplexed buttons, SD card slot, and LiPo battery.

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

---

## Quick start

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/shelbeely/Epaper-Journal.git
cd Epaper-Journal

# Build dev firmware
pio run -e dev

# Flash via USB
pio run -e dev --target upload

# Monitor serial output
pio device monitor --baud 115200
```

See **[docs/building.md](docs/building.md)** for the full build, flash, OTA, and API reference.

---

## Project structure

```
.
├── src/
│   ├── main.cpp                  # Phase 0 MVP entry point
│   ├── config.h                  # Hardware pin constants
│   ├── diagnostics/              # X4Log, X4Diagnostics
│   ├── display/                  # X4Display wrapper (EInkDisplay)
│   ├── input/                    # X4Input wrapper (InputManager)
│   ├── storage/                  # X4Storage wrapper (SDCardManager)
│   ├── ota/                      # OtaManager — pull OTA + rollback
│   └── web/                      # WebApi — HTTP diagnostics + display endpoints
├── open-x4-sdk/                  # community-sdk (git submodule)
├── platformio.ini                # PlatformIO config (dev + release envs)
├── partitions_ota.csv            # Dual OTA slots + FAT data partition
├── sdkconfig.defaults            # ESP-IDF Kconfig overrides
├── tools/                        # Agent build/OTA/health scripts
└── docs/building.md              # Full documentation
```

---

## Safety features

- **OTA rollback** — new firmware must pass a health-check gate before `esp_ota_mark_app_valid_cancel_rollback()` is called. Failed checks trigger automatic rollback.
- **Crash-loop detection** — NVS boot counter; ≥ 3 unclean boots activates Safe Mode.
- **Safe Mode** — power button held at boot skips all experimental features, exposes OTA recovery endpoint only.
- **No secrets in logs** — Wi-Fi credentials and tokens are never serialized to Serial or HTTP responses.

---

## Contributing

See [CONTRIBUTING](#) and the [PR template](.github/PULL_REQUEST_TEMPLATE.md).
Bug reports and feature requests: use the [issue templates](.github/ISSUE_TEMPLATE/).
