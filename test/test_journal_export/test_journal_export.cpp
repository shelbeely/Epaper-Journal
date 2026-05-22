// ─────────────────────────────────────────────────────────────────────────────
// test/test_journal_export/test_journal_export.cpp
//
// Focused unit tests for ZIP-export helper logic shared by the web backup API.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "Preferences.h"

#include "vault/VaultManager.cpp"          // NOLINT(bugprone-suspicious-include)
#include "journal/JournalExport.cpp"       // NOLINT(bugprone-suspicious-include)

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_zip_path_strips_leading_slash(void) {
    const String result =
        JournalExport::zipPathFromEntryPath("/journal/2026/05/20260522-010000.md");
    TEST_ASSERT_EQUAL_STRING("journal/2026/05/20260522-010000.md", result.c_str());
}

void test_select_export_content_returns_raw_when_encrypted_mode(void) {
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));

    const String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nBody";
    const String encrypted = vault.encrypt(plaintext);

    const String result =
        JournalExport::selectExportContent(encrypted, &vault, true);
    TEST_ASSERT_EQUAL_STRING(encrypted.c_str(), result.c_str());
}

void test_select_export_content_decrypts_when_vault_is_unlocked(void) {
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));

    const String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nBody";
    const String encrypted = vault.encrypt(plaintext);

    const String result =
        JournalExport::selectExportContent(encrypted, &vault, false);
    TEST_ASSERT_EQUAL_STRING(plaintext.c_str(), result.c_str());
}

void test_select_export_content_preserves_raw_when_vault_is_locked(void) {
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));

    const String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nBody";
    const String encrypted = vault.encrypt(plaintext);
    vault.lock();

    const String result =
        JournalExport::selectExportContent(encrypted, &vault, false);
    TEST_ASSERT_EQUAL_STRING(encrypted.c_str(), result.c_str());
}

void test_select_export_content_preserves_raw_when_decrypt_fails(void) {
    VaultManager writerVault;
    VaultManager wrongVault;
    TEST_ASSERT_TRUE(writerVault.deriveKeyFromPin("1234"));
    TEST_ASSERT_TRUE(wrongVault.deriveKeyFromPin("9999"));

    const String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nBody";
    const String encrypted = writerVault.encrypt(plaintext);

    const String result =
        JournalExport::selectExportContent(encrypted, &wrongVault, false);
    TEST_ASSERT_EQUAL_STRING(encrypted.c_str(), result.c_str());
}

void test_crc32_matches_standard_reference_value(void) {
    const char* text = "123456789";
    const uint32_t crc =
        JournalExport::crc32(reinterpret_cast<const uint8_t*>(text), strlen(text));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_zip_path_strips_leading_slash);
    RUN_TEST(test_select_export_content_returns_raw_when_encrypted_mode);
    RUN_TEST(test_select_export_content_decrypts_when_vault_is_unlocked);
    RUN_TEST(test_select_export_content_preserves_raw_when_vault_is_locked);
    RUN_TEST(test_select_export_content_preserves_raw_when_decrypt_fails);
    RUN_TEST(test_crc32_matches_standard_reference_value);

    return UNITY_END();
}
