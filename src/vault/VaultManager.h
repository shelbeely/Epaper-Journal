#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// VaultManager.h — AES-256-GCM encrypted journal vault
//
// Provides per-session PIN-based encryption for journal entries.
// Key derivation:
//   - v2: PBKDF2-SHA256(pin, salt, 100000) → 32-byte AES key.
//   - v1 (legacy): PBKDF2-SHA256(pin, salt, 10000).
// The 16-byte salt is generated once and persisted in NVS.
//
// Encrypted file format (stored on SD card):
//   ---vault-v2---
//   <base64(nonce[12] | tag[16] | ciphertext[N])>
//
// The plaintext that is encrypted is the full YAML-frontmatter Markdown file.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>

class VaultManager {
public:
    enum class UnlockResult : uint8_t {
        Success,
        InvalidPin,
        LockedOut,
        Error,
    };

    VaultManager();
    ~VaultManager();

    // Derive the 32-byte AES key from `pin` using PBKDF2-SHA256 and the
    // NVS-persisted salt. Creates and saves the salt on first call.
    // Returns true on success; the vault is unlocked after a successful call.
    bool deriveKeyFromPin(const char* pin);

    // Outcome of the most recent deriveKeyFromPin() call.
    UnlockResult lastUnlockResult() const;

    // Failed-attempt / lockout state persisted in NVS.
    bool isUnlockLockedOut() const;
    uint32_t unlockRetryAfterSeconds() const;
    uint32_t failedUnlockAttempts() const;

    // Zero out the in-memory key. The vault is locked after this call.
    void lock();

    // True if the key is non-zero (vault is unlocked).
    bool isUnlocked() const;

    // Encrypt `plaintext` and return the vault-format string.
    // Returns an empty string on failure or when the vault is locked.
    String encrypt(const String& plaintext);

    // Decrypt a vault-format string.
    // Returns the plaintext on success, or an empty string on failure
    // (e.g., wrong key, corrupted data, vault locked).
    String decrypt(const String& ciphertext);

    // True if `content` begins with the vault header (VAULT_HEADER).
    static bool isEncryptedContent(const String& content);

    static constexpr const char* VAULT_HEADER = "---vault-v2---\n";
    static constexpr const char* VAULT_HEADER_V1 = "---vault-v1---\n";
    static constexpr const char* VAULT_HEADER_V2 = VAULT_HEADER;

private:
    struct LockState {
        uint32_t failedAttempts = 0;
        uint32_t lockoutUntil   = 0;
    };

    uint8_t _key[32];
    UnlockResult _lastUnlockResult = UnlockResult::Error;
    uint8_t _legacyKey[32];

    // Load (or generate + save) the 16-byte PBKDF2 salt from NVS.
    // Returns true on success.
    bool _loadOrCreateSalt(uint8_t salt[16]);

    bool _loadPinVerifier(uint8_t* verifier, size_t len);
    bool _storePinVerifier(const uint8_t key[32]);
    bool _verifyDerivedKey(const uint8_t key[32], bool& matched);

    LockState _loadLockState() const;
    bool _saveLockState(const LockState& state) const;
    void _resetLockState();
    void _recordFailedAttempt();
    uint32_t _nowSeconds() const;
    uint32_t _retryAfterSeconds(const LockState& state) const;
};
