// ─────────────────────────────────────────────────────────────────────────────
// WebApi.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "WebApi.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../config.h"
#include "../journal/JournalManager.h"
#include "../journal/JournalFrontmatter.h"
#include "../wifi/WifiProvisioning.h"
#include "ui_bundle.h"
#include "mbedtls/sha256.h"
#include <esp_random.h>

static bool rejectWhenWifiDisabled(AsyncWebServerRequest* req) {
    if (WiFi.getMode() == WIFI_OFF) {
        req->send(503, "application/json", "{\"error\":\"wifi_disabled\"}");
        return true;
    }
    return false;
}

// ── LogRingBuffer ─────────────────────────────────────────────────────────────

static const char WIFI_SETUP_HTML[] PROGMEM = R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Wi-Fi setup</title>
  <style>
    :root { color-scheme: light; }
    body {
      margin: 0;
      font-family: Arial, sans-serif;
      background: #f5f0e8;
      color: #2f241d;
      display: flex;
      min-height: 100vh;
      align-items: center;
      justify-content: center;
      padding: 24px;
    }
    main {
      width: min(100%, 420px);
      background: #fffdf8;
      border: 1px solid #d8cfc2;
      border-radius: 16px;
      padding: 24px;
      box-shadow: 0 12px 32px rgba(47, 36, 29, 0.12);
    }
    h1 { margin-top: 0; font-size: 1.8rem; }
    p { line-height: 1.5; }
    label { display: block; margin-top: 16px; font-weight: 600; }
    input {
      width: 100%;
      margin-top: 8px;
      padding: 12px;
      border-radius: 10px;
      border: 1px solid #b9ad9e;
      box-sizing: border-box;
      font-size: 1rem;
    }
    button {
      width: 100%;
      margin-top: 20px;
      padding: 12px;
      border: 0;
      border-radius: 999px;
      background: #2f241d;
      color: #fffdf8;
      font-size: 1rem;
      font-weight: 700;
      cursor: pointer;
    }
    .note { font-size: 0.95rem; color: #5a4a3f; }
  </style>
</head>
<body>
  <main>
    <h1>Set up Wi-Fi</h1>
    <p>Connect the journal to your local Wi-Fi network. The new credentials are saved on the device and used after reboot.</p>
    <form method="POST" action="/api/wifi/config">
      <label for="ssid">Wi-Fi SSID</label>
      <input id="ssid" name="ssid" maxlength="32" autocomplete="off" required>
      <label for="password">Wi-Fi password</label>
      <input id="password" name="password" type="password" maxlength="63" autocomplete="new-password">
      <button type="submit">Save and reboot</button>
    </form>
    <p class="note">If your network is open, leave the password blank.</p>
  </main>
</body>
</html>
)html";

static const char WIFI_SETUP_CONFIRMATION_HTML[] PROGMEM = R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Wi-Fi saved</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 24px;
      background: #f5f0e8;
      color: #2f241d;
      font-family: Arial, sans-serif;
      text-align: center;
    }
    main {
      width: min(100%, 420px);
      background: #fffdf8;
      border: 1px solid #d8cfc2;
      border-radius: 16px;
      padding: 28px;
      box-shadow: 0 12px 32px rgba(47, 36, 29, 0.12);
    }
    h1 { margin-top: 0; }
    p { line-height: 1.5; }
  </style>
</head>
<body>
  <main>
    <h1>Wi-Fi updated</h1>
    <p>The new network settings were saved successfully.</p>
    <p>The journal is rebooting now.</p>
  </main>
</body>
</html>
)html";

static const char WIFI_SETUP_ERROR_HTML[] PROGMEM = R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Wi-Fi setup error</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 24px;
      background: #f5f0e8;
      color: #2f241d;
      font-family: Arial, sans-serif;
      text-align: center;
    }
    main {
      width: min(100%, 420px);
      background: #fffdf8;
      border: 1px solid #d8cfc2;
      border-radius: 16px;
      padding: 28px;
      box-shadow: 0 12px 32px rgba(47, 36, 29, 0.12);
    }
    a { color: #2f241d; font-weight: 700; }
  </style>
</head>
<body>
  <main>
    <h1>Unable to save Wi-Fi settings</h1>
    <p>Check the network name and try again.</p>
    <p><a href="/wifi-setup">Return to setup</a></p>
  </main>
</body>
</html>
)html";

