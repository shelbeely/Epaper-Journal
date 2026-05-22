# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- Retry NTP sync hourly while Wi-Fi is connected, persist the last-known-good epoch, and reuse it to keep journal entries visible after clock drift or failed sync.

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
