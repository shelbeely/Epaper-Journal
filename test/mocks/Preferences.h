#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/Preferences.h — stub for ESP32 NVS Preferences library
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
public:
    bool     begin(const char* ns, bool /*readOnly*/ = false) {
        _namespace = ns ? ns : "";
        return true;
    }
    void     end() {}
    uint32_t getUInt(const char* key, uint32_t defaultVal = 0) {
        auto it = _store.find(_fullKey(key));
        if (it == _store.end() || it->second.size() != sizeof(uint32_t)) return defaultVal;
        uint32_t out = defaultVal;
        std::memcpy(&out, it->second.data(), sizeof(uint32_t));
        return out;
    }
    bool     putUInt(const char* key, uint32_t val) {
        std::vector<uint8_t> bytes(sizeof(uint32_t));
        std::memcpy(bytes.data(), &val, sizeof(uint32_t));
        _store[_fullKey(key)] = bytes;
        return true;
    }
    String getString(const char* key, const char* defaultVal = "") {
        auto it = _store.find(_fullKey(key));
        if (it == _store.end()) return String(defaultVal);
        return String(reinterpret_cast<const char*>(it->second.data()), it->second.size());
    }
    size_t putString(const char* key, const char* value) {
        const char* src = value ? value : "";
        size_t len = std::strlen(src);
        _store[_fullKey(key)] = std::vector<uint8_t>(src, src + len);
        return len;
    }
    size_t putString(const char* key, const String& value) {
        return putString(key, value.c_str());
    }
    static void clearStore() {
        _store.clear();
    }
    static void reset() { clearStore(); }

    // Byte-array operations (used by VaultManager for salt storage)
    size_t getBytes(const char* key, void* out, size_t maxLen) {
        auto it = _store.find(_fullKey(key));
        if (it == _store.end()) return 0;
        size_t n = std::min(maxLen, it->second.size());
        std::memcpy(out, it->second.data(), n);
        return n;
    }
    size_t putBytes(const char* key, const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        _store[_fullKey(key)] = std::vector<uint8_t>(p, p + len);
        return len;
    }
    bool isKey(const char* key) {
        return _store.count(_fullKey(key)) > 0;
    }

private:
    std::string _namespace;

    std::string _fullKey(const char* key) const {
        return _namespace + "::" + (key ? key : "");
    }

    // All instances in a test binary share one backing store so that
    // begin()/end()/re-open() behaves like NVS within the same test run.
    static std::map<std::string, std::vector<uint8_t>> _bytesStore;
    static std::map<std::string, uint32_t> _uintStore;
};

inline std::map<std::string, std::vector<uint8_t>> Preferences::_store;
