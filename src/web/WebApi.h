#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// WebApi.h — HTTP API server
//
// Always-available display endpoints:  /api/display/*
// Always-available journal endpoints:  /api/journal/*
// Always-available vault endpoints:    /api/vault/*
// Dev-only endpoints:                  /api/dev/*  (CONFIG_X4_DIAG_HTTP_API)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../diagnostics/X4Diagnostics.h"
#include "../display/X4Display.h"
#include "../ota/OtaManager.h"
#include "../vault/VaultManager.h"

// Forward declaration — include JournalManager.h only in WebApi.cpp
class JournalManager;

// A minimal ring buffer for the last N log lines (dev builds only)
struct LogRingBuffer {
    static constexpr uint8_t CAPACITY = 50;
    String lines[CAPACITY];
    uint8_t head = 0;
    uint8_t count = 0;

    void push(const String& line) {
        lines[head] = line;
        head = (head + 1) % CAPACITY;
        if (count < CAPACITY) count++;
    }

    // Serialize to a JSON array string
    String toJson() const;
};

class WebApi {
public:
    // Pass references to shared objects; WebApi does not own them.
    WebApi(X4Diagnostics& diag, X4Display& display, OtaManager& ota,
           JournalManager& jm, VaultManager& vault);

    // Start the HTTP server (call after Wi-Fi is up).
    void begin();
    void setWifiProvisioningMode(bool enabled);

    // Push a line into the dev log ring buffer.
    void pushLog(const String& line);

private:
    AsyncWebServer _server{80};
    X4Diagnostics& _diag;
    X4Display&     _display;
    OtaManager&    _ota;
    JournalManager& _jm;
    VaultManager&  _vault;
    LogRingBuffer  _logs;
    bool           _wifiProvisioningActive = false;

    // Challenge-response nonce state (single-use, 60-second TTL).
    uint8_t  _challengeNonce[32] = {};
    uint32_t _challengeExpiry    = 0;   // millis() when nonce expires
    bool     _challengeActive    = false;

    void _registerDisplayRoutes();
    void _registerJournalRoutes();
    void _registerVaultRoutes();
    void _registerWifiProvisioningRoutes();
    void _registerExportRoutes();   // /api/export/* + /manifest.json + /sw.js

#if CONFIG_X4_DIAG_HTTP_API
    void _registerDevRoutes();
#endif
};
