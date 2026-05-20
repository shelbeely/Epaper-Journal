#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/HTTPClient.h — stub for the Arduino ESP32 HTTPClient library
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "WiFiClientSecure.h"

class HTTPClient {
    WiFiClient _dummy;
public:
    void setTimeout(uint32_t /*ms*/) {}

    bool begin(WiFiClientSecure& /*client*/, const String& /*url*/) {
        return false;
    }
    bool begin(WiFiClientSecure& /*client*/, const char* /*url*/) {
        return false;
    }

    int    GET()            { return -1; }
    void   end()            {}
    String getString()      { return String(""); }
    bool   connected()      { return false; }

    WiFiClient* getStreamPtr() { return &_dummy; }
};
