// ─────────────────────────────────────────────────────────────────────────────
// OtaManager.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "OtaManager.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include "../config.h"
#include "../diagnostics/X4Log.h"

OtaManager::OtaManager() {}

// ── Boot-time hooks ───────────────────────────────────────────────────────────

bool OtaManager::onBoot() {
    X4_LOG(X4M_BOOT_START);

    // Check pending-verify state
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
        esp_ota_get_state_partition(running, &state);
        _pendingVerify = (state == ESP_OTA_IMG_PENDING_VERIFY);
    }
    if (_pendingVerify) {
        X4_LOG(X4M_OTA_PENDING_VERIFY);
    }

    // NVS boot counter
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    _bootCount = prefs.getUInt(NVS_KEY_BOOT_COUNT, 0) + 1;
    prefs.putUInt(NVS_KEY_BOOT_COUNT, _bootCount);
    prefs.end();

    X4_LOGF("BOOT_COUNT", "count=%u", _bootCount);
    return (_bootCount >= CRASH_LOOP_THRESHOLD);
}

void OtaManager::markBootHealthy() {
    if (_pendingVerify) {
        esp_ota_mark_app_valid_cancel_rollback();
        _pendingVerify = false;
        X4_LOG(X4M_OTA_MARK_VALID);
    }
    // Reset crash-loop counter
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUInt(NVS_KEY_BOOT_COUNT, 0);
    prefs.end();
    X4_LOG(X4M_BOOT_OK);
}

void OtaManager::requestRollback() {
    X4_LOG(X4M_OTA_ROLLBACK_REQUESTED);
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

// ── OTA operations ────────────────────────────────────────────────────────────

OtaManifest OtaManager::fetchManifest() {
    OtaManifest m;
    WiFiClientSecure client;
    client.setInsecure(); // TODO: pin certificate for production
    HTTPClient http;
    http.setTimeout(OTA_CONNECT_TIMEOUT_MS);
    if (!http.begin(client, OTA_MANIFEST_URL)) {
        return m;
    }
    int code = http.GET();
    if (code != 200) {
        http.end();
        return m;
    }
    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        return m;
    }
    m.device            = doc["device"].as<String>();
    m.channel           = doc["channel"].as<String>();
    m.version           = doc["version"].as<String>();
    m.url               = doc["url"].as<String>();
    m.sha256            = doc["sha256"].as<String>();
    m.minBatteryPercent = doc["min_battery_percent"] | 20;
    m.notes             = doc["notes"].as<String>();
    m.valid             = true;
    return m;
}

OtaResult OtaManager::applyUpdate(const OtaManifest& manifest, uint16_t batteryPercent) {
    if (!manifest.valid) return OtaResult::MANIFEST_FETCH_FAILED;

    // Guard: correct device and channel
    if (manifest.device != FIRMWARE_DEVICE)   return OtaResult::WRONG_DEVICE;
    if (manifest.channel != FIRMWARE_CHANNEL) return OtaResult::WRONG_CHANNEL;

    // Guard: newer version
    if (!isNewerVersion(manifest.version, FIRMWARE_VERSION)) return OtaResult::ALREADY_CURRENT;

    // Guard: battery
    if (batteryPercent < manifest.minBatteryPercent) return OtaResult::LOW_BATTERY;

    // Guard: space
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (!target) return OtaResult::INSUFFICIENT_SPACE;

    // Download + flash
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(OTA_CONNECT_TIMEOUT_MS);
    if (!http.begin(client, manifest.url)) return OtaResult::DOWNLOAD_FAILED;

    int code = http.GET();
    if (code != 200) {
        http.end();
        return OtaResult::DOWNLOAD_FAILED;
    }

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(target, OTA_SIZE_UNKNOWN, &handle) != ESP_OK) {
        http.end();
        return OtaResult::FLASH_FAILED;
    }

    // SHA-256 context
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 0);
    mbedtls_md_starts(&ctx);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[512];
    bool flashOk = true;
    while (http.connected()) {
        size_t avail = stream->available();
        if (avail == 0) { delay(1); continue; }
        size_t toRead = (avail > sizeof(buf)) ? sizeof(buf) : avail;
        int rd = stream->readBytes(buf, toRead);
        if (rd <= 0) break;
        mbedtls_md_update(&ctx, buf, rd);
        if (esp_ota_write(handle, buf, rd) != ESP_OK) { flashOk = false; break; }
    }
    http.end();

    if (!flashOk) {
        esp_ota_abort(handle);
        mbedtls_md_free(&ctx);
        return OtaResult::FLASH_FAILED;
    }

    // Verify SHA-256
    uint8_t digest[32];
    mbedtls_md_finish(&ctx, digest);
    mbedtls_md_free(&ctx);

    char hexDigest[65];
    for (int i = 0; i < 32; i++) snprintf(hexDigest + i * 2, 3, "%02x", digest[i]);
    hexDigest[64] = '\0';
    if (manifest.sha256 != hexDigest) {
        esp_ota_abort(handle);
        return OtaResult::HASH_MISMATCH;
    }

    if (esp_ota_end(handle) != ESP_OK) return OtaResult::FLASH_FAILED;
    if (esp_ota_set_boot_partition(target) != ESP_OK) return OtaResult::FLASH_FAILED;

    esp_restart();
    return OtaResult::OK; // unreachable
}

