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

### On-device Wi-Fi provisioning

`WIFI_PROVISIONING_ENABLED` defaults to `1` in `src/config.h`. On boot, the firmware:

1. Loads Wi-Fi credentials from NVS (`wifi_creds` namespace) if a saved SSID exists.
2. Falls back to the compile-time `WIFI_SSID` / `WIFI_PASSWORD` values when NVS is empty.
3. Attempts the STA connection for 10 seconds while the `eJournal` soft-AP remains available.

If the STA connection does not succeed within 10 seconds, browse to
`http://192.168.4.1/wifi-setup` while connected to the device soft-AP. Submit the form to save
new credentials into NVS and reboot the device. The saved credentials take priority on all future
boots until they are replaced by another provisioning flow.

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
RELEASE_VERSION=0.1.0 ./tools/agent_build_release.sh
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

### Always-available journal API (Phase 2)

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | eJournal SPA — in-browser Markdown journal editor |
| `/api/journal/entries` | GET | List entries for `?year=YYYY&month=MM` |
| `/api/journal/entry` | GET | Read raw Markdown entry at `?path=<path>` |
| `/api/journal/entry` | POST | Save entry — body: `{"path":"...","content":"..."}` |
| `/api/journal/entry` | DELETE | Delete entry at `?path=<path>` |
| `/api/journal/new` | POST | Create entry, return `{"path":"..."}` — body (optional): `{"title":"..."}` |

#### `POST /api/journal/new`

Creates a new journal entry on the SD card using the current timestamp as the
filename, then returns the full path so the browser can immediately open the editor.

```bash
# Create a new entry (no body required — defaults to "New Entry")
curl -X POST http://192.168.4.1/api/journal/new \
  -H "Content-Type: application/json" \
  -d '{"title":"My First Entry"}'
# → 201  {"path":"/journal/2026/05/20260520-103000.md"}
```

## Phase 3 — Journal UX features

### Writing prompt packs

Each day the browse screen header shows a different writing prompt, drawn from
`src/journal/PromptPack.h` — a header-only library of 30 embedded prompts.
The prompt rotates deterministically by day-of-year so it is consistent across
reboots and requires no RNG or storage.

```cpp
struct tm now = gClock.now();
const char* prompt = PromptPack::today(now);   // day-of-year rotation
const char* prompt = PromptPack::getPrompt(42); // by seed (wraps)
```

### Streak calendar (`[ STREAK ]`)

Select **[ STREAK ]** from the browse list to open the monthly calendar view.
Each day that has at least one journal entry is shown as a filled black square;
today's date is outlined. Navigate months with **Up** (prev) / **Down** (next)
and exit with **Back** or **Confirm**.

### Sleep screen

Pressing the **power button** or leaving the device idle for
`IDLE_SLEEP_TIMEOUT_MS` (default 5 minutes, configurable in `src/config.h`)
shows a brief "sleep screen" with the current date, time, and a "ZZZ" marker
before the device enters deep sleep. Set a custom timeout:

```ini
; platformio.ini build_flags
-DIDLE_SLEEP_TIMEOUT_MS=180000   ; 3 minutes
```

## Phase 4 — Privacy Mode / Encrypted Vault

Phase 4 adds AES-256-GCM encryption for journal entries via the ESP32's mbedTLS library.

### VaultManager (`src/vault/VaultManager.h`)

Key derivation uses **PBKDF2-HMAC-SHA256** (10 000 iterations) against a 16-byte random salt persisted in NVS under the `vault` namespace.

Encrypted file format stored on the SD card:
```
---vault-v1---
<base64(nonce[12] | tag[16] | ciphertext[N])>
```

The plaintext that is encrypted is the full YAML-frontmatter Markdown file.
`JournalManager` transparently encrypts on create/save and decrypts on load when the vault is unlocked.

### PIN entry (`src/ui/PinScreen.h`)

Select **[ UNLOCK VAULT ]** from the browse list to open the 4-digit PIN entry UI.

| Button | Action |
|---|---|
| Up / Down | Increment / decrement current digit (0–9, wraps) |
| Left / Right | Move cursor to previous / next digit |
| Confirm | Submit PIN → derive key → unlock vault |
| Back | Cancel |

Select **[ LOCK VAULT ]** to immediately zero the in-memory key.

### Vault HTTP API

Four endpoints are always available when the device is on Wi-Fi:

| Method | Path | Body | Description |
|---|---|---|---|
| `GET` | `/api/vault/status` | — | Returns vault lock state plus failed-attempt / lockout metadata. |
| `GET` | `/api/vault/challenge` | — | `{"nonce":"<64 hex chars>"}` — 32-byte single-use nonce (60 s TTL) |
| `POST` | `/api/vault/unlock` | `{"response":"<hex(SHA256(nonce+pin))>"}` | Challenge-response unlock; returns `{"ok": true}` or HTTP `429` with `Retry-After` while locked out. |
| `POST` | `/api/vault/lock` | — | Zero the in-memory key; returns `{"ok": true}` |

```bash
# Check vault state
curl http://192.168.4.1/api/vault/status

# Unlock (PIN "1234") using challenge-response:
NONCE=$(curl -s http://192.168.4.1/api/vault/challenge | python3 -c "import sys,json; print(json.load(sys.stdin)['nonce'])")
RESPONSE=$(python3 -c "
import hashlib, sys
nonce = bytes.fromhex('$NONCE')
pin   = b'1234'
print(hashlib.sha256(nonce + pin).hexdigest())
")
curl -X POST http://192.168.4.1/api/vault/unlock \
     -H 'Content-Type: application/json' \
     -d "{\"response\":\"$RESPONSE\"}"

# If the PIN has been tried too many times, the device responds with:
#   HTTP/1.1 429 Too Many Requests
#   Retry-After: 30
#   {"ok":false,"error":"vault_locked","retry_after":30,"failed_attempts":3}

# Lock
curl -X POST http://192.168.4.1/api/vault/lock
```

> **Security note:** The challenge-response flow ensures the raw PIN is never transmitted over HTTP. The server-side nonce is single-use and expires after 60 seconds, preventing replay attacks. Failed unlock attempts are persisted in NVS and escalate through 30 second, 5 minute, and 1 hour lockout windows.

### Locked entries in browse list

When the vault is locked, encrypted entries show `[LOCKED]` as their title in the browse list and cannot be opened or edited. They are safe to browse without revealing any content.

 (`data/index.html`) is gzip-compressed and embedded in
flash as a C byte array in `src/web/ui_bundle.h`.  The header is auto-generated
before each `dev` / `release` build via the PlatformIO `extra_scripts` hook:

```bash
# Regenerate manually after editing data/index.html:
python3 tools/embed_ui.py
```

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
