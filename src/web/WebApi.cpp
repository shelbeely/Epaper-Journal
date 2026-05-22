// ─────────────────────────────────────────────────────────────────────────────
// WebApi.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "WebApi.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <array>
#include <memory>
#include <vector>
#include "../config.h"
#include "../journal/JournalExport.h"
#include "../journal/JournalManager.h"
#include "../journal/JournalFrontmatter.h"
#include "ui_bundle.h"

// ── LogRingBuffer ─────────────────────────────────────────────────────────────

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

namespace {

struct ZipExportFile {
    String path;
    String zipPath;
    uint32_t crc32 = 0;
    uint32_t size = 0;
    uint32_t localOffset = 0;
    uint32_t centralOffset = 0;
    std::vector<uint8_t> localHeader;
    std::vector<uint8_t> centralHeader;
};

static void appendLe16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back((uint8_t)(value & 0xFFu));
    out.push_back((uint8_t)((value >> 8u) & 0xFFu));
}

static void appendLe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back((uint8_t)(value & 0xFFu));
    out.push_back((uint8_t)((value >> 8u) & 0xFFu));
    out.push_back((uint8_t)((value >> 16u) & 0xFFu));
    out.push_back((uint8_t)((value >> 24u) & 0xFFu));
}

static std::vector<uint8_t> buildLocalHeader(const ZipExportFile& file) {
    std::vector<uint8_t> out;
    out.reserve(30 + file.zipPath.length());
    appendLe32(out, 0x04034B50u);                    // local file header sig
    appendLe16(out, 20);                             // version needed
    appendLe16(out, 0);                              // flags
    appendLe16(out, 0);                              // method: stored
    appendLe16(out, 0);                              // mod time
    appendLe16(out, 0);                              // mod date
    appendLe32(out, file.crc32);
    appendLe32(out, file.size);
    appendLe32(out, file.size);
    appendLe16(out, (uint16_t)file.zipPath.length());
    appendLe16(out, 0);                              // extra length
    for (size_t i = 0; i < file.zipPath.length(); ++i) {
        out.push_back((uint8_t)file.zipPath[i]);
    }
    return out;
}

static std::vector<uint8_t> buildCentralHeader(const ZipExportFile& file) {
    std::vector<uint8_t> out;
    out.reserve(46 + file.zipPath.length());
    appendLe32(out, 0x02014B50u);                    // central dir sig
    appendLe16(out, 20);                             // version made by
    appendLe16(out, 20);                             // version needed
    appendLe16(out, 0);                              // flags
    appendLe16(out, 0);                              // method: stored
    appendLe16(out, 0);                              // mod time
    appendLe16(out, 0);                              // mod date
    appendLe32(out, file.crc32);
    appendLe32(out, file.size);
    appendLe32(out, file.size);
    appendLe16(out, (uint16_t)file.zipPath.length());
    appendLe16(out, 0);                              // extra length
    appendLe16(out, 0);                              // comment length
    appendLe16(out, 0);                              // disk number
    appendLe16(out, 0);                              // internal attrs
    appendLe32(out, 0);                              // external attrs
    appendLe32(out, file.localOffset);
    for (size_t i = 0; i < file.zipPath.length(); ++i) {
        out.push_back((uint8_t)file.zipPath[i]);
    }
    return out;
}

static std::array<uint8_t, 22> buildEndRecord(uint16_t count,
                                              uint32_t centralSize,
                                              uint32_t centralOffset) {
    std::array<uint8_t, 22> out {};
    out[0] = 0x50; out[1] = 0x4B; out[2] = 0x05; out[3] = 0x06;
    out[8] = (uint8_t)(count & 0xFFu);
    out[9] = (uint8_t)((count >> 8u) & 0xFFu);
    out[10] = out[8];
    out[11] = out[9];
    out[12] = (uint8_t)(centralSize & 0xFFu);
    out[13] = (uint8_t)((centralSize >> 8u) & 0xFFu);
    out[14] = (uint8_t)((centralSize >> 16u) & 0xFFu);
    out[15] = (uint8_t)((centralSize >> 24u) & 0xFFu);
    out[16] = (uint8_t)(centralOffset & 0xFFu);
    out[17] = (uint8_t)((centralOffset >> 8u) & 0xFFu);
    out[18] = (uint8_t)((centralOffset >> 16u) & 0xFFu);
    out[19] = (uint8_t)((centralOffset >> 24u) & 0xFFu);
    return out;
}

static size_t copyBytes(uint8_t* dst,
                        size_t maxLen,
                        const uint8_t* src,
                        size_t srcLen,
                        size_t srcOffset) {
    if (srcOffset >= srcLen || maxLen == 0) return 0;
    size_t remaining = srcLen - srcOffset;
    size_t toCopy = (remaining < maxLen) ? remaining : maxLen;
    memcpy(dst, src + srcOffset, toCopy);
    return toCopy;
}

