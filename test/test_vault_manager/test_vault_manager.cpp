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

void setUp(void)  {
    Preferences::reset();
    setMillis(0);
}
void tearDown(void) {}

// ── isEncryptedContent ────────────────────────────────────────────────────────

void test_is_encrypted_content_detects_header(void) {
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

void test_wrong_pin_increments_failed_attempt_counter(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1234"));
    vm.lock();

    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("4321"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::InvalidPin,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_EQUAL_UINT32(1, vm.failedUnlockAttempts());
    TEST_ASSERT_FALSE(vm.isUnlockLockedOut());

    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("9999"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::InvalidPin,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_EQUAL_UINT32(2, vm.failedUnlockAttempts());
    TEST_ASSERT_FALSE(vm.isUnlockLockedOut());
}

void test_third_failed_pin_attempt_triggers_30_second_lockout(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1234"));
    vm.lock();

    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));

    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::LockedOut,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_TRUE(vm.isUnlockLockedOut());
    TEST_ASSERT_EQUAL_UINT32(3, vm.failedUnlockAttempts());
    TEST_ASSERT_EQUAL_UINT32(30, vm.unlockRetryAfterSeconds());
}

void test_lockout_blocks_unlock_until_retry_after_elapses(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("2468"));
    vm.lock();

    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("1111"));
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("1111"));
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("1111"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::LockedOut,
                          (int)vm.lastUnlockResult());

    advanceMillis(15000);
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("2468"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::LockedOut,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_EQUAL_UINT32(15, vm.unlockRetryAfterSeconds());

    advanceMillis(15000);
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("2468"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::Success,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_EQUAL_UINT32(0, vm.failedUnlockAttempts());
    TEST_ASSERT_FALSE(vm.isUnlockLockedOut());
}

void test_lockout_window_escalates_to_five_minutes_then_one_hour(void) {
    VaultManager vm;
    TEST_ASSERT_TRUE(vm.deriveKeyFromPin("1357"));
    vm.lock();

    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_EQUAL_UINT32(30, vm.unlockRetryAfterSeconds());

    advanceMillis(30000);
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::LockedOut,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_EQUAL_UINT32(4, vm.failedUnlockAttempts());
    TEST_ASSERT_EQUAL_UINT32(300, vm.unlockRetryAfterSeconds());

    advanceMillis(300000);
    TEST_ASSERT_FALSE(vm.deriveKeyFromPin("0000"));
    TEST_ASSERT_EQUAL_INT((int)VaultManager::UnlockResult::LockedOut,
                          (int)vm.lastUnlockResult());
    TEST_ASSERT_EQUAL_UINT32(5, vm.failedUnlockAttempts());
    TEST_ASSERT_EQUAL_UINT32(3600, vm.unlockRetryAfterSeconds());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_is_encrypted_content_detects_header);
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
    RUN_TEST(test_wrong_pin_increments_failed_attempt_counter);
    RUN_TEST(test_third_failed_pin_attempt_triggers_30_second_lockout);
    RUN_TEST(test_lockout_blocks_unlock_until_retry_after_elapses);
    RUN_TEST(test_lockout_window_escalates_to_five_minutes_then_one_hour);

    return UNITY_END();
}
