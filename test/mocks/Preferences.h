#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/Preferences.h — stub for ESP32 NVS Preferences library
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
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
        auto it = _uintStore.find(_qualifyKey(key));
        return (it != _uintStore.end()) ? it->second : defaultVal;
    }
    bool     putUInt(const char* key, uint32_t val) {
        _uintStore[_qualifyKey(key)] = val;
        return true;
    }

    // Byte-array operations (used by VaultManager for salt storage)
    size_t getBytes(const char* key, void* out, size_t maxLen) {
        auto it = _bytesStore.find(_qualifyKey(key));
        if (it == _bytesStore.end()) return 0;
        size_t n = std::min(maxLen, it->second.size());
        std::memcpy(out, it->second.data(), n);
        return n;
    }
    size_t putBytes(const char* key, const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        _bytesStore[_qualifyKey(key)] = std::vector<uint8_t>(p, p + len);
        return len;
    }
    bool isKey(const char* key) {
        const std::string qualified = _qualifyKey(key);
        return _bytesStore.count(qualified) > 0 || _uintStore.count(qualified) > 0;
    }

    static void clear() {
        _bytesStore.clear();
        _uintStore.clear();
    }

private:
    std::string _qualifyKey(const char* key) const {
        return _namespace + ":" + (key ? key : "");
    }

    std::string _namespace;

    // All instances in a test binary share one backing store so that
    // begin()/end()/re-open() behaves like NVS within the same test run.
    static std::map<std::string, std::vector<uint8_t>> _bytesStore;
    static std::map<std::string, uint32_t> _uintStore;
};

inline std::map<std::string, std::vector<uint8_t>> Preferences::_bytesStore;
inline std::map<std::string, uint32_t> Preferences::_uintStore;