static TaskHandle_t sWifiRebootTask = nullptr;

static void rebootAfterProvisioning(void*) {
    delay(1500);
    ESP.restart();
    vTaskDelete(nullptr);
}

static void scheduleProvisioningReboot() {
    if (sWifiRebootTask != nullptr) return;
    xTaskCreate(rebootAfterProvisioning, "wifi-reboot", 2048, nullptr, 1, &sWifiRebootTask);
}

String LogRingBuffer::toJson() const {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
        arr.add(lines[(head - count + i + CAPACITY) % CAPACITY]);
    }
    String out;
    serializeJson(doc, out);
    return out;
}

// ── WebApi ────────────────────────────────────────────────────────────────────

WebApi::WebApi(X4Diagnostics& diag, X4Display& display, OtaManager& ota,
               JournalManager& jm, VaultManager& vault)
    : _diag(diag), _display(display), _ota(ota), _jm(jm), _vault(vault)
{}

void WebApi::begin() {
    _registerDisplayRoutes();
    _registerJournalRoutes();
    _registerVaultRoutes();
    _registerWifiProvisioningRoutes();
    _registerExportRoutes();
#if CONFIG_X4_DIAG_HTTP_API
    _registerDevRoutes();
#endif
    // Root — serve the eJournal SPA (gzip-compressed, generated by tools/embed_ui.py)
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (_wifiProvisioningActive) {
            req->send(200, "text/html", String(FPSTR(WIFI_SETUP_HTML)));
            return;
        }
        if (rejectWhenWifiDisabled(req)) return;
        AsyncWebServerResponse* resp = req->beginResponse_P(
            200, "text/html", UI_HTML_GZ, UI_HTML_GZ_LEN);
        resp->addHeader("Content-Encoding", "gzip");
        req->send(resp);
    });
    _server.onNotFound([this](AsyncWebServerRequest* req) {
        if (_wifiProvisioningActive) {
            req->redirect("/wifi-setup");
            return;
        }
        req->send(404, "text/plain", "not found");
    });
    _server.begin();
}

void WebApi::setWifiProvisioningMode(bool enabled) {
    _wifiProvisioningActive = enabled;
}

void WebApi::pushLog(const String& line) {
    _logs.push(line);
}

// ── /api/display/* — always available ────────────────────────────────────────

