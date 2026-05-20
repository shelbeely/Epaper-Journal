#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// VaultManager.h — AES-256-GCM encrypted journal vault
//
// Provides per-session PIN-based encryption for journal entries.
// Key derivation: PBKDF2-SHA256(pin, salt, 10000) → 32-byte AES key.
// The 16-byte salt is generated once and persisted in NVS.
//
// Encrypted file format (stored on SD card):
//   ---vault-v1---
//   <base64(nonce[12] | tag[16] | ciphertext[N])>
//
// The plaintext that is encrypted is the full YAML-frontmatter Markdown file.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>

class VaultManager {
public:
    VaultManager();
    ~VaultManager();

    // Derive the 32-byte AES key from `pin` using PBKDF2-SHA256 and the
    // NVS-persisted salt. Creates and saves the salt on first call.
    // Returns true on success; the vault is unlocked after a successful call.
    bool deriveKeyFromPin(const char* pin);

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

    static constexpr const char* VAULT_HEADER = "---vault-v1---\n";

private:
    uint8_t _key[32];

    // Load (or generate + save) the 16-byte PBKDF2 salt from NVS.
    // Returns true on success.
    bool _loadOrCreateSalt(uint8_t salt[16]);
};
