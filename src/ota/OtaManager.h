#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// OtaManager.h — pull-based OTA with SHA-256 verification and rollback support
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <esp_ota_ops.h>

// Result of an OTA manifest check
struct OtaManifest {
    bool   valid        = false;
    String device;
    String channel;
    String version;
    String url;
    String sha256;
    uint8_t minBatteryPercent = 20;
    String notes;
};

// Result of an OTA apply attempt
enum class OtaResult {
    OK,
    ALREADY_CURRENT,
    WRONG_DEVICE,
    WRONG_CHANNEL,
    LOW_BATTERY,
    INSUFFICIENT_SPACE,
    DOWNLOAD_FAILED,
    HASH_MISMATCH,
    FLASH_FAILED,
    MANIFEST_FETCH_FAILED,
    MANIFEST_PARSE_FAILED,
};

class OtaManager {
public:
    OtaManager();

    // ── Boot-time hooks ───────────────────────────────────────────────────

    // Call early in setup():
    //   - Increments NVS boot counter.
    //   - If counter >= CRASH_LOOP_THRESHOLD, returns true (→ safe mode).
    //   - If app is in PENDING_VERIFY state, emits X4M_OTA_PENDING_VERIFY.
    bool onBoot();

    // Call after all health checks pass to mark OTA valid and reset counter.
    void markBootHealthy();

    // Call to request a rollback (marks app invalid and reboots).
    void requestRollback();

    // ── OTA operations ────────────────────────────────────────────────────

    // Fetch + parse the manifest from OTA_MANIFEST_URL.
    OtaManifest fetchManifest();

    // Download, verify, and flash the firmware described by manifest.
    // batteryPercent is the current battery level for pre-flight check.
    OtaResult applyUpdate(const OtaManifest& manifest, uint16_t batteryPercent);

    // ── State queries ─────────────────────────────────────────────────────
    bool isPendingVerify() const;
    uint32_t bootCount()   const { return _bootCount; }

    static const char* resultString(OtaResult r);

private:
    uint32_t _bootCount = 0;
    bool _pendingVerify = false;

    // Compare semantic versions; returns true if `candidate` is newer than `current`.
    static bool isNewerVersion(const String& candidate, const String& current);
};