void WebApi::_registerDisplayRoutes() {
    // GET /api/display/status
    _server.on("/api/display/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _diag.refresh();
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        JsonObject disp = obj["display"].to<JsonObject>();
        const X4DisplayStatus& s = _display.status();
        disp["driver"]          = s.driverName;
        disp["initialized"]     = s.initialized;
        disp["lastRefreshMs"]   = s.lastRefreshMs;
        disp["lastRefreshType"] = s.lastRefreshType;
        if (s.lastError) disp["lastError"] = s.lastError;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/display/screenshot.bmp — 1-bit BMP from framebuffer
    _server.on("/api/display/screenshot.bmp", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        uint16_t w = _display.width();
        uint16_t h = _display.height();
        uint16_t rowBytes = (w + 7) / 8;
        // BMP file: 14-byte file header + 40-byte DIB header
        uint32_t dataSize   = rowBytes * h;
        uint32_t fileSize   = 54 + 8 + dataSize; // +8 for 2-color palette
        uint8_t* bmp = (uint8_t*)malloc(fileSize);
        if (!bmp) { req->send(500, "text/plain", "OOM"); return; }
        memset(bmp, 0, fileSize);

        // File header
        bmp[0] = 'B'; bmp[1] = 'M';
        bmp[2] = fileSize & 0xFF; bmp[3] = (fileSize >> 8) & 0xFF;
        bmp[4] = (fileSize >> 16) & 0xFF; bmp[5] = (fileSize >> 24) & 0xFF;
        bmp[10] = 62; // pixel data offset = 54 + 8

        // DIB header (BITMAPINFOHEADER)
        bmp[14] = 40;
        bmp[18] = w & 0xFF; bmp[19] = (w >> 8) & 0xFF;
        // BMP height is negative for top-down
        int32_t negH = -(int32_t)h;
        memcpy(&bmp[22], &negH, 4);
        bmp[26] = 1; bmp[27] = 0;  // color planes
        bmp[28] = 1; bmp[29] = 0;  // bits per pixel
        bmp[34] = dataSize & 0xFF; bmp[35] = (dataSize >> 8) & 0xFF;

        // Color table: index 0 = black, index 1 = white
        bmp[54] = 0x00; bmp[55] = 0x00; bmp[56] = 0x00; bmp[57] = 0x00;
        bmp[58] = 0xFF; bmp[59] = 0xFF; bmp[60] = 0xFF; bmp[61] = 0x00;

        // Pixel data — framebuffer is already 1bpp, 1=white 0=black
        const uint8_t* fb = _display.getFrameBuffer();
        memcpy(&bmp[62], fb, dataSize);

        AsyncWebServerResponse* resp = req->beginResponse_P(
            200, "image/bmp", bmp, fileSize);
        req->send(resp);
        free(bmp);
    });

    // POST /api/display/test-pattern  body: {"pattern":"checkerboard"}
    _server.on("/api/display/test-pattern", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (rejectWhenWifiDisabled(req)) return;
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(400, "text/plain", "bad json");
                return;
            }
            const char* pattern = doc["pattern"] | "checkerboard";
            bool ok = _display.renderTestPattern(pattern);
            req->send(ok ? 200 : 404, "text/plain", ok ? "ok" : "unknown pattern");
        }
    );

    // POST /api/display/refresh/full
    _server.on("/api/display/refresh/full", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _display.fullRefresh();
        req->send(200, "text/plain", "ok");
    });

    // POST /api/display/refresh/partial  body: {"x":0,"y":0,"w":100,"h":100}
    _server.on("/api/display/refresh/partial", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (rejectWhenWifiDisabled(req)) return;
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(400, "text/plain", "bad json");
                return;
            }
            uint16_t x = doc["x"] | 0;
            uint16_t y = doc["y"] | 0;
            uint16_t w = doc["w"] | 100;
            uint16_t h = doc["h"] | 100;
            _display.partialRefresh(x, y, w, h);
            req->send(200, "text/plain", "ok");
        }
    );

    // POST /api/display/clear
    _server.on("/api/display/clear", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _display.clear();
        req->send(200, "text/plain", "ok");
    });
}

// ── /api/journal/* ────────────────────────────────────────────────────────────

