#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/Preferences.h — stub for ESP32 NVS Preferences library
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
public:
    bool     begin(const char* /*ns*/, bool /*readOnly*/ = false) { return true; }
    void     end() {}
    uint32_t getUInt(const char* /*key*/, uint32_t defaultVal = 0) {
        return defaultVal;
    }
    bool     putUInt(const char* /*key*/, uint32_t /*val*/) { return true; }

    // Byte-array operations (used by VaultManager for salt storage)
    size_t getBytes(const char* key, void* out, size_t maxLen) {
        auto it = _store.find(key ? key : "");
        if (it == _store.end()) return 0;
        size_t n = std::min(maxLen, it->second.size());
        std::memcpy(out, it->second.data(), n);
        return n;
    }
    size_t putBytes(const char* key, const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        _store[key ? key : ""] = std::vector<uint8_t>(p, p + len);
        return len;
    }
    bool isKey(const char* key) {
        return _store.count(key ? key : "") > 0;
    }

private:
    // All instances in a test binary share one backing store so that
    // begin()/end()/re-open() behaves like NVS within the same test run.
    static std::map<std::string, std::vector<uint8_t>> _store;
};

inline std::map<std::string, std::vector<uint8_t>> Preferences::_store;

