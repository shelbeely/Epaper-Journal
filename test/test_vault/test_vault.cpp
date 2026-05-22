// ─────────────────────────────────────────────────────────────────────────────
// test/test_vault/test_vault.cpp
//
// Focused unit tests for VaultManager's critical encryption and key-derivation
// behavior.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "Preferences.h"

#define private public
#include "vault/VaultManager.h"
#undef private
#include "vault/VaultManager.cpp"  // NOLINT(bugprone-suspicious-include)

#include <unity.h>
#include <cstring>

void setUp(void) {
    Preferences::clearStore();
    _stubNonceCounter = 0;
}

void tearDown(void) {}

void test_encrypt_decrypt_roundtrip_returns_original_plaintext(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1234"));

    const String plaintext = "---\ntitle: Secret\n---\nEncrypted body.\n";
    const String ciphertext = vm.encrypt(plaintext);

    TEST_ASSERT_TRUE(VaultManager::isEncryptedContent(ciphertext));
    TEST_ASSERT_EQUAL_STRING(plaintext.c_str(), vm.decrypt(ciphertext).c_str());
}

void test_decrypt_with_wrong_key_returns_empty_string(void) {
    VaultManager writer;
    TEST_ASSERT_TRUE(writer.deriveKeyFromPin("1234"));
    const String ciphertext = writer.encrypt("hidden");

    // Clear the NVS store so that reader can set a new PIN verifier
    // (a second VaultManager can't unlock with a different PIN if a verifier exists)
    Preferences::clearStore();

    VaultManager reader;
    TEST_ASSERT_TRUE(reader.deriveKeyFromPin("9999"));
    TEST_ASSERT_TRUE(reader.decrypt(ciphertext).isEmpty());
}

void test_is_encrypted_content_identifies_vault_header(void) {
    TEST_ASSERT_TRUE(VaultManager::isEncryptedContent(String(VaultManager::VAULT_HEADER) + "abc\n"));
    TEST_ASSERT_FALSE(VaultManager::isEncryptedContent("---\ntitle: Plain\n---\n"));
}

void test_derive_key_from_pin_produces_non_zero_32_byte_key(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1234"));
    TEST_ASSERT_EQUAL_UINT32(32, sizeof(vm._key));

    bool hasNonZeroByte = false;
    for (uint8_t byte : vm._key) {
        if (byte != 0) {
            hasNonZeroByte = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(hasNonZeroByte);
}

void test_same_pin_and_salt_produce_same_key(void) {
    VaultManager first;
    TEST_ASSERT_TRUE(first.deriveKeyFromPin("1234"));
    uint8_t firstKey[32];
    std::memcpy(firstKey, first._key, sizeof(firstKey));

    VaultManager second;
    TEST_ASSERT_TRUE(second.deriveKeyFromPin("1234"));

    TEST_ASSERT_EQUAL_INT(0, std::memcmp(firstKey, second._key, sizeof(firstKey)));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_encrypt_decrypt_roundtrip_returns_original_plaintext);
    RUN_TEST(test_decrypt_with_wrong_key_returns_empty_string);
    RUN_TEST(test_is_encrypted_content_identifies_vault_header);
    RUN_TEST(test_derive_key_from_pin_produces_non_zero_32_byte_key);
    RUN_TEST(test_same_pin_and_salt_produce_same_key);

    return UNITY_END();
}