void WebApi::_registerJournalRoutes() {
    // GET /api/journal/entries?year=YYYY&month=MM
    _server.on("/api/journal/entries", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        uint16_t year  = req->hasParam("year")  ? (uint16_t)req->getParam("year")->value().toInt()  : 0;
        uint8_t  month = req->hasParam("month") ? (uint8_t) req->getParam("month")->value().toInt() : 0;
        auto paths = _jm.listEntries(year, month);
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (auto& path : paths) {
            JournalEntry e;
            if (_jm.loadEntry(path, e)) {
                JsonObject obj = arr.add<JsonObject>();
                obj["path"]  = path;
                obj["title"] = e.title;
                obj["date"]  = e.date;
            }
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/journal/search?q=<keyword>
    _server.on("/api/journal/search", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->hasParam("q")) {
            req->send(400, "text/plain", "missing q");
            return;
        }

        String query = req->getParam("q")->value();
        query.trim();
        if (query.isEmpty()) {
            req->send(400, "text/plain", "missing q");
            return;
        }

        auto paths = _jm.searchEntries(query);
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& path : paths) {
            JournalEntry e;
            bool loaded = _jm.loadEntry(path, e);

            JsonObject obj = arr.add<JsonObject>();
            obj["path"] = path;
            if (loaded && !e.locked) {
                obj["title"] = e.title;
                obj["date"]  = e.date;
            } else {
                String label = JournalManager::labelFromFilename(path);
                obj["title"] = label;
                obj["date"]  = label;
            }
        }

        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/journal/entry?path=<path>
    _server.on("/api/journal/entry", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        if (!req->hasParam("path")) {
            req->send(400, "text/plain", "missing path");
            return;
        }
        String path = req->getParam("path")->value();
        String content = _jm.readEntryRaw(path);
        if (content.isEmpty()) {
            req->send(404, "text/plain", "not found");
            return;
        }
        req->send(200, "text/markdown", content);
    });

    // POST /api/journal/entry  body: {"path":"...","content":"..."}
    _server.on("/api/journal/entry", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (rejectWhenWifiDisabled(req)) return;
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(400, "text/plain", "bad json");
                return;
            }
            String path    = doc["path"].as<String>();
            String content = doc["content"].as<String>();
            if (path.isEmpty() || content.isEmpty()) {
                req->send(400, "text/plain", "missing path or content");
                return;
            }
            JournalEntry e;
            JournalFrontmatter::parse(content, e);
            bool ok = _jm.saveEntry(path, e);
            req->send(ok ? 200 : 500, "text/plain", ok ? "ok" : "write failed");
        }
    );

    // DELETE /api/journal/entry?path=<path>
    _server.on("/api/journal/entry", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        if (!req->hasParam("path")) {
            req->send(400, "text/plain", "missing path");
            return;
        }
        String path = req->getParam("path")->value();
        bool ok = _jm.deleteEntry(path);
        req->send(ok ? 200 : 404, "text/plain", ok ? "ok" : "not found or delete failed");
    });

    // POST /api/journal/new  body (optional): {"title":"..."}
    // Creates a new entry and returns {"path":"..."}.
    _server.on("/api/journal/new", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (rejectWhenWifiDisabled(req)) return;
            String title = "New Entry";
            if (len > 0) {
                JsonDocument doc;
                if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
                    const char* t = doc["title"];
                    if (t && t[0] != '\0') title = t;
                }
            }
            String path = _jm.createEntry(title);
            if (path.isEmpty()) {
                req->send(503, "text/plain", "storage unavailable");
                return;
            }
            JsonDocument resp;
            resp["path"] = path;
            String out;
            serializeJson(resp, out);
            req->send(201, "application/json", out);
        }
    );
}

// ── Vault routes ──────────────────────────────────────────────────────────────
//   GET  /api/vault/status     — {"locked": bool}
//   GET  /api/vault/challenge  — {"nonce":"<64 hex chars>"} (32-byte random)
//   POST /api/vault/unlock     — body {"response":"<hex(SHA256(nonce+pin))>"}
//   POST /api/vault/lock       — {} → {"ok": true}

