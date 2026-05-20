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

private:
    bool _synced = false;

    bool _isTimeSet() const;
    void _saveToNvs() const;
    void _loadFromNvs();
};
