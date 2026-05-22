#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// config.h — Xteink X4 hardware pin constants & project-wide defaults
// ─────────────────────────────────────────────────────────────────────────────

// ── E-Paper display (SSD1677 via software SPI) ───────────────────────────────
#define EPD_SCLK  8   // SPI Clock
#define EPD_MOSI  10  // SPI MOSI (Master Out Slave In)
#define EPD_CS    21  // Chip Select
#define EPD_DC    4   // Data/Command
#define EPD_RST   5   // Reset
#define EPD_BUSY  6   // Busy

// ── Button ADC pins (InputManager) ───────────────────────────────────────────
// Resolved automatically by InputManager using InputManager::BUTTON_ADC_PIN_1 etc.
// Mirrored here for safe-mode direct GPIO read (before InputManager::begin()).
#define POWER_BUTTON_GPIO  3   // Maps to InputManager::POWER_BUTTON_PIN

// ── Battery ADC ───────────────────────────────────────────────────────────────
// Pin TBD — update when official hardware schematic is published.
// A value of 0 disables the battery monitor.
#define BATTERY_ADC_PIN    0
#define BATTERY_DIVIDER    2.0f

// ── Wi-Fi credentials ─────────────────────────────────────────────────────────
// Store actual credentials in a local secrets.h (gitignored) or
// inject via build flags. These are safe placeholder values only.
#ifndef WIFI_SSID
  #define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "your-password"
#endif
#ifndef WIFI_PROVISIONING_ENABLED
  #define WIFI_PROVISIONING_ENABLED 1
#endif

// Soft-AP SSID for Web Editor mode
#define SOFTAP_SSID     "eJournal"
#define SOFTAP_PASSWORD ""              // open AP — gate access via captive portal

// ── OTA manifest endpoint ─────────────────────────────────────────────────────
#define OTA_MANIFEST_URL "https://updates.example.com/x4/manifest.json"
#define OTA_CONNECT_TIMEOUT_MS 10000

// ── Crash-loop detection ──────────────────────────────────────────────────────
#define CRASH_LOOP_THRESHOLD  3     // boots without a clean health-check
#define NVS_NAMESPACE         "x4sys"
#define NVS_KEY_BOOT_COUNT    "bootcnt"

// ── Phase 3 — Journal UX ─────────────────────────────────────────────────────
// Idle timeout before the sleep screen is shown (milliseconds).
// Default: 5 minutes. Override via build flags if needed.
#ifndef IDLE_SLEEP_TIMEOUT_MS
  #define IDLE_SLEEP_TIMEOUT_MS 300000UL  // 5 minutes
#endif

// ── E-Ink display quality ─────────────────────────────────────────────────────
// After this many consecutive fast refreshes, X4Display::fastRefresh()
// automatically performs a full refresh instead to clear ghosting artifacts.
// Increase the value for snappier navigation (more ghosting risk) or decrease
// for cleaner images (occasional slow refresh during heavy navigation).
// Set to 0 to disable auto-ghosting cleanup entirely.
#ifndef GHOSTING_FULL_REFRESH_INTERVAL
  #define GHOSTING_FULL_REFRESH_INTERVAL 8
#endif

// ── Firmware identity (overridden by build flags) ────────────────────────────
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "0.1.0"
#endif
#ifndef FIRMWARE_CHANNEL
  #define FIRMWARE_CHANNEL "dev"
#endif
#ifndef FIRMWARE_DEVICE
  #define FIRMWARE_DEVICE  "xteink-x4"
#endif
#ifndef FIRMWARE_COMMIT
  #define FIRMWARE_COMMIT  "unknown"
#endif
