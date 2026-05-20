// ─────────────────────────────────────────────────────────────────────────────
// X4Clock.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "X4Clock.h"
#include <Preferences.h>
#include "../config.h"
#include "../diagnostics/X4Log.h"

// NVS keys (re-use the x4sys namespace from config.h)
static constexpr const char* NVS_KEY_EPOCH = "epoch";

X4Clock::X4Clock() {}

bool X4Clock::sync(uint32_t timeoutMs) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    uint32_t start = millis();
    while (!_isTimeSet() && (millis() - start) < timeoutMs) {
        delay(100);
    }
    _synced = _isTimeSet();
    if (_synced) {
        _saveToNvs();
        X4_LOG("CLOCK_SYNCED");
    } else {
        _loadFromNvs();
        X4_LOG("CLOCK_NTP_FAILED");
    }
    return _synced;
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

// ── Private helpers ──────────────────────────────────────────────────────────

bool X4Clock::_isTimeSet() const {
    // time() returns seconds since epoch; consider set if > year 2020
    return time(nullptr) > 1577836800L; // 2020-01-01 00:00:00 UTC
}

void X4Clock::_saveToNvs() const {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUInt(NVS_KEY_EPOCH, (uint32_t)time(nullptr));
    prefs.end();
}

void X4Clock::_loadFromNvs() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    uint32_t savedEpoch = prefs.getUInt(NVS_KEY_EPOCH, 0);
    prefs.end();

    if (savedEpoch > 1577836800UL) {
        struct timeval tv = { (time_t)savedEpoch, 0 };
        settimeofday(&tv, nullptr);
        X4_LOGF("CLOCK_NVS_RESTORE", "epoch=%u", savedEpoch);
    }
}