void WebApi::_registerVaultRoutes() {
    // GET /api/vault/status
    _server.on("/api/vault/status", HTTP_GET,
               [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        JsonDocument doc;
        doc["locked"] = !_vault.isUnlocked();
        doc["unlock_locked_out"] = _vault.isUnlockLockedOut();
        doc["failed_attempts"] = _vault.failedUnlockAttempts();
        if (_vault.isUnlockLockedOut()) {
            doc["error"] = "vault_locked";
            doc["retry_after"] = _vault.unlockRetryAfterSeconds();
        }
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // GET /api/vault/challenge — generate a single-use 32-byte nonce (60s TTL)
    _server.on("/api/vault/challenge", HTTP_GET,
               [this](AsyncWebServerRequest* req) {
        esp_fill_random(_challengeNonce, sizeof(_challengeNonce));
        _challengeExpiry = (uint32_t)millis() + 60000UL;
        _challengeActive = true;

        char hexBuf[65];
        for (int i = 0; i < 32; i++) {
            snprintf(&hexBuf[i * 2], 3, "%02x", _challengeNonce[i]);
        }
        hexBuf[64] = '\0';

        String body = String("{\"nonce\":\"") + hexBuf + "\"}";
        req->send(200, "application/json", body);
    });

    // POST /api/vault/lock
    _server.on("/api/vault/lock", HTTP_POST,
               [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _vault.lock();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // POST /api/vault/unlock — body: {"response":"<hex(SHA256(nonce+pin))>"}
    // The raw PIN never travels in the request; the server brute-forces the
    // 4-digit space (10 000 SHA-256 ops) to recover the PIN and derive the key.
    _server.on("/api/vault/unlock", HTTP_POST,
               [](AsyncWebServerRequest* req) { /* body handled below */ },
               nullptr,
               [this](AsyncWebServerRequest* req,
                      uint8_t* data, size_t len, size_t, size_t) {
        if (rejectWhenWifiDisabled(req)) return;
        // 1. Validate and consume the nonce immediately (single-use).
        if (!_challengeActive ||
            (uint32_t)millis() > _challengeExpiry) {
            _challengeActive = false;
            req->send(400, "application/json",
                      "{\"ok\":false,\"error\":\"no active challenge\"}");
            return;
        }
        _challengeActive = false;   // consume — one-time use

        // 2. Parse the JSON body.
        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
            req->send(400, "application/json",
                      "{\"ok\":false,\"error\":\"invalid JSON\"}");
            return;
        }
        const char* responseHex = doc["response"] | "";
        if (strlen(responseHex) != 64) {
            req->send(400, "application/json",
                      "{\"ok\":false,\"error\":\"missing or invalid response\"}");
            return;
        }

        // 3. Decode the 64-char hex response into 32 bytes.
        uint8_t receivedHash[32];
        for (int i = 0; i < 32; i++) {
            char hi = responseHex[i * 2];
            char lo = responseHex[i * 2 + 1];
            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hv = hexVal(hi), lv = hexVal(lo);
            if (hv < 0 || lv < 0) {
                req->send(400, "application/json",
                          "{\"ok\":false,\"error\":\"invalid hex\"}");
                return;
            }
            receivedHash[i] = (uint8_t)((hv << 4) | lv);
        }

        // 4. Brute-force the 4-digit PIN space: compute SHA256(nonce || pin)
        //    for each candidate and compare.  10 000 SHA-256 calls is fast
        //    (~10 ms on ESP32) and the nonce is single-use, preventing replay.
        char matchedPin[5] = {'\0'};
        uint8_t computed[32];
        mbedtls_sha256_context sha_ctx;

        for (int n = 0; n <= 9999; n++) {
            char candidate[5];
            snprintf(candidate, sizeof(candidate), "%04d", n);

            mbedtls_sha256_init(&sha_ctx);
            mbedtls_sha256_starts(&sha_ctx, 0);  // 0 = SHA-256
            mbedtls_sha256_update(&sha_ctx, _challengeNonce,
                                  sizeof(_challengeNonce));
            mbedtls_sha256_update(&sha_ctx,
                                  reinterpret_cast<const uint8_t*>(candidate),
                                  4);
            mbedtls_sha256_finish(&sha_ctx, computed);
            mbedtls_sha256_free(&sha_ctx);

            if (memcmp(computed, receivedHash, 32) == 0) {
                memcpy(matchedPin, candidate, 5);
                break;
            }
        }

        // Zero out the nonce regardless of outcome.
        memset(_challengeNonce, 0, sizeof(_challengeNonce));

        if (matchedPin[0] == '\0') {
            req->send(401, "application/json",
                      "{\"ok\":false,\"error\":\"incorrect PIN\"}");
            return;
        }

        if (_vault.deriveKeyFromPin(matchedPin)) {
            req->send(200, "application/json", "{\"ok\":true}");
        } else {
            JsonDocument resp;
            resp["ok"] = false;
            resp["failed_attempts"] = _vault.failedUnlockAttempts();

            switch (_vault.lastUnlockResult()) {
            case VaultManager::UnlockResult::LockedOut: {
                uint32_t retryAfter = _vault.unlockRetryAfterSeconds();
                resp["error"] = "vault_locked";
                resp["retry_after"] = retryAfter;

                String body;
                serializeJson(resp, body);
                auto* response = req->beginResponse(429, "application/json", body);
                response->addHeader("Retry-After", String(retryAfter).c_str());
                req->send(response);
                return;
            }
            case VaultManager::UnlockResult::InvalidPin:
                resp["error"] = "invalid_pin";
                break;
            default:
                resp["error"] = "key_derivation_failed";
                break;
            }

            String body;
            serializeJson(resp, body);
            int statusCode = (_vault.lastUnlockResult() == VaultManager::UnlockResult::InvalidPin)
                                 ? 401
                                 : 500;
            req->send(statusCode, "application/json", body);
        }
    });
}

void WebApi::_registerWifiProvisioningRoutes() {
    _server.on("/wifi-setup", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!_wifiProvisioningActive) {
            req->send(404, "text/plain", "not found");
            return;
        }

        req->send(200, "text/html", String(FPSTR(WIFI_SETUP_HTML)));
    });

    _server.on("/api/wifi/config", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!_wifiProvisioningActive) {
            req->send(403, "text/plain", "forbidden");
            return;
        }

        if (!req->hasParam("ssid", true) || !req->hasParam("password", true)) {
            req->send(400, "text/html", String(FPSTR(WIFI_SETUP_ERROR_HTML)));
            return;
        }

        String ssid = req->getParam("ssid", true)->value();
        String password = req->getParam("password", true)->value();
        if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63) {
            req->send(400, "text/html", String(FPSTR(WIFI_SETUP_ERROR_HTML)));
            return;
        }

        if (!WifiProvisioning::saveCredentials(ssid, password)) {
            req->send(500, "text/html", String(FPSTR(WIFI_SETUP_ERROR_HTML)));
            return;
        }

        req->send(200, "text/html", String(FPSTR(WIFI_SETUP_CONFIRMATION_HTML)));
        scheduleProvisioningReboot();
    });
}

