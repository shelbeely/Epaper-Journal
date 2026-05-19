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

// Soft-AP SSID for Web Editor mode
#define SOFTAP_SSID     "Pocket Shrine"
#define SOFTAP_PASSWORD ""              // open AP — gate access via captive portal

// ── OTA manifest endpoint ─────────────────────────────────────────────────────
#define OTA_MANIFEST_URL "https://updates.example.com/x4/manifest.json"
#define OTA_CONNECT_TIMEOUT_MS 10000

// ── Crash-loop detection ──────────────────────────────────────────────────────
#define CRASH_LOOP_THRESHOLD  3     // boots without a clean health-check
#define NVS_NAMESPACE         "x4sys"
#define NVS_KEY_BOOT_COUNT    "bootcnt"

// ── Firmware identity (overridden by build flags) ────────────────────────────
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "0.0.0"
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
