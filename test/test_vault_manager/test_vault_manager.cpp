// ─────────────────────────────────────────────────────────────────────────────
// test/test_vault_manager/test_vault_manager.cpp
//
// Unit tests for VaultManager — encrypt/decrypt, lock/unlock, format detection.
// Uses native stubs for mbedTLS (test/mocks/mbedtls/) and Preferences.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"

// Include the VaultManager implementation directly (no library step needed)
#include "vault/VaultManager.h"
#include "vault/VaultManager.cpp"

#include <unity.h>
#include <string.h>
#include <vector>

void setUp(void)  {}
void tearDown(void) {}

static String makeLegacyCiphertext(const char* pin, const String& plaintext) {
    uint8_t salt[16] = {0};
    Preferences prefs;
    prefs.begin("vault", false);
    size_t n = prefs.getBytes("salt", salt, sizeof(salt));
    prefs.end();
    if (n != sizeof(salt)) return "";

    uint8_t key[32] = {0};
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&md_ctx, md_info, 1) != 0) {
        mbedtls_md_free(&md_ctx);
        return "";
    }

    int rc = mbedtls_pkcs5_pbkdf2_hmac(
        &md_ctx,
        reinterpret_cast<const unsigned char*>(pin), strlen(pin),
        salt, sizeof(salt),
        10000,
        sizeof(key), key);
    mbedtls_md_free(&md_ctx);
    if (rc != 0) return "";

    const size_t ptLen = plaintext.length();
    std::vector<uint8_t> blob(12 + 16 + ptLen);
    uint8_t* nonce = blob.data();
    uint8_t* tag   = blob.data() + 12;
    uint8_t* ct    = blob.data() + 12 + 16;
    memset(nonce, 0, 12);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    rc = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, ptLen,
        nonce, 12, nullptr, 0,
        reinterpret_cast<const uint8_t*>(plaintext.c_str()), ct,
        16, tag);
    mbedtls_gcm_free(&gcm);
    if (rc != 0) return "";

    String result = VaultManager::VAULT_HEADER_V1;
    result += b64Enc(blob.data(), blob.size());
    result += "\n";
    return result;
}

// ── isEncryptedContent ────────────────────────────────────────────────────────

void test_is_encrypted_content_detects_header(void) {
    String enc = "---vault-v2---\nABC123\n";
    TEST_ASSERT_TRUE(VaultManager::isEncryptedContent(enc));
}

void test_is_encrypted_content_detects_legacy_header(void) {
    String enc = "---vault-v1---\nABC123\n";
    TEST_ASSERT_TRUE(VaultManager::isEncryptedContent(enc));
}

void test_is_encrypted_content_rejects_plaintext(void) {
    String plain = "---\ntitle: Hello\n---\n# Body\n";
    TEST_ASSERT_FALSE(VaultManager::isEncryptedContent(plain));
}

void test_is_encrypted_content_rejects_empty(void) {
    TEST_ASSERT_FALSE(VaultManager::isEncryptedContent(""));
}

// ── lock / isUnlocked ─────────────────────────────────────────────────────────

void test_initially_locked(void) {
    VaultManager vm;
    TEST_ASSERT_FALSE(vm.isUnlocked());
}

void test_unlocked_after_derive_key(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1234"));
    TEST_ASSERT_TRUE(vm.isUnlocked());
}

void test_locked_after_lock(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("1234");
    vm.lock();
    TEST_ASSERT_FALSE(vm.isUnlocked());
}

void test_derive_empty_pin_fails(void) {
    VaultManager vm;
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin(""));
    TEST_ASSERT_FALSE(vm.isUnlocked());
}

void test_derive_null_pin_fails(void) {
    VaultManager vm;
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin(nullptr));
    TEST_ASSERT_FALSE(vm.isUnlocked());
}

// ── encrypt ───────────────────────────────────────────────────────────────────

void test_encrypt_fails_when_locked(void) {
    VaultManager vm;
    String result = vm.encrypt("hello");
    TEST_ASSERT_TRUE(result.isEmpty());
}

void test_encrypt_fails_for_empty_plaintext(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("1234");
    String result = vm.encrypt("");
    TEST_ASSERT_TRUE(result.isEmpty());
}

