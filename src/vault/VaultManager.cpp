// ─────────────────────────────────────────────────────────────────────────────
// VaultManager.cpp — AES-256-GCM encrypted vault implementation
// ─────────────────────────────────────────────────────────────────────────────

#include "VaultManager.h"

#include <string.h>
#include <stdint.h>

// mbedTLS headers — resolved to real SDK on device, stubs in test/mocks/
#include "mbedtls/gcm.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"

// NVS (Preferences) — resolved to real library on device, stub in test/mocks/
#include <Preferences.h>

// Hardware RNG on ESP32; fall back to stdlib rand() in native/test builds.
#ifdef ESP_PLATFORM
#  include <esp_random.h>
static void _genRandom(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i += sizeof(uint32_t)) {
        uint32_t r = esp_random();
        size_t n = (i + sizeof(uint32_t) <= len) ? sizeof(uint32_t) : len - i;
        memcpy(buf + i, &r, n);
    }
}
#else
#  include <cstdlib>
static uint32_t _stubNonceCounter = 0;
static void _genRandom(uint8_t* buf, size_t len) {
    // Deterministic stub: increments a counter per call for unique nonces
    memset(buf, 0, len);
    _stubNonceCounter++;
    size_t copy = (len < sizeof(_stubNonceCounter)) ? len : sizeof(_stubNonceCounter);
    memcpy(buf, &_stubNonceCounter, copy);
}
#endif

// ── Minimal base64 encoder/decoder (RFC 4648, no line wrapping) ──────────────

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String b64Enc(const uint8_t* data, size_t len) {
    String out;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i + 1 < len) v |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) v |= (uint32_t)data[i + 2];
        out += B64[(v >> 18) & 63];
        out += B64[(v >> 12) & 63];
        out += (i + 1 < len) ? B64[(v >> 6) & 63] : '=';
        out += (i + 2 < len) ? B64[v & 63]        : '=';
    }
    return out;
}

static int b64CharVal(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Decode base64 into `out` (must have enough space). Returns byte count or -1.
static int b64Dec(const String& in, uint8_t* out, size_t maxOut) {
    size_t outLen = 0;
    size_t inLen  = in.length();
    for (size_t i = 0; i + 3 < inLen; i += 4) {
        int a = b64CharVal(in[i]);
        int b = b64CharVal(in[i + 1]);
        int c = b64CharVal(in[i + 2]);
        int d = b64CharVal(in[i + 3]);
        if (a < 0 || b < 0) return -1;
        if (outLen >= maxOut) return -1;
        out[outLen++] = (uint8_t)((a << 2) | (b >> 4));
        if (c >= 0) {
            if (outLen >= maxOut) return -1;
            out[outLen++] = (uint8_t)((b << 4) | (c >> 2));
        }
        if (d >= 0) {
            if (outLen >= maxOut) return -1;
            out[outLen++] = (uint8_t)(((c & 0x03) << 6) | d);
        }
    }
    return (int)outLen;
}

// ── VaultManager ──────────────────────────────────────────────────────────────

VaultManager::VaultManager() {
    memset(_key, 0, sizeof(_key));
    memset(_legacyKey, 0, sizeof(_legacyKey));
}

VaultManager::~VaultManager() {
    // Zero out the key on destruction
    memset(_key, 0, sizeof(_key));
    memset(_legacyKey, 0, sizeof(_legacyKey));
}

bool VaultManager::isUnlocked() const {
    for (int i = 0; i < 32; i++) {
        if (_key[i] != 0) return true;
    }
    return false;
}

void VaultManager::lock() {
    memset(_key, 0, sizeof(_key));
    memset(_legacyKey, 0, sizeof(_legacyKey));
}

bool VaultManager::_loadOrCreateSalt(uint8_t salt[16]) {
    Preferences prefs;
    prefs.begin("vault", false);
    if (prefs.isKey("salt")) {
        size_t n = prefs.getBytes("salt", salt, 16);
        prefs.end();
        return n == 16;
    }
    // Generate a new salt
    _genRandom(salt, 16);
    prefs.putBytes("salt", salt, 16);
    prefs.end();
    return true;
}

bool VaultManager::deriveKeyFromPin(const char* pin) {
    if (!pin || pin[0] == '\0') return false;

    uint8_t salt[16];
    if (!_loadOrCreateSalt(salt)) return false;

    // PBKDF2-HMAC-SHA256: derive both current (100k) and legacy (10k) keys.
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&md_ctx, md_info, 1) != 0) {
        mbedtls_md_free(&md_ctx);
        return false;
    }

    int rc = mbedtls_pkcs5_pbkdf2_hmac(
        &md_ctx,
        reinterpret_cast<const unsigned char*>(pin), strlen(pin),
        salt, 16,
        100000,
        32, _key);

    if (rc == 0) {
        rc = mbedtls_pkcs5_pbkdf2_hmac(
            &md_ctx,
            reinterpret_cast<const unsigned char*>(pin), strlen(pin),
            salt, 16,
            10000,
            32, _legacyKey);
    }

    mbedtls_md_free(&md_ctx);
    if (rc != 0) {
        memset(_key, 0, sizeof(_key));
        memset(_legacyKey, 0, sizeof(_legacyKey));
        return false;
    }
    return true;
}

