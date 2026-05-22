#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// test/mocks/HTTPClient.h — stub for the Arduino ESP32 HTTPClient library
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "WiFiClientSecure.h"

inline int g_http_begin_call_count = 0;

inline void resetHTTPClientMock() {
    g_http_begin_call_count = 0;
}

class HTTPClient {
    WiFiClient _dummy;
public:
    void setTimeout(uint32_t /*ms*/) {}

    bool begin(WiFiClientSecure& /*client*/, const String& /*url*/) {
        g_http_begin_call_count++;
        return false;
    }
    bool begin(WiFiClientSecure& /*client*/, const char* /*url*/) {
        g_http_begin_call_count++;
        return false;
    }

    int    GET()            { return -1; }
    void   end()            {}
    String getString()      { return String(""); }
    bool   connected()      { return false; }

    WiFiClient* getStreamPtr() { return &_dummy; }
};
