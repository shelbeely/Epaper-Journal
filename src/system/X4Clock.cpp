// ─────────────────────────────────────────────────────────────────────────────
// X4Clock.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Clock.h"
#include <Preferences.h>
#include <WiFi.h>
#include "../config.h"
#include "../diagnostics/X4Log.h"

static constexpr const char* CLOCK_NAMESPACE = "system";
static constexpr const char* NVS_KEY_EPOCH = "last_epoch";
static constexpr uint32_t MIN_REASONABLE_EPOCH = 1704067200UL;  // 2024-01-01 UTC
static constexpr uint32_t SYNC_INTERVAL_MS = 3600000UL;

X4Clock::X4Clock() {}

bool X4Clock::sync(uint32_t timeoutMs) {
    _hasSyncAttempt = true;
    _lastSyncAttemptMs = millis();
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    uint32_t start = millis();
    while (!_isTimeSet() && (millis() - start) < timeoutMs) {
        delay(100);
    }
    _synced = _isTimeSet();
    if (_synced) {
        uint32_t epoch = (uint32_t)time(nullptr);
        _lastSyncSuccessMs = millis();
        _saveToNvs(epoch);
        X4_LOG("CLOCK_SYNCED");
    } else {
        _loadFromNvs(true);
        X4_LOG("CLOCK_NTP_FAILED");
    }
    return _synced;
}

bool X4Clock::syncIfNeeded(uint32_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) return false;
    uint32_t lastReferenceMs = _synced ? _lastSyncSuccessMs : _lastSyncAttemptMs;
    if (_hasSyncAttempt && (millis() - lastReferenceMs) < SYNC_INTERVAL_MS) {
        return false;
    }
    return sync(timeoutMs);
}

struct tm X4Clock::now() const {
    time_t t = time(nullptr);
    struct tm info;
    localtime_r(&t, &info);
    return info;
}

String X4Clock::nowIso() const {
    struct tm t = now();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    return String(buf);
}

String X4Clock::nowFilestamp() const {
    struct tm t = now();
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    return String(buf);
}

void X4Clock::currentYearMonth(uint16_t& year, uint8_t& month) const {
    struct tm t = now();
    year  = (uint16_t)(t.tm_year + 1900);
    month = (uint8_t)(t.tm_mon + 1);
}

uint16_t X4Clock::effectiveCurrentYear() {
    uint16_t year;
    uint8_t month;
    currentYearMonth(year, month);

    uint16_t persistedYear = _persistedYear();
    return persistedYear > year ? persistedYear : year;
}

// ── Private helpers ──────────────────────────────────────────────────────────

bool X4Clock::_isTimeSet() const {
    return time(nullptr) >= (time_t)MIN_REASONABLE_EPOCH;
}

void X4Clock::_saveToNvs(uint32_t epoch) {
    Preferences prefs;
    prefs.begin(CLOCK_NAMESPACE, false);
    prefs.putUInt(NVS_KEY_EPOCH, epoch);
    prefs.end();
    _persistedEpoch = epoch;
    _persistedEpochLoaded = true;
}

bool X4Clock::_loadFromNvs(bool applyToClock) {
    if (_persistedEpochLoaded && _persistedEpoch >= MIN_REASONABLE_EPOCH) {
        if (applyToClock) {
            struct timeval tv = { (time_t)_persistedEpoch, 0 };
            settimeofday(&tv, nullptr);
            X4_LOGF("CLOCK_NVS_RESTORE", "epoch=%u", _persistedEpoch);
        }
        return true;
    }

    Preferences prefs;
    prefs.begin(CLOCK_NAMESPACE, true);
    uint32_t savedEpoch = prefs.getUInt(NVS_KEY_EPOCH, 0);
    prefs.end();

    _persistedEpoch = savedEpoch;
    _persistedEpochLoaded = true;

    if (savedEpoch >= MIN_REASONABLE_EPOCH) {
        if (applyToClock) {
            _synced = false;
            _lastSyncSuccessMs = 0;
            struct timeval tv = { (time_t)savedEpoch, 0 };
            settimeofday(&tv, nullptr);
            X4_LOGF("CLOCK_NVS_RESTORE", "epoch=%u", savedEpoch);
        }
        return true;
    }

    return false;
}

uint16_t X4Clock::_persistedYear() {
    if (!_loadFromNvs(false)) return 0;

    time_t t = (time_t)_persistedEpoch;
    struct tm info;
    localtime_r(&t, &info);
    return (uint16_t)(info.tm_year + 1900);
}