// ── /api/dev/* — development only ────────────────────────────────────────────

#if CONFIG_X4_DIAG_HTTP_API
void WebApi::_registerDevRoutes() {
    // GET /api/dev/status — full X4Diagnostics JSON
    _server.on("/api/dev/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _diag.refresh();
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        _diag.toJson(obj);
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/dev/health
    _server.on("/api/dev/health", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _diag.refresh();
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        bool allOk = _diag.display.initialized && _diag.storageReady &&
                     _diag.batteryPercent > 0 && !_diag.safeModeActive;
        obj["ok"] = allOk;
        JsonObject checks = obj["checks"].to<JsonObject>();
        checks["display"]   = _diag.display.initialized;
        checks["storage"]   = _diag.storageReady;
        checks["battery"]   = (_diag.batteryPercent > 0);
        checks["safeMode"]  = _diag.safeModeActive;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/dev/logs
    _server.on("/api/dev/logs", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        req->send(200, "application/json", _logs.toJson());
    });

    // GET /api/dev/ota
    _server.on("/api/dev/ota", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _diag.refresh();
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["currentSlot"]       = _diag.otaCurrentSlot;
        obj["pendingVerify"]     = _diag.otaPendingVerify;
        obj["rollbackAvailable"] = _diag.otaRollbackAvailable;
        obj["firmwareVersion"]   = _diag.firmwareVersion;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/dev/display — same as /api/display/status but under dev path
    _server.on("/api/dev/display", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        _diag.refresh();
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        const X4DisplayStatus& s = _display.status();
        obj["driver"]          = s.driverName;
        obj["initialized"]     = s.initialized;
        obj["lastRefreshMs"]   = s.lastRefreshMs;
        obj["lastRefreshType"] = s.lastRefreshType;
        if (s.lastError) obj["lastError"] = s.lastError;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // POST /api/dev/ota/check — fetch manifest and return result
    _server.on("/api/dev/ota/check", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        OtaManifest m = _ota.fetchManifest();
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["valid"]   = m.valid;
        if (m.valid) {
            obj["version"] = m.version;
            obj["channel"] = m.channel;
            obj["notes"]   = m.notes;
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // POST /api/dev/ota/apply — download and flash
    _server.on("/api/dev/ota/apply", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        OtaManifest m = _ota.fetchManifest();
        OtaResult r = _ota.applyUpdate(m, _diag.batteryPercent);
        req->send(200, "text/plain", OtaManager::resultString(r));
        // If OK, applyUpdate() already called esp_restart()
    });

    // POST /api/dev/ota/rollback
    _server.on("/api/dev/ota/rollback", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        req->send(200, "text/plain", "rolling back...");
        delay(200);
        _ota.requestRollback();
    });

    // POST /api/dev/reboot
    _server.on("/api/dev/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        req->send(200, "text/plain", "rebooting...");
        delay(200);
        ESP.restart();
    });
}
#endif // CONFIG_X4_DIAG_HTTP_API

// ── Export & PWA routes ───────────────────────────────────────────────────────
//
// GET /manifest.json       — PWA Web App Manifest
// GET /sw.js               — minimal service worker (offline cache)
// GET /api/export/all      — JSON array of every entry across all months
//
// These routes are always available regardless of build configuration.

static const char PWA_MANIFEST[] PROGMEM = R"json(
{
  "name": "eJournal",
  "short_name": "Journal",
  "description": "E-Paper Journal companion",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#f5f0e8",
  "theme_color": "#4a4a4a",
  "icons": [
    {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
    {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png"}
  ]
}
)json";

static const char SERVICE_WORKER[] PROGMEM = R"js(
const CACHE = 'pocket-shrine-v1';
const PRECACHE = ['/'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(PRECACHE)));
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', e => {
  const url = e.request.url;
  // Cache-first for the SPA shell; network-first for API calls
  if (url.includes('/api/') || url.includes('/api/export')) {
    e.respondWith(fetch(e.request).catch(() =>
      new Response('{"error":"offline"}', {headers: {'Content-Type':'application/json'}})
    ));
  } else {
    e.respondWith(
      caches.match(e.request).then(r => r || fetch(e.request).then(resp => {
        return caches.open(CACHE).then(c => { c.put(e.request, resp.clone()); return resp; });
      }))
    );
  }
});
)js";

