#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/Preferences.h — stub for ESP32 NVS Preferences library
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>

class Preferences {
public:
    bool     begin(const char* /*ns*/, bool /*readOnly*/ = false) { return true; }
    void     end() {}
    uint32_t getUInt(const char* /*key*/, uint32_t defaultVal = 0) {
        return defaultVal;
    }
    bool     putUInt(const char* /*key*/, uint32_t /*val*/) { return true; }
};
