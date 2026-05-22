// ─────────────────────────────────────────────────────────────────────────────
// VaultManager.cpp — AES-256-GCM encrypted vault implementation
// ─────────────────────────────────────────────────────────────────────────────

#include "VaultManager.h"

#include <string.h>
#include <stdint.h>
#include <time.h>

// mbedTLS headers — resolved to real SDK on device, stubs in test/mocks/
#include "mbedtls/gcm.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"

// NVS (Preferences) — resolved to real library on device, stub in test/mocks/
#include <Preferences.h>

#include "../diagnostics/X4Log.h"

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

namespace {
constexpr char VAULT_NAMESPACE[]      = "vault";
constexpr char VAULT_LOCK_NAMESPACE[] = "vault_lock";
constexpr char VAULT_SALT_KEY[]       = "salt";
constexpr char VAULT_VERIFIER_KEY[]   = "verifier";
constexpr char LOCK_ATTEMPTS_KEY[]    = "failed";
constexpr char LOCK_UNTIL_KEY[]       = "until";

constexpr uint32_t LOCKOUT_WINDOWS_SECONDS[] = {30, 300, 3600};
constexpr uint32_t CLOCK_SET_EPOCH           = 1577836800UL;

constexpr uint8_t PIN_VERIFIER_MAGIC[] = {
    'e', 'J', 'o', 'u', 'r', 'n', 'a', 'l',
    '-', 'v', 'a', 'u', 'l', 't', '-', '1'
};
} // namespace

VaultManager::VaultManager() {
    memset(_key, 0, sizeof(_key));
    _lastUnlockResult = UnlockResult::Error;
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

VaultManager::UnlockResult VaultManager::lastUnlockResult() const {
    return _lastUnlockResult;
}

bool VaultManager::isUnlockLockedOut() const {
    return _retryAfterSeconds(_loadLockState()) > 0;
}

uint32_t VaultManager::unlockRetryAfterSeconds() const {
    return _retryAfterSeconds(_loadLockState());
}

uint32_t VaultManager::failedUnlockAttempts() const {
    return _loadLockState().failedAttempts;
}

void VaultManager::lock() {
    memset(_key, 0, sizeof(_key));
    memset(_legacyKey, 0, sizeof(_legacyKey));
}

bool VaultManager::_loadOrCreateSalt(uint8_t salt[16]) {
    Preferences prefs;
    prefs.begin(VAULT_NAMESPACE, false);
    if (prefs.isKey(VAULT_SALT_KEY)) {
        size_t n = prefs.getBytes(VAULT_SALT_KEY, salt, 16);
        prefs.end();
        return n == 16;
    }
    // Generate a new salt
    _genRandom(salt, 16);
    prefs.putBytes(VAULT_SALT_KEY, salt, 16);
    prefs.end();
    return true;
}

bool VaultManager::deriveKeyFromPin(const char* pin) {
    if (!pin || pin[0] == '\0') {
        _lastUnlockResult = UnlockResult::Error;
        return false;
    }

    LockState lockState = _loadLockState();
    uint32_t retryAfter = _retryAfterSeconds(lockState);
    if (retryAfter > 0) {
        _lastUnlockResult = UnlockResult::LockedOut;
        X4_LOGF("VAULT_LOCKOUT", "active failed=%u retry_after=%us",
                lockState.failedAttempts, retryAfter);
        return false;
    }

    uint8_t salt[16];
    if (!_loadOrCreateSalt(salt)) {
        _lastUnlockResult = UnlockResult::Error;
        return false;
    }

    // PBKDF2-HMAC-SHA256: derive both current (100k) and legacy (10k) keys.
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&md_ctx, md_info, 1) != 0) {
        mbedtls_md_free(&md_ctx);
        _lastUnlockResult = UnlockResult::Error;
        return false;
    }

    uint8_t derivedKey[32];
    int rc = mbedtls_pkcs5_pbkdf2_hmac(
        &md_ctx,
        reinterpret_cast<const unsigned char*>(pin), strlen(pin),
        salt, 16,
        100000,
        32, derivedKey);

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
        memset(derivedKey, 0, sizeof(derivedKey));
        _lastUnlockResult = UnlockResult::Error;
        return false;
    }

    bool matched = false;
    bool verified = _verifyDerivedKey(derivedKey, matched);
    if (!verified) {
        memset(derivedKey, 0, sizeof(derivedKey));
        _lastUnlockResult = UnlockResult::Error;
        return false;
    }

    if (!matched) {
        memset(derivedKey, 0, sizeof(derivedKey));
        _recordFailedAttempt();
        if (isUnlockLockedOut()) {
            _lastUnlockResult = UnlockResult::LockedOut;
        } else {
            _lastUnlockResult = UnlockResult::InvalidPin;
        }
        return false;
    }

    memcpy(_key, derivedKey, sizeof(_key));
    memset(derivedKey, 0, sizeof(derivedKey));
    _resetLockState();
    _lastUnlockResult = UnlockResult::Success;
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

