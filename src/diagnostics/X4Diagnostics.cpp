// ─────────────────────────────────────────────────────────────────────────────
// X4Diagnostics.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Diagnostics.h"
#include <WiFi.h>

void X4Diagnostics::refresh() {
    freeHeap    = ESP.getFreeHeap();
    minFreeHeap = ESP.getMinFreeHeap();
    resetReason = static_cast<int>(esp_reset_reason());
    ipAddress   = WiFi.localIP().toString();
    rssi        = WiFi.RSSI();

    // OTA slot info
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        otaCurrentSlot = (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) ? 0 : 1;
    }
    const esp_partition_t* nextUpdate = esp_ota_get_next_update_partition(nullptr);
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running) {
        esp_ota_get_state_partition(running, &state);
    }
    otaPendingVerify    = (state == ESP_OTA_IMG_PENDING_VERIFY);
    otaRollbackAvailable = (nextUpdate != nullptr);
}

void X4Diagnostics::toJson(JsonObject& obj) const {
    // firmware
    JsonObject fw = obj["firmware"].to<JsonObject>();
    fw["version"]   = firmwareVersion;
    fw["channel"]   = firmwareChannel;
    fw["device"]    = firmwareDevice;
    fw["commit"]    = firmwareCommit;
    fw["built"]     = buildTimestamp;

    // ota
    JsonObject ota = obj["ota"].to<JsonObject>();
    ota["pendingVerify"]     = otaPendingVerify;
    ota["rollbackAvailable"] = otaRollbackAvailable;
    ota["currentSlot"]       = otaCurrentSlot;

    // boot
    JsonObject boot = obj["boot"].to<JsonObject>();
    boot["count"]       = bootCount;
    boot["resetReason"] = resetReason;
    boot["safeMode"]    = safeModeActive;

    // heap
    JsonObject heap = obj["heap"].to<JsonObject>();
    heap["free"]    = freeHeap;
    heap["minFree"] = minFreeHeap;

    // wifi — SSID deliberately omitted to prevent credential leakage
    JsonObject wifi = obj["wifi"].to<JsonObject>();
    wifi["ip"]   = ipAddress;
    wifi["rssi"] = rssi;

    // storage
    JsonObject storage = obj["storage"].to<JsonObject>();
    storage["ready"] = storageReady;

    // display
    JsonObject disp = obj["display"].to<JsonObject>();
    disp["driver"]          = display.driverName;
    disp["initialized"]     = display.initialized;
    disp["lastRefreshMs"]   = display.lastRefreshMs;
    disp["lastRefreshType"] = display.lastRefreshType;
    if (display.lastError) {
        disp["lastError"] = display.lastError;
    }

    // battery
    JsonObject bat = obj["battery"].to<JsonObject>();
    bat["percent"] = batteryPercent;
    bat["mv"]      = batteryMv;
}