bool OtaManager::isPendingVerify() const {
    return _pendingVerify;
}

/*static*/
bool OtaManager::isNewerVersion(const String& candidate, const String& current) {
    // Full semver comparison per https://semver.org/
    // Format: MAJOR.MINOR.PATCH[-PRERELEASE]
    // Pre-release has LOWER precedence than the release (e.g. 1.0.0-dev < 1.0.0).

    auto parseSemVer = [](const String& v, int& maj, int& min, int& pat, String& pre) {
        maj = min = pat = 0;
        pre = "";
        const char* s = v.c_str();
        sscanf(s, "%d.%d.%d", &maj, &min, &pat);
        const char* dash = strchr(s, '-');
        if (dash) pre = String(dash + 1);
    };

    // Returns true when every character in s is a decimal digit (s non-empty).
    auto isNumericId = [](const String& s) -> bool {
        if (s.isEmpty()) return false;
        for (size_t i = 0; i < s.length(); ++i) {
            if (s[i] < '0' || s[i] > '9') return false;
        }
        return true;
    };

    // Compare two pre-release strings per semver §11.
    // Returns -1 (a < b), 0 (a == b), or +1 (a > b).
    // An empty string means "no pre-release" and sorts HIGHER than any pre-release.
    auto comparePreRelease = [&isNumericId](const String& a, const String& b) -> int {
        if (a == b) return 0;
        if (a.isEmpty()) return  1;   // release > pre-release
        if (b.isEmpty()) return -1;   // pre-release < release

        // Both have pre-release — compare dot-separated identifiers left to right.
        int ai = 0, bi = 0;
        while (true) {
            int aDot = a.indexOf('.', ai);
            int bDot = b.indexOf('.', bi);
            String aId = (aDot < 0) ? a.substring(ai) : a.substring(ai, aDot);
            String bId = (bDot < 0) ? b.substring(bi) : b.substring(bi, bDot);

            bool aNum = isNumericId(aId);
            bool bNum = isNumericId(bId);

            int cmp;
            if (aNum && bNum) {
                // Both numeric: compare as integers.
                int av = atoi(aId.c_str()), bv = atoi(bId.c_str());
                cmp = (av > bv) ? 1 : (av < bv) ? -1 : 0;
            } else if (aNum) {
                cmp = -1;  // numeric identifier has lower precedence than alphanumeric
            } else if (bNum) {
                cmp = 1;   // alphanumeric identifier has higher precedence than numeric
            } else {
                // Both alphanumeric: compare lexically (ASCII).
                int r = strcmp(aId.c_str(), bId.c_str());
                cmp = (r > 0) ? 1 : (r < 0) ? -1 : 0;
            }

            if (cmp != 0) return cmp;

            // Equal identifier — check field count.
            bool aEnd = (aDot < 0);
            bool bEnd = (bDot < 0);
            if (aEnd && bEnd) return 0;
            if (aEnd) return -1;  // fewer fields → lower precedence
            if (bEnd) return  1;  // more fields → higher precedence
            ai = aDot + 1;
            bi = bDot + 1;
        }
    };

    int cMaj, cMin, cPat, nMaj, nMin, nPat;
    String cPre, nPre;
    parseSemVer(current,   cMaj, cMin, cPat, cPre);
    parseSemVer(candidate, nMaj, nMin, nPat, nPre);

    if (nMaj != cMaj) return nMaj > cMaj;
    if (nMin != cMin) return nMin > cMin;
    if (nPat != cPat) return nPat > cPat;
    return comparePreRelease(nPre, cPre) > 0;
}

/*static*/
const char* OtaManager::resultString(OtaResult r) {
    switch (r) {
        case OtaResult::OK:                    return "ok";
        case OtaResult::ALREADY_CURRENT:       return "already_current";
        case OtaResult::WRONG_DEVICE:          return "wrong_device";
        case OtaResult::WRONG_CHANNEL:         return "wrong_channel";
        case OtaResult::LOW_BATTERY:           return "low_battery";
        case OtaResult::INSUFFICIENT_SPACE:    return "insufficient_space";
        case OtaResult::DOWNLOAD_FAILED:       return "download_failed";
        case OtaResult::HASH_MISMATCH:         return "hash_mismatch";
        case OtaResult::FLASH_FAILED:          return "flash_failed";
        case OtaResult::MANIFEST_FETCH_FAILED: return "manifest_fetch_failed";
        case OtaResult::MANIFEST_PARSE_FAILED: return "manifest_parse_failed";
        default:                               return "unknown";
    }
}
