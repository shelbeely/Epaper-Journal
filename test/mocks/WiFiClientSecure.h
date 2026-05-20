#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/WiFiClientSecure.h — stub for ESP32 WiFiClient / WiFiClientSecure
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstddef>

class WiFiClient {
public:
    virtual int    available()                          { return 0; }
    virtual int    readBytes(uint8_t* /*buf*/, size_t /*len*/) { return 0; }
    virtual ~WiFiClient() = default;
};

class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {}
};
