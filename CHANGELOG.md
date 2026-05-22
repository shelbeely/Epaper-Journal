# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
### Added
- Added `tools/serve_ota_manifest.py` to host OTA `manifest.json` + `firmware.bin` locally with computed SHA-256, and documented manifest setup/placeholder behavior.
- Journal entry keyword search from the device browse menu and `/api/journal/search`.
- New entry title prompt screen with selectable date, daily-prompt, and untitled defaults before creating a journal entry.

- Wi-Fi provisioning fallback at `http://192.168.4.1/wifi-setup` that stores credentials in NVS and reboots the device.
- Added a persisted Wi-Fi toggle (`WIFI_AUTO_ON`, NVS `system/wifi_on`) with a Browse menu item to turn Wi-Fi ON/OFF.

### Changed
- Boot Wi-Fi setup now prefers saved NVS credentials over compile-time defaults when available.
- Wi-Fi now restores from persisted state on boot instead of always starting.
- Browse idle/power deep-sleep path now powers down Wi-Fi before entering sleep.
- Web API now returns `503 {"error":"wifi_disabled"}` when Wi-Fi is disabled.

### Fixed
- OTA health check now treats an active soft-AP interface as healthy even when no clients are connected, preventing false rollback requests after flashing.
- Journal entry saves are now atomic: writes go to a sibling `.tmp` file first and are renamed into place only after a successful write.
- On boot, stale orphaned journal `.tmp` files are cleaned up from month directories.

### Security
- Added NVS-backed vault PIN lockout tracking with escalating 30 second, 5 minute, and 1 hour back-off windows after repeated failed unlock attempts.
- `/api/vault/unlock` now returns HTTP 429 with `Retry-After` when the vault is temporarily locked and reports the remaining wait time.
- Increased vault PIN key-derivation cost to PBKDF2-SHA256 with 100,000 iterations for new vault ciphertext.
- Added legacy vault migration so `---vault-v1---` (10k-iteration) content is transparently re-encrypted as `---vault-v2---` after successful unlock.
- Vault unlock now uses a challenge-response flow: `GET /api/vault/challenge`
  returns a single-use 32-byte nonce; `POST /api/vault/unlock` accepts
  `{"response":"<hex(SHA256(nonce+pin))>"}` so the raw PIN is never sent over
  HTTP.  The server brute-forces the 4-digit space to match the response and
  then derives the key, keeping PBKDF2 as the key-stretching primitive.
- Web UI vault panel updated to perform the two-step unlock flow using the
  Web Crypto API (`crypto.subtle.digest`).
- Nonces are single-use and expire after 60 seconds.

## [0.1.0] - 2026-05-20

### Added
- First public firmware release for the Xteink X4 journal device.
- On-device journal browsing, entry creation, reading, and calendar navigation.
- Daily prompt pack support and sleep-screen journaling UX.
- Encrypted vault support with PIN-based unlock and AES-256-GCM storage.
- HTTP endpoints for display, journal, vault, export, and OTA workflows.
- OTA health checks, rollback handling, and safe-mode boot protections.

### Documentation
- Reworked the README with a clearer project overview, setup, release, and development guidance.
- Added this changelog to track future releases.