void test_encrypt_produces_vault_header(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("1234");
    String result = vm.encrypt("test content");
    TEST_ASSERT_TRUE(VaultManager::isEncryptedContent(result));
}

// ── decrypt ───────────────────────────────────────────────────────────────────

void test_decrypt_fails_when_locked(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("1234");
    String ciphertext = vm.encrypt("secret");
    vm.lock();
    String result = vm.decrypt(ciphertext);
    TEST_ASSERT_TRUE(result.isEmpty());
}

void test_decrypt_fails_for_plain_text(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("1234");
    String result = vm.decrypt("---\ntitle: Hello\n---\nbody\n");
    TEST_ASSERT_TRUE(result.isEmpty());
}

// ── roundtrip ─────────────────────────────────────────────────────────────────

void test_roundtrip_short_text(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("9999");
    const char* plain = "Hello, Vault!";
    String cipher = vm.encrypt(plain);
    TEST_ASSERT_FALSE(cipher.isEmpty());
    String result = vm.decrypt(cipher);
    TEST_ASSERT_EQUAL_STRING(plain, result.c_str());
}

void test_roundtrip_frontmatter(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("0000");
    String plain = "---\ntitle: My Secret\ndate: 2026-05-20 10:30:00\n---\nPrivate body.\n";
    String cipher = vm.encrypt(plain);
    String result = vm.decrypt(cipher);
    TEST_ASSERT_EQUAL_STRING(plain.c_str(), result.c_str());
}

void test_roundtrip_lock_unlock(void) {
    VaultManager vm;
    vm.deriveKeyFromPin("5678");
    String plain  = "Lock/unlock test";
    String cipher = vm.encrypt(plain);
    vm.lock();
    vm.deriveKeyFromPin("5678");
    String result = vm.decrypt(cipher);
    TEST_ASSERT_EQUAL_STRING(plain.c_str(), result.c_str());
}

// ── wrong key ─────────────────────────────────────────────────────────────────

void test_wrong_key_decrypt_fails(void) {
    VaultManager vm1;
    vm1.deriveKeyFromPin("1111");
    String cipher = vm1.encrypt("sensitive data");

    VaultManager vm2;
    vm2.deriveKeyFromPin("2222");
    String result = vm2.decrypt(cipher);
    // Different PIN → different key → GCM auth tag mismatch → empty
    TEST_ASSERT_TRUE(result.isEmpty());
}

void test_decrypt_legacy_v1_content_succeeds(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1234"));

    String legacyCipher = makeLegacyCiphertext("1234", "legacy secret");
    TEST_ASSERT_FALSE(legacyCipher.isEmpty());
    TEST_ASSERT_TRUE(legacyCipher.startsWith(VaultManager::VAULT_HEADER_V1));

    String result = vm.decrypt(legacyCipher);
    TEST_ASSERT_EQUAL_STRING("legacy secret", result.c_str());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_is_encrypted_content_detects_header);
    RUN_TEST(test_is_encrypted_content_detects_legacy_header);
    RUN_TEST(test_is_encrypted_content_rejects_plaintext);
    RUN_TEST(test_is_encrypted_content_rejects_empty);
    RUN_TEST(test_initially_locked);
    RUN_TEST(test_unlocked_after_derive_key);
    RUN_TEST(test_locked_after_lock);
    RUN_TEST(test_derive_empty_pin_fails);
    RUN_TEST(test_derive_null_pin_fails);
    RUN_TEST(test_encrypt_fails_when_locked);
    RUN_TEST(test_encrypt_fails_for_empty_plaintext);
    RUN_TEST(test_encrypt_produces_vault_header);
    RUN_TEST(test_decrypt_fails_when_locked);
    RUN_TEST(test_decrypt_fails_for_plain_text);
    RUN_TEST(test_roundtrip_short_text);
    RUN_TEST(test_roundtrip_frontmatter);
    RUN_TEST(test_roundtrip_lock_unlock);
    RUN_TEST(test_wrong_key_decrypt_fails);
    RUN_TEST(test_decrypt_legacy_v1_content_succeeds);

    return UNITY_END();
}
