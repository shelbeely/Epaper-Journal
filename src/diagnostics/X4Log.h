#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Log.h — structured serial log markers
//
// Usage:
//   X4_LOG("BOOT_START");
//   X4_LOGF("HEAP", "free=%u", ESP.getFreeHeap());
//
// All macros compile to nothing when CONFIG_X4_DEV_DIAGNOSTICS == 0.
// ─────────────────────────────────────────────────────────────────────────────

#if CONFIG_X4_DEV_DIAGNOSTICS
  #include <Arduino.h>
  #define X4_LOG(marker)          Serial.println("[X4:" marker "]")
  #define X4_LOGF(marker, fmt, ...) \
      do { Serial.print("[X4:" marker "] "); Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)
#else
  #define X4_LOG(marker)            ((void)0)
  #define X4_LOGF(marker, fmt, ...) ((void)0)
#endif

#if CONFIG_X4_VERBOSE_DISPLAY_DIAGNOSTICS
  #define X4_DLOG(marker)          X4_LOG(marker)
  #define X4_DLOGF(marker, fmt, ...) X4_LOGF(marker, fmt, ##__VA_ARGS__)
#else
  #define X4_DLOG(marker)            ((void)0)
  #define X4_DLOGF(marker, fmt, ...) ((void)0)
#endif

// ── Marker constants ──────────────────────────────────────────────────────────
// System / boot
#define X4M_BOOT_START              "BOOT_START"
#define X4M_BOOT_OK                 "BOOT_OK"
// Wi-Fi
#define X4M_WIFI_OK                 "WIFI_OK"
#define X4M_WIFI_FAILED             "WIFI_FAILED"
// OTA
#define X4M_OTA_PENDING_VERIFY      "OTA_PENDING_VERIFY"
#define X4M_OTA_MARK_VALID          "OTA_MARK_VALID"
#define X4M_OTA_ROLLBACK_REQUESTED  "OTA_ROLLBACK_REQUESTED"
// Display
#define X4M_DISPLAY_INIT_OK         "DISPLAY_INIT_OK"
#define X4M_DISPLAY_INIT_FAILED     "DISPLAY_INIT_FAILED"
#define X4M_DISPLAY_REFRESH_OK      "DISPLAY_REFRESH_OK"
#define X4M_DISPLAY_BUSY_TIMEOUT    "DISPLAY_BUSY_TIMEOUT"
// Input
#define X4M_INPUT_OK                "INPUT_OK"
#define X4M_INPUT_FAILED            "INPUT_FAILED"
// Health
#define X4M_HEALTH_OK               "HEALTH_OK"
#define X4M_HEALTH_FAILED           "HEALTH_FAILED"
// Storage
#define X4M_STORAGE_OK              "STORAGE_OK"
#define X4M_STORAGE_FAILED          "STORAGE_FAILED"
// Verbose display markers (emitted only when CONFIG_X4_VERBOSE_DISPLAY_DIAGNOSTICS)
#define X4M_DISPLAY_INIT_START          "DISPLAY_INIT_START"
#define X4M_DISPLAY_FRAMEBUFFER_ALLOC   "DISPLAY_FRAMEBUFFER_ALLOC_OK"
#define X4M_DISPLAY_FULL_REFRESH_START  "DISPLAY_FULL_REFRESH_START"
#define X4M_DISPLAY_PARTIAL_REFRESH_START "DISPLAY_PARTIAL_REFRESH_START"
#define X4M_DISPLAY_SLEEP_OK            "DISPLAY_SLEEP_OK"
#define X4M_DISPLAY_WAKE_OK             "DISPLAY_WAKE_OK"
#define X4M_DISPLAY_SPI_ERROR           "DISPLAY_SPI_ERROR"
