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
    bool begin(const char* ns, bool /*readOnly*/ = false) {
        _ns = ns ? ns : "";
        return true;
    }
    void     end() {}
    uint32_t getUInt(const char* key, uint32_t defaultVal = 0) {
        auto it = _uintStore.find(_scopedKey(key));
        return (it == _uintStore.end()) ? defaultVal : it->second;
    }
    bool putUInt(const char* key, uint32_t val) {
        _uintStore[_scopedKey(key)] = val;
        return true;
    }

    // Byte-array operations (used by VaultManager for salt storage)
    size_t getBytes(const char* key, void* out, size_t maxLen) {
        auto it = _bytesStore.find(_scopedKey(key));
        if (it == _bytesStore.end()) return 0;
        size_t n = std::min(maxLen, it->second.size());
        std::memcpy(out, it->second.data(), n);
        return n;
    }
    size_t putBytes(const char* key, const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        _bytesStore[_scopedKey(key)] = std::vector<uint8_t>(p, p + len);
        return len;
    }
    bool isKey(const char* key) {
        std::string scoped = _scopedKey(key);
        return _uintStore.count(scoped) > 0 || _bytesStore.count(scoped) > 0;
    }

    static void reset() {
        _uintStore.clear();
        _bytesStore.clear();
    }

private:
    std::string _scopedKey(const char* key) const {
        return _ns + ":" + (key ? key : "");
    }

    std::string _ns;

    // All instances in a test binary share one backing store so that
    // begin()/end()/re-open() behaves like NVS within the same test run.
    static std::map<std::string, uint32_t> _uintStore;
    static std::map<std::string, std::vector<uint8_t>> _bytesStore;
};

inline std::map<std::string, uint32_t> Preferences::_uintStore;
inline std::map<std::string, std::vector<uint8_t>> Preferences::_bytesStore;
