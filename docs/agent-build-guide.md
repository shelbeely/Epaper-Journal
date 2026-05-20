# Xteink X4 — AI Agent Build Guide

> **Audience**: This document is written for an AI coding agent starting from a fresh clone. Follow
> every section in order to produce a verified, flashable firmware binary for the Xteink X4 device.

---

## Table of Contents

1. [Hardware Identity](#1-hardware-identity)
2. [Toolchain Requirements](#2-toolchain-requirements)
3. [Repository Setup & Submodule Init](#3-repository-setup--submodule-init)
4. [Secrets & Credentials Injection](#4-secrets--credentials-injection)
5. [Build Environments](#5-build-environments)
6. [Known Hardware Quirks](#6-known-hardware-quirks)
7. [Flashing Procedure](#7-flashing-procedure)
8. [Post-Flash Verification](#8-post-flash-verification)
9. [Common Failure Modes](#9-common-failure-modes)
10. [Open Hardware Questions](#10-open-hardware-questions)

---

## 1. Hardware Identity

### MCU

| Property | Value | Notes |
|---|---|---|
| Chip family | ESP32 (classic dual-core Xtensa LX6) | `esp32dev` board target used until official manifest ships |
| Flash size | 4 MB | Inferred from `partitions_ota.csv` totalling 4 MB |
| PSRAM | Unknown / not assumed | No PSRAM usage in firmware; do not enable |
| Framework | Arduino (via arduino-esp32) | `espidf` also listed as supported in SDK `library.json` files |

> **Note**: There is no official PlatformIO board manifest for the Xteink X4 yet. The generic
> `esp32dev` target is used as a stand-in. When an official manifest is published, update
> `platformio.ini` → `board` and verify pin definitions still match.

### E-Paper Display

| Property | Value |
|---|---|
| Controller | SSD1677 |
| Panel | GDEQ0426T82 (4.26 inch) |
| Resolution | 800 × 480 pixels (X4 default) |
| Buffer size | 48,000 bytes (800 × 480 ÷ 8) |
| Dual-buffer RAM | 96 KB (default mode) |
| Single-buffer RAM | 48 KB (enable with `-DEINK_DISPLAY_SINGLE_BUFFER_MODE`) |
| Interface | Software SPI (non-default ESP32 pins) |

#### Display SPI Pins

| Signal | GPIO | Direction |
|---|---|---|
| SCLK | 8 | Output |
| MOSI | 10 | Output |
| CS | 21 | Output |
| DC (Data/Command) | 4 | Output |
| RST | 5 | Output |
| BUSY | 6 | Input |

> MISO is not connected to the display; `SPI.begin()` is called with MISO = -1.

#### Display vs X3 Panel Geometry

The `EInkDisplay` driver supports two panel sizes controlled by calling (or not calling)
`setDisplayX3()` **before** `begin()`:

| Panel | Resolution | Buffer size | `setDisplayX3()` |
|---|---|---|---|
| X4 (default) | 800 × 480 | 48,000 bytes | **Do not call** |
| X3 | 792 × 528 | 52,272 bytes | Call before `begin()` |

This firmware targets the **X4** panel. Do not call `setDisplayX3()`.

The maximum statically allocated buffer is `MAX_BUFFER_SIZE = 52,272` bytes to support both panels.

### Input Buttons

The InputManager reads ADC-multiplexed buttons on two GPIO pins plus one direct digital pin.

| Role | GPIO | Type |
|---|---|---|
| Button bank 1 (BACK, CONFIRM, LEFT, RIGHT) | 1 | ADC |
| Button bank 2 (UP, DOWN) | 2 | ADC |
| Power button | 3 | Digital (INPUT_PULLUP) |

ADC no-button threshold: `3900`. Debounce delay: `5 ms`.

#### Button Index Constants (`InputManager.h`)

| Constant | Index |
|---|---|
| `BTN_BACK` | 0 |
| `BTN_CONFIRM` | 1 |
| `BTN_LEFT` | 2 |
| `BTN_RIGHT` | 3 |
| `BTN_UP` | 4 |
| `BTN_DOWN` | 5 |
| `BTN_POWER` | 6 |

### SD Card

| Property | Value |
|---|---|
| Chip Select | GPIO 12 |
| SPI frequency | 40 MHz |
| SPI bus | Hardware SPI (SdFat default bus) |
| MISO / MOSI / SCK | **Undocumented** — see [Open Hardware Questions](#10-open-hardware-questions) |

> **Warning**: The EInkDisplay library calls `SPI.begin(SCLK=8, MISO=-1, MOSI=10, CS=21)`, which
> overrides the global Arduino `SPI` object's pin assignments. SDCardManager then calls
> `sd.begin(CS=12, 40 MHz)` against that same global `SPI` instance. Until the SD card MISO pin
> is confirmed and the SPI bus wiring is clarified, SD card functionality may not work reliably.
> Track this in [Open Hardware Questions](#10-open-hardware-questions).

### Battery Monitor

| Property | Value |
|---|---|
| ADC pin | **TBD** — currently `0` (disabled) |
| Voltage divider multiplier | 2.0× |

When `BATTERY_ADC_PIN == 0` the battery monitor is skipped entirely. The health-check treats
`batteryPercent == 0` as a failure, so OTA verify will fail if the battery pin is never set.
Set `BATTERY_ADC_PIN` in `src/config.h` when the schematic is confirmed.

### Partition Table (`partitions_ota.csv`)

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| nvs | data | nvs | 0x9000 | 0x5000 (20 KB) |
| otadata | data | ota | 0xE000 | 0x2000 (8 KB) |
| app0 | app | ota_0 | 0x10000 | 0x1E0000 (1920 KB) |
| app1 | app | ota_1 | 0x1F0000 | 0x1E0000 (1920 KB) |
| storage | data | fat | 0x3D0000 | 0x30000 (192 KB) |

Total flash consumed: 4 MB exactly. Use a 4 MB (or larger) flash chip. Do not use the default
Arduino partition tables — they lack dual OTA slots.

---

## 2. Toolchain Requirements

### Required tools

| Tool | Minimum version | Install command |
|---|---|---|
| Python | 3.8+ | System package manager or [python.org](https://python.org) |
| PlatformIO Core | 6.x | `pip install platformio` |
| Git | 2.x (with submodule support) | System package manager |

Verify:

```bash
python --version   # Python 3.x
pio --version      # PlatformIO Core, x.x.x
git --version      # git version 2.x.x
```

### PlatformIO platform and framework

PlatformIO auto-installs these on first build. The exact versions are resolved by PlatformIO's
dependency solver; no version pins are currently set in `platformio.ini`.

| Component | Identifier |
|---|---|
| Platform | `espressif32` |
| Framework | `arduino` |
| Board target | `esp32dev` |

### Third-party libraries (pinned in `platformio.ini`)

These are automatically downloaded by PlatformIO. The versions below are pinned in `lib_deps`
and must not be changed without testing:

| Library | Declared version | Notes |
|---|---|---|
| `greiman/SdFat` | `^2.3.1` | |
| `bblanchon/ArduinoJson` | `^7.0.0` | |
| `mathieucarbou/ESP Async WebServer` | `^3.0.0` | Use full owner prefix; bare name is ambiguous |
| `mathieucarbou/AsyncTCP` | `^3.0.0` | Matched to the WebServer fork above |

> **Important**: The library `ESP Async WebServer` exists under multiple owners in the PlatformIO
> registry (`mathieucarbou`, `me-no-dev`, `zeed`, etc.). Always use the full
> `mathieucarbou/ESP Async WebServer` form in `lib_deps`. Bare `ESP Async WebServer@^1.x.x`
> will fail to resolve and produce a build error.

### Verified resolved versions (from successful `pio run -e dev`)

| Library | Resolved version |
|---|---|
| `EInkDisplay` | 1.0.0 |
| `BatteryMonitor` | 1.0.0 |
| `InputManager` | 1.1.0 |
| `SDCardManager` | 2.0.0 |
| `SdFat` | 2.3.1 |
| `ArduinoJson` | 7.4.3 |
| `ESP Async WebServer` | 3.0.6 |
| `AsyncTCP` | 3.3.2 |

Expected build result: **RAM ~31% (101 KB / 328 KB), Flash ~59% (1.14 MB / 1.92 MB)**

**Expected warnings** (not errors):
- `FILE_READ` / `FILE_WRITE` redefined — harmless macro clash between SdFat and Arduino FS.h
- `beginResponse_P` deprecated — `src/web/WebApi.cpp` uses a deprecated-but-functional API in
  ESPAsyncWebServer 3.x; will be cleaned up in a future development phase

### Community SDK libraries (via git submodule)

These are provided as local symlinks into `open-x4-sdk/` and are **not** downloaded from the
PlatformIO registry. They must be present via the submodule (see next section).

| Library | Version | Path in SDK |
|---|---|---|
| `EInkDisplay` | 1.0.0 | `open-x4-sdk/libs/display/EInkDisplay` |
| `BatteryMonitor` | 1.0.0 | `open-x4-sdk/libs/hardware/BatteryMonitor` |
| `InputManager` | 1.1.0 | `open-x4-sdk/libs/hardware/InputManager` |
| `SDCardManager` | 2.0.0 | `open-x4-sdk/libs/hardware/SDCardManager` |

---

## 3. Repository Setup & Submodule Init

### Step 1 — Clone

```bash
git clone https://github.com/shelbeely/Epaper-Journal.git
cd Epaper-Journal
```

### Step 2 — Initialize the community SDK submodule

The community SDK is a git submodule pinned at a specific commit. **Always initialize it before
building.**

```bash
git submodule update --init --recursive
```

Expected output (no errors):

```
Submodule 'open-x4-sdk' (https://github.com/open-x4-epaper/community-sdk.git) registered for path 'open-x4-sdk'
Cloning into '.../open-x4-sdk'...
Submodule path 'open-x4-sdk': checked out '7d86603ad27709a9a766bb5ad893cfc39e60777e'
```

The locked commit SHA is **`7d86603`** (branch `main` of `open-x4-epaper/community-sdk`).

### Step 3 — Verify the submodule

```bash
git submodule status
# Expected: 7d86603... open-x4-sdk (heads/main)

ls open-x4-sdk/libs/display/EInkDisplay/include/EInkDisplay.h
# Must exist — if missing, re-run step 2
```

### Step 4 — Verify symlinks resolve

```bash
pio pkg list   # or: ls .pio/libdeps/ after first build
```

PlatformIO resolves `symlink://open-x4-sdk/libs/...` entries at build time; no manual symlink
creation is required.

---

## 4. Secrets & Credentials Injection

`src/config.h` ships with placeholder Wi-Fi credentials. Real credentials **must not be
committed**. Three safe methods are available.

### Option A — `src/secrets.h` (recommended for local development)

1. Create `src/secrets.h` (already gitignored):

   ```cpp
   #define WIFI_SSID     "MyNetwork"
   #define WIFI_PASSWORD "MyPassword"
   ```

2. In `src/config.h`, add:

   ```cpp
   #if __has_include("secrets.h")
   #  include "secrets.h"
   #endif
   ```

   (This inclusion is already documented in `docs/building.md`.)

### Option B — Build flags in `platformio.ini` (CI / agent builds)

Add to the target environment in `platformio.ini`:

```ini
build_flags =
    -DWIFI_SSID=\"MyNetwork\"
    -DWIFI_PASSWORD=\"MyPassword\"
```

### Option C — Environment variables injected at build time

Pass as shell environment before running `pio`:

```bash
PLATFORMIO_BUILD_FLAGS="-DWIFI_SSID=\"MyNetwork\" -DWIFI_PASSWORD=\"MyPassword\"" pio run -e dev
```

### Soft AP (Web Editor mode)

`SOFTAP_SSID` defaults to `"eJournal"` with an open password. Override in the same way if
needed. The captive portal gates access.

---

## 5. Build Environments

Two build environments are defined in `platformio.ini`:

| Environment | Command | Purpose |
|---|---|---|
| `dev` | `pio run -e dev` | Development; all diagnostics on |
| `release` | `pio run -e release` | Production; all diagnostics off |

### `CONFIG_X4_*` compile-time flags

| Flag | `dev` | `release` | Effect |
|---|---|---|---|
| `CONFIG_X4_DEV_DIAGNOSTICS` | 1 | 0 | Enables structured serial log markers (`[X4:…]`) |
| `CONFIG_X4_AGENT_DIAGNOSTICS` | 1 | 0 | Enables additional agent-targeted log output |
| `CONFIG_X4_VERBOSE_DISPLAY_DIAGNOSTICS` | 1 | 0 | Verbose SPI/display timing logs |
| `CONFIG_X4_DIAG_HTTP_API` | 1 | 0 | Enables `/api/dev/*` HTTP diagnostic endpoints |

### Firmware identity flags

| Flag | `dev` default | `release` default | Set by |
|---|---|---|---|
| `FIRMWARE_VERSION` | `"0.1.0-dev"` | `"0.1.0"` | Build flag / agent script |
| `FIRMWARE_CHANNEL` | `"dev"` | `"release"` | Build flag |
| `FIRMWARE_DEVICE` | `"xteink-x4"` | `"xteink-x4"` | Build flag |
| `FIRMWARE_COMMIT` | `"unknown"` | `"unknown"` | Injected by agent scripts only |

### Building (quick reference)

```bash
# Development build (manual)
pio run -e dev
# Output: .pio/build/dev/firmware.bin

# Development build with git commit hash (via agent script)
./tools/agent_build_dev.sh
# Output: .pio/build/dev/firmware.bin

# Release build
RELEASE_VERSION=0.1.0 ./tools/agent_build_release.sh
# Output: .pio/build/release/firmware.bin
```

### Single-buffer mode (required for this firmware)

`EINK_DISPLAY_SINGLE_BUFFER_MODE` is **always enabled** in both `dev` and `release` builds.

The dual-buffer mode allocates two 52,272-byte framebuffers (104 KB total). Combined with
Wi-Fi, AsyncTCP, and the TLS certificate bundle, this overflows the classic ESP32's ~328 KB
available DRAM. Single-buffer mode reduces display RAM to 52,272 bytes, bringing total RAM
usage to ~31% (~101 KB of 328 KB).

Do not remove `-DEINK_DISPLAY_SINGLE_BUFFER_MODE` from `build_flags` unless the RAM budget
is explicitly re-evaluated.

**Trade-off**: ~100 ms extra per fast refresh (extra RED RAM sync after each update).

---

## 6. Known Hardware Quirks

### 6.1 SPI bus — EPD and SD card share the global `SPI` object

`EInkDisplay::begin()` calls:

```cpp
SPI.begin(SCLK=8, MISO=-1, MOSI=10, CS=21);
```

This reassigns the global Arduino `SPI` bus to use non-default pins and disables MISO. The
`SDCardManager` then uses that same global `SPI` bus with CS=12. If the SD card MISO line is
wired to a GPIO, SdFat needs MISO configured — the current code does not set a MISO pin for the
SD card. This is an open gap; see [Section 10](#10-open-hardware-questions).

### 6.2 SPI clock speeds

| Peripheral | Clock | Notes |
|---|---|---|
| EPD (X4) | 40 MHz | In `EInkDisplay.cpp` (non-X3 mode) |
| EPD (X3) | 10 MHz | In `EInkDisplay.cpp` (X3 mode) |
| SD card | 40 MHz | In `SDCardManager.cpp` |
| SPI mode | Mode 0 (CPOL=0, CPHA=0) | MSB first |

### 6.3 Display refresh timings

| Refresh type | Duration | API call |
|---|---|---|
| Full refresh | ~1600 ms | `displayBuffer(FULL_REFRESH)` |
| Half refresh | ~1720 ms | `displayBuffer(HALF_REFRESH)` |
| Fast (custom LUT) | ~500 ms | `displayBuffer(FAST_REFRESH)` |
| Partial window | ~600 ms | `displayWindow(x, y, w, h)` |
| Power on | ~100 ms | After `begin()` |
| Power off | ~200 ms | After `deepSleep()` |
| Reset pulse (low) | 10 ms | During `resetDisplay()` |
| Software reset settle | 10 ms | After command 0x12 |

Always call `display.deepSleep()` before cutting power to lock the displayed image.

### 6.4 Display X coordinate alignment

**All X coordinates and widths passed to `displayWindow()` must be multiples of 8** (byte
boundary constraint of the SSD1677 RAM addressing).

### 6.5 Y-axis orientation reversal

The SSD1677 scans gates bottom-to-top. The driver compensates internally — application code
uses top-left-origin coordinates. Do not apply manual Y reversal.

### 6.6 Framebuffer bit encoding

| Bit value | Pixel colour |
|---|---|
| `1` (0xFF byte) | White |
| `0` (0x00 byte) | Black |

`clearScreen(0xFF)` → white. `clearScreen(0x00)` → black.

### 6.7 Grayscale rendering (4-level)

Grayscale requires two rendering passes and a custom 111-byte LUT:

1. **Pass 1 (BW)**: render to both BW RAM and RED RAM, perform a base refresh.
2. **Pass 2 (gray)**: write LSB buffer then MSB buffer via `copyGrayscaleLsbBuffers()` /
   `copyGrayscaleMsbBuffers()`, then call `displayGrayBuffer()`.

In single-buffer mode, save the BW buffer before pass 2 and restore it with
`cleanupGrayscaleBuffers()` after.

LUT voltage values used by the driver: `VGH=0x17, VSH1=0x41, VSH2=0xA8, VSL=0x32, VCOM=0x30`.

### 6.8 Safe mode and power-button pre-read

`X4Input::readPowerButtonGpio()` is called **before** `InputManager::begin()` in `setup()`. This
direct GPIO read uses `POWER_BUTTON_GPIO = 3`. If the power button is held at boot, safe mode
activates (skips storage init).

### 6.9 ADC button debounce

Debounce delay is `5 ms` (defined as `DEBOUNCE_DELAY` in `InputManager.cpp`). The ADC
no-button threshold is `3900`. Do not change these without re-tuning the ADC ranges.

### 6.10 Crash-loop detection (OTA rollback)

The OTA manager increments a boot counter in NVS (namespace `"x4sys"`, key `"bootcnt"`) on
every boot. If the counter reaches `CRASH_LOOP_THRESHOLD = 3` without a clean health-check
clearing it, `esp_ota_mark_app_invalid_rollback_and_reboot()` is called automatically.

### 6.11 Task watchdog

`sdkconfig.defaults` enables the task watchdog with a **10-second timeout**. Operations that
block the main task for more than 10 seconds (e.g. slow display full refreshes inside a tight
loop without `delay()`) will trigger a watchdog reset. The main loop stack is set to **16 KB**.

### 6.12 mbedTLS certificate bundle

OTA manifest fetches use HTTPS. The certificate bundle is compiled in
(`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y`). This increases binary size by ~130 KB.

---

## 7. Flashing Procedure

### 7.1 USB first-flash (required for initial load)

Connect the Xteink X4 via USB. Identify the serial port (e.g. `/dev/ttyUSB0` on Linux,
`COMx` on Windows, `/dev/cu.usbserial-*` on macOS).

```bash
# Flash dev build
pio run -e dev --target upload

# PlatformIO auto-detects the port. To specify manually:
pio run -e dev --target upload --upload-port /dev/ttyUSB0
```

Monitor serial output immediately after flash:

```bash
pio device monitor --baud 115200
```

### 7.2 OTA update (dev builds only, subsequent updates)

OTA is only available in `dev` builds (`CONFIG_X4_DIAG_HTTP_API=1`).

```bash
# Trigger OTA via the agent helper script
./tools/agent_ota_upload.sh <device-ip>

# Or trigger directly via HTTP
curl -X POST http://<device-ip>/api/dev/ota/apply
```

The device fetches the manifest from `OTA_MANIFEST_URL` (defined in `src/config.h`), downloads
the binary, and flashes it to the inactive OTA slot.

### 7.3 OTA rollback

```bash
curl -X POST http://<device-ip>/api/dev/ota/rollback
```

---

## 8. Post-Flash Verification

### 8.1 Expected serial log markers (dev build)

After a successful boot the following markers appear in order on the serial monitor:

```
[X4:BOOT_START]
[X4:DISPLAY_INIT_START]
[X4:DISPLAY_INIT_OK]
[X4:INPUT_OK]
[X4:STORAGE_OK]
[X4:WIFI_OK]
[X4:BOOT_OK]
```

Any `[X4:*_FAIL]` or `SAFE_MODE_ACTIVE` marker indicates a problem. Use the HTTP API to
retrieve diagnostics.

### 8.2 Agent health check

```bash
./tools/agent_healthcheck.sh <device-ip>
# Exits 0 if {"ok": true}, 1 on timeout or failure
```

Equivalent HTTP call:

```bash
curl http://<device-ip>/api/dev/health
# Expected: {"ok": true, "checks": {"display": true, "storage": true, "battery": true, "wifi": true}}
```

### 8.3 HTTP diagnostic API (dev builds only)

| Endpoint | Method | Description |
|---|---|---|
| `/api/dev/status` | GET | Full diagnostics JSON |
| `/api/dev/health` | GET | `{"ok": true/false, "checks": {...}}` |
| `/api/dev/logs` | GET | Ring-buffer of last 50 log lines |
| `/api/dev/ota` | GET | OTA slot info |
| `/api/dev/display` | GET | Display status |
| `/api/dev/ota/check` | POST | Fetch manifest, return result |
| `/api/dev/ota/apply` | POST | Download + flash OTA update |
| `/api/dev/ota/rollback` | POST | Roll back to previous slot |
| `/api/dev/reboot` | POST | Restart the device |

### 8.4 Always-available display API

| Endpoint | Method | Description |
|---|---|---|
| `/api/display/status` | GET | Display status JSON |
| `/api/display/screenshot.bmp` | GET | 1-bit BMP of current framebuffer |
| `/api/display/test-pattern` | POST | Render test pattern `{"pattern":"checkerboard"}` |
| `/api/display/refresh/full` | POST | Trigger full refresh |
| `/api/display/refresh/partial` | POST | Trigger partial refresh `{"x":0,"y":0,"w":100,"h":100}` |
| `/api/display/clear` | POST | Clear display |

### 8.5 OTA health-check flow (post-OTA verification)

After an OTA flash the new image starts in `PENDING_VERIFY` state. The firmware runs all health
checks on the first boot:

1. Display initialized (`gDiag.display.initialized == true`)
2. Storage ready (`gStorage.ready() == true`)
3. Battery non-zero (`gDiag.batteryPercent != 0`) — **will fail while `BATTERY_ADC_PIN == 0`**
4. Wi-Fi connected (`WiFi.status() == WL_CONNECTED`)

If all pass → `esp_ota_mark_app_valid_cancel_rollback()` is called.  
If any fail → `esp_ota_mark_app_invalid_rollback_and_reboot()` is called (automatic rollback).

---

## 9. Common Failure Modes

### 9.1 Build fails: "No such file or directory: EInkDisplay.h"

**Cause**: The git submodule was not initialized.

**Fix**:
```bash
git submodule update --init --recursive
ls open-x4-sdk/libs/display/EInkDisplay/include/EInkDisplay.h   # must exist
```

### 9.2 Build fails: "Board 'esp32dev' not found" or platform not installed

**Cause**: PlatformIO platform not yet installed in this environment.

**Fix**:
```bash
pio platform install espressif32
pio run -e dev
```

### 9.3 OTA health check fails immediately after flash

**Cause**: `BATTERY_ADC_PIN == 0` causes `batteryPercent == 0`, which the health check treats as
a failed battery reading.

**Fix**: If running without confirmed battery ADC wiring, temporarily bypass the battery check
in `runHealthChecks()` in `src/main.cpp`, or set `BATTERY_ADC_PIN` to the correct GPIO once
confirmed. Do **not** set it to a random GPIO — this will read floating ADC noise.

### 9.4 Wi-Fi health check fails

**Cause**: `WIFI_SSID` / `WIFI_PASSWORD` are still the placeholder values, or the network is
unreachable.

**Fix**: Inject real credentials via one of the three methods in [Section 4](#4-secrets--credentials-injection).

### 9.5 Crash loop detected on every boot

**Cause**: The boot counter in NVS reached `CRASH_LOOP_THRESHOLD = 3` (three boots without a
clean health check). Safe mode activates automatically.

**Fix**:
1. Hold the power button at boot to enter safe mode intentionally.
2. Fix the underlying health check failure.
3. Re-flash via USB or OTA; the boot counter resets on a clean boot.

To manually erase NVS and reset the counter:
```bash
pio run -e dev --target erase
pio run -e dev --target upload
```

### 9.6 Build fails: "Multiple libraries were found for …"

**Cause**: A system-wide or `.pio/` cached version of a library conflicts with the submodule
symlink.

**Fix**:
```bash
pio pkg purge   # clear cached packages
pio run -e dev
```

### 9.7 SD card not detected

**Cause**: Likely the SPI bus conflict described in [Section 6.1](#61-spi-bus--epd-and-sd-card-share-the-global-spi-object).

**Fix**: Until the SD card MISO pin is confirmed, test by calling
`SDCardManager::begin()` after `EInkDisplay::begin()` and checking `SdMan.ready()`.
If it returns false, the MISO pin may need to be set explicitly via `SdFat`'s `SdSpiConfig`
constructor. Track in [Section 10](#10-open-hardware-questions).

### 9.8 Display shows ghosting or partial refresh artifacts

**Cause**: A full refresh was not performed after power-on.

**Fix**: Always call `display.displayBuffer(FULL_REFRESH)` (or `gDisplay.fullRefresh()`) on the
first frame after `display.begin()`. Subsequent frames can use `FAST_REFRESH`.

### 9.9 "pio: command not found"

**Fix**:
```bash
pip install platformio
# or
python -m pip install platformio
# Add PlatformIO to PATH if needed: ~/.platformio/penv/bin/
```

### 9.10 Build fails: "ESP Async WebServer … Warning: Could not find the package"

**Cause**: The bare library name `ESP Async WebServer` is ambiguous — multiple packages match
it in the registry. PlatformIO picks none and the header cannot be found.

**Fix**: Ensure `platformio.ini` uses the full owner-qualified form:

```ini
lib_deps =
    mathieucarbou/ESP Async WebServer@^3.0.0
    mathieucarbou/AsyncTCP@^3.0.0
```

### 9.11 Build fails: "DRAM segment … overflowed by N bytes"

**Cause**: Dual-buffer display mode (default) allocates two 52 KB framebuffers. Combined with
Wi-Fi, TLS, and AsyncTCP, this exceeds the ~328 KB DRAM budget of the classic ESP32.

**Fix**: Ensure `-DEINK_DISPLAY_SINGLE_BUFFER_MODE` is present in `build_flags` for **both**
`dev` and `release` environments. This reduces display RAM by 52 KB and brings the firmware
within budget.

---

## 10. Open Hardware Questions

These items could not be confirmed from any publicly available schematic, community SDK source,
or GitHub repository at the time this guide was written. They must be resolved before firmware
development proceeds beyond Phase 0.

| # | Question | Impact | Where to update when resolved |
|---|---|---|---|
| 1 | **Battery ADC pin** — what GPIO is the battery voltage divider connected to? | `BATTERY_ADC_PIN=0` disables the monitor; OTA health check always fails on battery | `src/config.h` → `BATTERY_ADC_PIN` |
| 2 | **SD card MISO pin** — what GPIO is SD card MISO wired to? | SD card may not function correctly (SPI MISO disabled by EPD init) | `open-x4-sdk/libs/hardware/SDCardManager/src/SDCardManager.cpp` → configure `SdSpiConfig` with explicit MISO pin |
| 3 | **ESP32 chip variant** — classic dual-core ESP32, ESP32-S3, or ESP32-C3? | Wrong board target produces incorrect binaries; RAM and peripheral addresses differ | `platformio.ini` → `board` |
| 4 | **Official PlatformIO board manifest** — has one been published for the X4? | Official manifest provides correct flash size, CPU speed, and pin defaults | `platformio.ini` → `board` |
| 5 | **SD card SPI bus** — does the X4 wire the SD card to the same SPI bus as the EPD (pins 8/10) or to the ESP32 default VSPI (18/19/23)? | Determines whether `SPI.begin()` pin assignments need to be separated | `SDCardManager.cpp` → `SdSpiConfig` init |

To report confirmed answers, open a GitHub issue with the label `hardware-confirmation` or
submit a PR that updates `src/config.h`, `SDCardManager.cpp`, and this document.

---

## Appendix: Pin Summary

| Signal | GPIO | Source |
|---|---|---|
| EPD SCLK | 8 | `src/config.h`, `EInkDisplay/README.md` |
| EPD MOSI | 10 | `src/config.h`, `EInkDisplay/README.md` |
| EPD CS | 21 | `src/config.h`, `EInkDisplay/README.md` |
| EPD DC | 4 | `src/config.h`, `EInkDisplay/README.md` |
| EPD RST | 5 | `src/config.h`, `EInkDisplay/README.md` |
| EPD BUSY | 6 | `src/config.h`, `EInkDisplay/README.md` |
| Button ADC bank 1 | 1 | `InputManager.h` |
| Button ADC bank 2 | 2 | `InputManager.h` |
| Power button | 3 | `InputManager.h`, `src/config.h` |
| SD card CS | 12 | `SDCardManager.cpp` |
| Battery ADC | **TBD** | `src/config.h` (`BATTERY_ADC_PIN=0`) |
| SD card MISO | **TBD** | Undocumented |
| SD card MOSI | **TBD** | Undocumented |
| SD card SCK | **TBD** | Undocumented |