class JournalZipStreamer {
public:
    JournalZipStreamer(JournalManager& jm, VaultManager& vault, bool exportEncrypted)
        : _jm(jm), _vault(vault), _exportEncrypted(exportEncrypted) {
        _prepare();
    }

    size_t totalSize() const { return _totalSize; }

    size_t fill(uint8_t* data, size_t maxLen, size_t offset) {
        size_t written = 0;
        while (written < maxLen && offset + written < _totalSize) {
            size_t pos = offset + written;

            if (pos < _centralOffset) {
                const ZipExportFile* file = _findLocalFile(pos);
                if (!file) break;

                size_t dataStart = file->localOffset + file->localHeader.size();
                if (pos < dataStart) {
                    written += copyBytes(data + written, maxLen - written,
                                         file->localHeader.data(),
                                         file->localHeader.size(),
                                         pos - file->localOffset);
                    continue;
                }

                String content = _loadContent(*file);
                written += copyBytes(data + written, maxLen - written,
                                     (const uint8_t*)content.c_str(),
                                     content.length(),
                                     pos - dataStart);
                continue;
            }

            if (pos < _endOffset) {
                const ZipExportFile* file = _findCentralFile(pos);
                if (!file) break;
                written += copyBytes(data + written, maxLen - written,
                                     file->centralHeader.data(),
                                     file->centralHeader.size(),
                                     pos - file->centralOffset);
                continue;
            }

            written += copyBytes(data + written, maxLen - written,
                                 _endRecord.data(), _endRecord.size(),
                                 pos - _endOffset);
        }

        return written;
    }

private:
    JournalManager& _jm;
    VaultManager& _vault;
    bool _exportEncrypted;
    std::vector<ZipExportFile> _files;
    std::array<uint8_t, 22> _endRecord {};
    uint32_t _centralOffset = 0;
    uint32_t _endOffset = 0;
    uint32_t _totalSize = 0;

    String _loadContent(const ZipExportFile& file) {
        String raw = _jm.readEntryRaw(file.path);
        return JournalExport::selectExportContent(raw, &_vault, _exportEncrypted);
    }

    void _prepare() {
        uint32_t nextLocalOffset = 0;
        auto paths = _jm.listAllPaths();
        _files.reserve(paths.size());

        for (const auto& path : paths) {
            String raw = _jm.readEntryRaw(path);
            if (raw.isEmpty()) continue;

            String content =
                JournalExport::selectExportContent(raw, &_vault, _exportEncrypted);

            ZipExportFile file;
            file.path = path;
            file.zipPath = JournalExport::zipPathFromEntryPath(path);
            file.size = content.length();
            file.crc32 = JournalExport::crc32(
                (const uint8_t*)content.c_str(), content.length());
            file.localOffset = nextLocalOffset;
            file.localHeader = buildLocalHeader(file);
            nextLocalOffset += file.localHeader.size() + file.size;

            _files.push_back(file);
        }

        _centralOffset = nextLocalOffset;
        uint32_t nextCentralOffset = _centralOffset;
        for (auto& file : _files) {
            file.centralOffset = nextCentralOffset;
            file.centralHeader = buildCentralHeader(file);
            nextCentralOffset += file.centralHeader.size();
        }

        _endOffset = nextCentralOffset;
        uint32_t centralSize = _endOffset - _centralOffset;
        _endRecord = buildEndRecord((uint16_t)_files.size(),
                                    centralSize,
                                    _centralOffset);
        _totalSize = _endOffset + _endRecord.size();
    }

    const ZipExportFile* _findLocalFile(size_t pos) const {
        for (const auto& file : _files) {
            size_t fileEnd = file.localOffset + file.localHeader.size() + file.size;
            if (pos >= file.localOffset && pos < fileEnd) return &file;
        }
        return nullptr;
    }

    const ZipExportFile* _findCentralFile(size_t pos) const {
        for (const auto& file : _files) {
            size_t fileEnd = file.centralOffset + file.centralHeader.size();
            if (pos >= file.centralOffset && pos < fileEnd) return &file;
        }
        return nullptr;
    }
};

} // namespace

// ── WebApi ────────────────────────────────────────────────────────────────────

WebApi::WebApi(X4Diagnostics& diag, X4Display& display, OtaManager& ota,
               JournalManager& jm, VaultManager& vault)
    : _diag(diag), _display(display), _ota(ota), _jm(jm), _vault(vault)
{}

void WebApi::begin() {
    _registerDisplayRoutes();
    _registerJournalRoutes();
    _registerVaultRoutes();
    _registerExportRoutes();
#if CONFIG_X4_DIAG_HTTP_API
    _registerDevRoutes();
#endif
    // Root — serve the eJournal SPA (gzip-compressed, generated by tools/embed_ui.py)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* resp = req->beginResponse_P(
            200, "text/html", UI_HTML_GZ, UI_HTML_GZ_LEN);
        resp->addHeader("Content-Encoding", "gzip");
        req->send(resp);
    });
    _server.begin();
}