String VaultManager::encrypt(const String& plaintext) {
    if (!isUnlocked()) return "";
    if (plaintext.isEmpty()) return "";

    size_t ptLen  = plaintext.length();
    size_t blobLen = 12 + 16 + ptLen;          // nonce + tag + ciphertext
    uint8_t* blob = new uint8_t[blobLen];
    if (!blob) return "";

    uint8_t* nonce = blob;
    uint8_t* tag   = blob + 12;
    uint8_t* ct    = blob + 12 + 16;

    _genRandom(nonce, 12);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _key, 256);

    int rc = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, ptLen,
        nonce, 12, nullptr, 0,
        reinterpret_cast<const uint8_t*>(plaintext.c_str()), ct,
        16, tag);

    mbedtls_gcm_free(&gcm);

    if (rc != 0) {
        delete[] blob;
        return "";
    }

    String result = VAULT_HEADER;
    result += b64Enc(blob, blobLen);
    result += "\n";

    delete[] blob;
    return result;
}

String VaultManager::decrypt(const String& ciphertext) {
    if (!isUnlocked()) return "";
    if (!isEncryptedContent(ciphertext)) return "";

    // Strip header line and any trailing newline
    const bool isLegacy = ciphertext.startsWith(VAULT_HEADER_V1);
    const char* header  = isLegacy ? VAULT_HEADER_V1 : VAULT_HEADER_V2;
    const uint8_t* key  = isLegacy ? _legacyKey : _key;
    String b64 = ciphertext.substring(strlen(header));
    // Remove trailing newline if present
    if (b64.length() > 0 && b64[b64.length() - 1] == '\n') {
        b64 = b64.substring(0, b64.length() - 1);
    }

    // Maximum decoded size: 3/4 * base64_len + 4 slack
    size_t maxDec = (b64.length() * 3) / 4 + 4;
    uint8_t* blob = new uint8_t[maxDec];
    if (!blob) return "";

    int blobLen = b64Dec(b64, blob, maxDec);
    if (blobLen < 12 + 16) {
        delete[] blob;
        return "";
    }

    size_t ptLen = (size_t)blobLen - 12 - 16;
    const uint8_t* nonce = blob;
    const uint8_t* tag   = blob + 12;
    const uint8_t* ct    = blob + 12 + 16;

    uint8_t* pt = new uint8_t[ptLen + 1];
    if (!pt) { delete[] blob; return ""; }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);

    int rc = mbedtls_gcm_auth_decrypt(
        &gcm, ptLen,
        nonce, 12, nullptr, 0,
        tag, 16,
        ct, pt);

    mbedtls_gcm_free(&gcm);
    delete[] blob;

    if (rc != 0) {
        delete[] pt;
        return "";
    }

    pt[ptLen] = '\0';
    String result(reinterpret_cast<const char*>(pt));
    delete[] pt;
    return result;
}

/*static*/
bool VaultManager::isEncryptedContent(const String& content) {
    return content.startsWith(VAULT_HEADER_V2) || content.startsWith(VAULT_HEADER_V1);
}
