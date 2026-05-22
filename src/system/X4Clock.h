#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// X4Clock.h — Wall-clock time via SNTP with NVS fallback
//
// Call sync() once after Wi-Fi is up to set time via NTP.
// If NTP fails, the last persisted time is loaded from NVS.
// now() always returns the best available time.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <time.h>

class X4Clock {
public:
    X4Clock();

    // Attempt SNTP time sync. Returns true if time was set from NTP.
    // On failure, falls back to last NVS-persisted time.
    bool sync(uint32_t timeoutMs = 6000);

    // Retry SNTP only when Wi-Fi is connected and the hourly sync window has
    // elapsed since the last attempt.
    bool syncIfNeeded(uint32_t timeoutMs = 6000);

    // True if time has been synced from NTP at least once this boot.
    bool synced() const { return _synced; }

    // Current time as broken-down struct tm (UTC).
    struct tm now() const;

    // "YYYY-MM-DD HH:MM:SS" (for frontmatter / display).
    String nowIso() const;

    // "YYYYMMDD-HHMMSS" (for filenames).
    String nowFilestamp() const;

    // Populate year and month from current time.
    void currentYearMonth(uint16_t& year, uint8_t& month) const;

    // Current year, but never older than the last persisted good epoch.
    uint16_t effectiveCurrentYear();

private:
    bool _synced = false;
    bool _hasSyncAttempt = false;
    uint32_t _lastSyncAttemptMs = 0;
    uint32_t _lastSyncSuccessMs = 0;
    uint32_t _persistedEpoch = 0;
    bool _persistedEpochLoaded = false;

    bool _isTimeSet() const;
    void _saveToNvs(uint32_t epoch);
    bool _loadFromNvs(bool applyToClock);
    uint16_t _persistedYear();
};
