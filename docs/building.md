# Building the Xteink X4 Journal Firmware

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (CLI) or [PlatformIO IDE](https://platformio.org/install/ide) (VS Code extension)
- Git (with submodule support)
- Python 3.x (used by agent scripts)

## First-time setup

```bash
# 1. Clone the repository
git clone https://github.com/shelbeely/Epaper-Journal.git
cd Epaper-Journal

# 2. Initialize the community-sdk submodule
git submodule update --init --recursive

# 3. (Optional) Verify the SDK is present
ls open-x4-sdk/libs/display/EInkDisplay/
```

## Wi-Fi credentials

The firmware reads `WIFI_SSID` and `WIFI_PASSWORD` from `src/config.h`.
**Do not commit real credentials.** Instead, create a `src/secrets.h` that is gitignored,
and add to `src/config.h`:

```cpp
#if __has_include("secrets.h")
#  include "secrets.h"
#endif
```

Or inject via `platformio.ini` `build_flags`:

```ini
build_flags =
  -DWIFI_SSID=\"MyNetwork\"
  -DWIFI_PASSWORD=\"MyPassword\"
```

## Building

### Development build (all diagnostics on)

```bash
pio run -e dev
# Output: .pio/build/dev/firmware.bin
```

Or use the agent script (injects git commit hash):

```bash
./tools/agent_build_dev.sh
```

### Release build (all debug gates closed)

```bash
pio run -e release
# Output: .pio/build/release/firmware.bin
```

Or:

```bash
RELEASE_VERSION=1.0.0 ./tools/agent_build_release.sh
```

## Flashing

### USB (first flash)

```bash
pio run -e dev --target upload
```

### OTA (subsequent updates — dev builds only)

```bash
# Trigger the device to fetch and apply the manifest configured in OTA_MANIFEST_URL:
./tools/agent_ota_upload.sh <device-ip>

# Or trigger an OTA apply via the HTTP API directly:
curl -X POST http://<device-ip>/api/dev/ota/apply
```

## Monitoring Serial output

```bash
pio device monitor --baud 115200
```

In dev builds, look for structured markers like:

```
[X4:BOOT_START]
[X4:DISPLAY_INIT_START]
[X4:DISPLAY_INIT_OK]
[X4:INPUT_OK]
[X4:STORAGE_OK]
[X4:WIFI_OK]
[X4:BOOT_OK]
```

## Agent health check

After flashing a dev build and connecting to Wi-Fi:

```bash
./tools/agent_healthcheck.sh <device-ip>
# Exits 0 if device reports {"ok": true}, 1 on timeout/failure
```

## Dev HTTP API (dev builds only)

| Endpoint | Method | Description |
|---|---|---|
| `/api/dev/status` | GET | Full diagnostics JSON |
| `/api/dev/health` | GET | `{"ok": true/false, "checks": {...}}` |
| `/api/dev/logs` | GET | Ring-buffer of last 50 log lines |
| `/api/dev/ota` | GET | OTA slot info |
| `/api/dev/display` | GET | Display status |
| `/api/dev/display` | GET | Display status |
| `/api/dev/ota/check` | POST | Fetch manifest, return result |
| `/api/dev/ota/apply` | POST | Download + flash OTA update |
| `/api/dev/ota/rollback` | POST | Roll back to previous slot |
| `/api/dev/reboot` | POST | Restart the device |

### Always-available display API

| Endpoint | Method | Description |
|---|---|---|
| `/api/display/status` | GET | Display status JSON |
| `/api/display/screenshot.bmp` | GET | 1-bit BMP of current framebuffer |
| `/api/display/test-pattern` | POST | Render test pattern `{"pattern":"checkerboard"}` |
| `/api/display/refresh/full` | POST | Trigger full refresh |
| `/api/display/refresh/partial` | POST | Trigger partial refresh `{"x":0,"y":0,"w":100,"h":100}` |
| `/api/display/clear` | POST | Clear display |

## community-sdk

The firmware depends on the [OpenX4 E-Paper Community SDK](https://github.com/open-x4-epaper/community-sdk)
as a git submodule at `open-x4-sdk/`.

Libraries used:

| Library | Path | Purpose |
|---|---|---|
| `EInkDisplay` | `open-x4-sdk/libs/display/EInkDisplay` | SSD1677 e-paper driver |
| `BatteryMonitor` | `open-x4-sdk/libs/hardware/BatteryMonitor` | ADC battery level |
| `InputManager` | `open-x4-sdk/libs/hardware/InputManager` | ADC-multiplexed buttons |
| `SDCardManager` | `open-x4-sdk/libs/hardware/SDCardManager` | SD card file system |

## Partition table

The custom partition table (`partitions_ota.csv`) provides:

- Two OTA app slots for safe over-the-air updates with automatic rollback
- A FAT data partition for journal storage

## OTA rollback

The firmware implements automatic rollback:

1. After flashing, the new image starts in **PENDING_VERIFY** state.
2. All health checks run (display, storage, battery, Wi-Fi).
3. If all pass, `esp_ota_mark_app_valid_cancel_rollback()` is called.
4. If any check fails or a crash-loop is detected (≥ 3 boots without a clean health check),
   `esp_ota_mark_app_invalid_rollback_and_reboot()` is called automatically.

## Hardware pins (Xteink X4)

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
| Battery ADC | TBD (set `BATTERY_ADC_PIN` in `config.h`) |
