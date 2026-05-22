# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Security
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