bool VaultManager::_loadPinVerifier(uint8_t* verifier, size_t len) {
    Preferences prefs;
    prefs.begin(VAULT_NAMESPACE, true);
    size_t n = prefs.getBytes(VAULT_VERIFIER_KEY, verifier, len);
    prefs.end();
    return n == len;
}

bool VaultManager::_storePinVerifier(const uint8_t key[32]) {
    uint8_t blob[12 + 16 + sizeof(PIN_VERIFIER_MAGIC)];
    uint8_t* nonce = blob;
    uint8_t* tag   = blob + 12;
    uint8_t* ct    = blob + 12 + 16;

    _genRandom(nonce, 12);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    int rc = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, sizeof(PIN_VERIFIER_MAGIC),
        nonce, 12, nullptr, 0,
        PIN_VERIFIER_MAGIC, ct,
        16, tag);

    mbedtls_gcm_free(&gcm);
    if (rc != 0) return false;

    Preferences prefs;
    prefs.begin(VAULT_NAMESPACE, false);
    bool ok = prefs.putBytes(VAULT_VERIFIER_KEY, blob, sizeof(blob)) == sizeof(blob);
    prefs.end();
    return ok;
}

bool VaultManager::_verifyDerivedKey(const uint8_t key[32], bool& matched) {
    matched = false;

    uint8_t blob[12 + 16 + sizeof(PIN_VERIFIER_MAGIC)];
    if (!_loadPinVerifier(blob, sizeof(blob))) {
        matched = _storePinVerifier(key);
        return matched;
    }

    const uint8_t* nonce = blob;
    const uint8_t* tag   = blob + 12;
    const uint8_t* ct    = blob + 12 + 16;
    uint8_t plain[sizeof(PIN_VERIFIER_MAGIC)] = {0};

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    int rc = mbedtls_gcm_auth_decrypt(
        &gcm, sizeof(PIN_VERIFIER_MAGIC),
        nonce, 12, nullptr, 0,
        tag, 16,
        ct, plain);
    mbedtls_gcm_free(&gcm);

    if (rc != 0) return true;

    matched = memcmp(plain, PIN_VERIFIER_MAGIC, sizeof(PIN_VERIFIER_MAGIC)) == 0;
    return true;
}

VaultManager::LockState VaultManager::_loadLockState() const {
    Preferences prefs;
    prefs.begin(VAULT_LOCK_NAMESPACE, true);

    LockState state;
    state.failedAttempts = prefs.getUInt(LOCK_ATTEMPTS_KEY, 0);
    state.lockoutUntil   = prefs.getUInt(LOCK_UNTIL_KEY, 0);

    prefs.end();
    return state;
}

bool VaultManager::_saveLockState(const LockState& state) const {
    Preferences prefs;
    prefs.begin(VAULT_LOCK_NAMESPACE, false);
    bool ok = prefs.putUInt(LOCK_ATTEMPTS_KEY, state.failedAttempts) &&
              prefs.putUInt(LOCK_UNTIL_KEY, state.lockoutUntil);
    prefs.end();
    return ok;
}

void VaultManager::_resetLockState() {
    LockState state;
    _saveLockState(state);
}

void VaultManager::_recordFailedAttempt() {
    LockState state = _loadLockState();
    state.failedAttempts++;

    uint32_t lockoutWindow = 0;
    if (state.failedAttempts >= 3) {
        uint32_t stage = state.failedAttempts - 3;
        if (stage >= (sizeof(LOCKOUT_WINDOWS_SECONDS) / sizeof(LOCKOUT_WINDOWS_SECONDS[0]))) {
            stage = (sizeof(LOCKOUT_WINDOWS_SECONDS) / sizeof(LOCKOUT_WINDOWS_SECONDS[0])) - 1;
        }
        lockoutWindow = LOCKOUT_WINDOWS_SECONDS[stage];
        state.lockoutUntil = _nowSeconds() + lockoutWindow;
        X4_LOGF("VAULT_LOCKOUT", "failed=%u window=%us until=%u",
                state.failedAttempts, lockoutWindow, state.lockoutUntil);
    } else {
        state.lockoutUntil = 0;
    }

    _saveLockState(state);
}

uint32_t VaultManager::_nowSeconds() const {
#ifdef ESP_PLATFORM
    time_t now = time(nullptr);
    if (now > (time_t)CLOCK_SET_EPOCH) {
        return (uint32_t)now;
    }
#endif
    return millis() / 1000;
}

uint32_t VaultManager::_retryAfterSeconds(const LockState& state) const {
    uint32_t now = _nowSeconds();
    if (state.lockoutUntil <= now) return 0;
    return state.lockoutUntil - now;
}
