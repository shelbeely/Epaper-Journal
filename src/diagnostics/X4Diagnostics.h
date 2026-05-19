#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Diagnostics.h — runtime diagnostics struct + JSON serializer
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "../config.h"

struct X4DisplayStatus {
    const char* driverName   = "SSD1677";
    uint32_t    lastRefreshMs = 0;     // duration of last refresh (ms)
    const char* lastRefreshType = "none"; // "full", "fast", "partial", "gray"
    bool        initialized  = false;
    const char* lastError    = nullptr;
};

struct X4Diagnostics {
    // ── Firmware identity ──────────────────────────────────────────────────
    const char* firmwareVersion  = FIRMWARE_VERSION;
    const char* firmwareChannel  = FIRMWARE_CHANNEL;
    const char* firmwareDevice   = FIRMWARE_DEVICE;
    const char* firmwareCommit   = FIRMWARE_COMMIT;
    const char* buildTimestamp   = __DATE__ " " __TIME__;

    // ── OTA ───────────────────────────────────────────────────────────────
    bool   otaPendingVerify    = false;
    bool   otaRollbackAvailable = false;
    int    otaCurrentSlot      = -1;

    // ── Boot / crash-loop ─────────────────────────────────────────────────
    uint32_t bootCount         = 0;
    int      resetReason       = 0;   // esp_reset_reason_t cast to int

    // ── Heap ──────────────────────────────────────────────────────────────
    uint32_t freeHeap          = 0;
    uint32_t minFreeHeap       = 0;

    // ── Wi-Fi ─────────────────────────────────────────────────────────────
    // SSID intentionally NOT stored here to avoid accidental disclosure.
    String   ipAddress;
    int32_t  rssi              = 0;

    // ── Storage ───────────────────────────────────────────────────────────
    bool     storageReady      = false;
    uint64_t storageFreeBytesReserved = 0; // filled by X4Storage

    // ── Display ───────────────────────────────────────────────────────────
    X4DisplayStatus display;

    // ── Battery ───────────────────────────────────────────────────────────
    uint16_t batteryPercent    = 0;
    uint16_t batteryMv         = 0;

    // ── Safe mode ─────────────────────────────────────────────────────────
    bool     safeModeActive    = false;

    // ── Populate live fields ───────────────────────────────────────────────
    void refresh();

    // ── Serialize to JSON ─────────────────────────────────────────────────
    void toJson(JsonObject& obj) const;
};