void WebApi::_registerExportRoutes() {
    // GET /manifest.json — PWA Web App Manifest
    _server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        req->send(200, "application/manifest+json",
                  String(FPSTR(PWA_MANIFEST)));
    });

    // GET /sw.js — service worker
    _server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        auto* resp = req->beginResponse(200, "application/javascript",
                                         String(FPSTR(SERVICE_WORKER)));
        resp->addHeader("Service-Worker-Allowed", "/");
        req->send(resp);
    });

    // GET /api/export/all — JSON array of all entries (title, date, tags, body)
    // If an entry is locked (vault not unlocked), body is omitted.
    _server.on("/api/export/all", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (rejectWhenWifiDisabled(req)) return;
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        auto paths = _jm.listAllPaths();
        for (auto& path : paths) {
            JournalEntry e;
            bool ok = _jm.loadEntry(path, e);
            if (!ok) continue;

            JsonObject obj = arr.add<JsonObject>();
            // Build Obsidian-friendly filename from path: use the last segment
            int slash = path.lastIndexOf('/');
            String fn = (slash >= 0) ? path.substring(slash + 1) : path;
            obj["path"]     = path;
            obj["filename"] = fn;
            obj["title"]    = e.title;
            obj["date"]     = e.date;
            if (!e.tagsRaw.isEmpty()) obj["tags"] = e.tagsRaw;
            if (!e.locked) {
                obj["body"] = e.body;
            } else {
                obj["locked"] = true;
            }
        }

        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
}