void WebApi::pushLog(const String& line) {
    _logs.push(line);
}

// ── /api/display/* — always available ────────────────────────────────────────

void WebApi::_registerDisplayRoutes() {
    // GET /api/display/status
    _server.on("/api/display/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
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
        _display.fullRefresh();
        req->send(200, "text/plain", "ok");
    });

    // POST /api/display/refresh/partial  body: {"x":0,"y":0,"w":100,"h":100}
    _server.on("/api/display/refresh/partial", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
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
        _display.clear();
        req->send(200, "text/plain", "ok");
    });
}

// ── /api/journal/* ────────────────────────────────────────────────────────────

void WebApi::_registerJournalRoutes() {
    // GET /api/journal/entries?year=YYYY&month=MM
    _server.on("/api/journal/entries", HTTP_GET, [this](AsyncWebServerRequest* req) {
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

    // GET /api/journal/entry?path=<path>
    _server.on("/api/journal/entry", HTTP_GET, [this](AsyncWebServerRequest* req) {
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

    // GET /api/journal/export[?encrypted=1]
    _server.on("/api/journal/export", HTTP_GET, [this](AsyncWebServerRequest* req) {
        bool exportEncrypted =
            req->hasParam("encrypted") &&
            req->getParam("encrypted")->value() == "1";

        auto streamer =
            std::make_shared<JournalZipStreamer>(_jm, _vault, exportEncrypted);
        AsyncWebServerResponse* resp = req->beginResponse(
            "application/zip",
            streamer->totalSize(),
            [streamer](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                return streamer->fill(buffer, maxLen, index);
            });
        resp->addHeader("Content-Disposition",
                        "attachment; filename=\"" +
                        JournalExport::backupFilename(time(nullptr)) + "\"");
        req->send(resp);
    });
}

// ── Vault routes ──────────────────────────────────────────────────────────────
//   GET  /api/vault/status  — {"locked": bool}
//   POST /api/vault/unlock  — body {"pin":"1234"} → {"ok": bool}
//   POST /api/vault/lock    — {} → {"ok": true}

void WebApi::_registerVaultRoutes() {
    // GET /api/vault/status
    _server.on("/api/vault/status", HTTP_GET,
               [this](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["locked"] = !_vault.isUnlocked();
        String body;
        serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // POST /api/vault/lock
    _server.on("/api/vault/lock", HTTP_POST,
               [this](AsyncWebServerRequest* req) {
        _vault.lock();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // POST /api/vault/unlock — body: {"pin":"1234"}
    _server.on("/api/vault/unlock", HTTP_POST,
               [](AsyncWebServerRequest* req) { /* body handled below */ },
               nullptr,
               [this](AsyncWebServerRequest* req,
                      uint8_t* data, size_t len, size_t, size_t) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
            req->send(400, "application/json",
                      "{\"ok\":false,\"error\":\"invalid JSON\"}");
            return;
        }
        const char* pin = doc["pin"] | "";
        if (pin[0] == '\0') {
            req->send(400, "application/json",
                      "{\"ok\":false,\"error\":\"missing pin\"}");
            return;
        }
        if (_vault.deriveKeyFromPin(pin)) {
            req->send(200, "application/json", "{\"ok\":true}");
        } else {
            req->send(500, "application/json",
                      "{\"ok\":false,\"error\":\"key derivation failed\"}");
        }
    });
}

// ── /api/dev/* — development only ────────────────────────────────────────────

#if CONFIG_X4_DIAG_HTTP_API
void WebApi::_registerDevRoutes() {
    // GET /api/dev/status — full X4Diagnostics JSON
    _server.on("/api/dev/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
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
        req->send(200, "application/json", _logs.toJson());
    });

    // GET /api/dev/ota
    _server.on("/api/dev/ota", HTTP_GET, [this](AsyncWebServerRequest* req) {
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
        OtaManifest m = _ota.fetchManifest();
        OtaResult r = _ota.applyUpdate(m, _diag.batteryPercent);
        req->send(200, "text/plain", OtaManager::resultString(r));
        // If OK, applyUpdate() already called esp_restart()
    });

    // POST /api/dev/ota/rollback
    _server.on("/api/dev/ota/rollback", HTTP_POST, [this](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "rolling back...");
        delay(200);
        _ota.requestRollback();
    });

    // POST /api/dev/reboot
    _server.on("/api/dev/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
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
        req->send(200, "application/manifest+json",
                  String(FPSTR(PWA_MANIFEST)));
    });

    // GET /sw.js — service worker
    _server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        auto* resp = req->beginResponse(200, "application/javascript",
                                         String(FPSTR(SERVICE_WORKER)));
        resp->addHeader("Service-Worker-Allowed", "/");
        req->send(resp);
    });

    // GET /api/export/all — JSON array of all entries (title, date, tags, body)
    // If an entry is locked (vault not unlocked), body is omitted.
    _server.on("/api/export/all", HTTP_GET, [this](AsyncWebServerRequest* req) {
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
