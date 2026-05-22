// ─────────────────────────────────────────────────────────────────────────────
// test/test_journal_manager/test_journal_manager.cpp
//
// Unit tests for the pure static helper methods of JournalManager:
//   • JournalManager::labelFromFilename()  — human-readable label from path
//
// We compile JournalManager.cpp (and its dependencies X4Storage.cpp and
// X4Clock.cpp) directly into this translation unit by including the .cpp
// files after placing the mock headers on the include path via build_flags.
// ─────────────────────────────────────────────────────────────────────────────

// Mocks must come first so that angle-bracket includes resolve to our stubs.
#include "Arduino.h"
#include "Preferences.h"
#include "SDCardManager.h"

// Compile the sources under test directly.
// (build_src_filter = -<*> keeps the native test env clean; we own what
// gets compiled in each test directory.)
#include "vault/VaultManager.cpp"          // NOLINT(bugprone-suspicious-include)
#include "journal/JournalManager.cpp"   // NOLINT(bugprone-suspicious-include)
#include "storage/X4Storage.cpp"        // NOLINT
#include "system/X4Clock.cpp"           // NOLINT

#include <unity.h>

void setUp(void) {
    SDCardManager::_stubReady = false;
    SDCardManager::_stubEnsureDir = false;
    SDCardManager::_stubWrite = false;
    SDCardManager::_stubReadContent = String("");
}
void tearDown(void) {}

// ── labelFromFilename ─────────────────────────────────────────────────────────

// Standard filename without any path prefix.
void test_label_standard_filename(void) {
    const String result = JournalManager::labelFromFilename("20260520-103000.md");
    TEST_ASSERT_EQUAL_STRING("2026-05-20 10:30", result.c_str());
}

// Full SD path — the function must strip the directory part.
void test_label_full_path(void) {
    const String result = JournalManager::labelFromFilename(
            "/journal/2026/05/20260520-103000.md");
    TEST_ASSERT_EQUAL_STRING("2026-05-20 10:30", result.c_str());
}

// Different date/time to confirm character positions are correct.
void test_label_different_datetime(void) {
    const String result = JournalManager::labelFromFilename("20001231-235959.md");
    TEST_ASSERT_EQUAL_STRING("2000-12-31 23:59", result.c_str());
}

// Filename that is too short to be a timestamp → returned unchanged.
void test_label_filename_too_short(void) {
    // Length of "note.md" is 7, which is < 15.
    const String result = JournalManager::labelFromFilename("note.md");
    TEST_ASSERT_EQUAL_STRING("note.md", result.c_str());
}

// Filename of exactly 14 characters → returned unchanged (< 15 threshold).
void test_label_filename_exactly_14_chars(void) {
    // "14chars_only.x" == 14 characters
    const String result = JournalManager::labelFromFilename("14chars_only.x");
    TEST_ASSERT_EQUAL_STRING("14chars_only.x", result.c_str());
}

// Empty string input.
void test_label_empty_string(void) {
    const String result = JournalManager::labelFromFilename("");
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// Path where the filename itself is empty (trailing slash).
void test_label_trailing_slash(void) {
    // After the last '/' there is nothing → filename = "", length = 0 < 15
    const String result = JournalManager::labelFromFilename("/journal/2026/05/");
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// Filename without extension — still long enough to format.
void test_label_filename_no_extension(void) {
    // "20260101-120000" is 15 chars (just on the boundary), no ".md"
    // Formatter positions: [0-3]=YYYY, [4-5]=MM, [6-7]=DD, [9-10]=HH, [11-12]=MM
    // → year=2026, month=01, day=01, hour=12, min=00 → "2026-01-01 12:00"
    const String result = JournalManager::labelFromFilename("20260101-120000");
    TEST_ASSERT_EQUAL_STRING("2026-01-01 12:00", result.c_str());
}

// ── listAllPaths ──────────────────────────────────────────────────────────────

// When storage is not ready, listAllPaths must return an empty vector.
void test_list_all_paths_empty_when_not_ready(void) {
    SDCardManager::_stubReady = false;
    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);
    auto paths = jm.listAllPaths();
    TEST_ASSERT_EQUAL(0, (int)paths.size());
    SDCardManager::_stubReady = false; // restore
}

// When storage is ready but SD returns no files, listAllPaths still returns empty.
void test_list_all_paths_empty_no_files(void) {
    SDCardManager::_stubReady = true;
    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);
    auto paths = jm.listAllPaths();
    // SDCardManager stub returns empty lists → JournalManager should return {}
    TEST_ASSERT_EQUAL(0, (int)paths.size());
    SDCardManager::_stubReady = false;
}

// ── readEntryForExport ─────────────────────────────────────────────────────────

void test_read_entry_for_export_plaintext_returns_raw_content(void) {
    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);
    String out;
    String expected = "---\ntitle: Plain\ndate: 2026-05-22 01:00:00\n---\nHello\n";
    SDCardManager::_stubReadContent = expected;

    bool ok = jm.readEntryForExport("/journal/2026/05/plain.md", out, false);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), out.c_str());
}

void test_read_entry_for_export_encrypted_stays_raw_when_locked(void) {
    X4Storage storage;
    X4Clock clock;
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));
    String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nHidden\n";
    String encrypted = vault.encrypt(plaintext);
    vault.lock();
    JournalManager jm(storage, clock, &vault);
    String out;
    SDCardManager::_stubReadContent = encrypted;

    bool ok = jm.readEntryForExport("/journal/2026/05/secret.md", out, false);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(encrypted.c_str(), out.c_str());
}

void test_read_entry_for_export_encrypted_decrypts_when_unlocked(void) {
    X4Storage storage;
    X4Clock clock;
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));
    String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nHidden\n";
    String encrypted = vault.encrypt(plaintext);
    JournalManager jm(storage, clock, &vault);
    String out;
    SDCardManager::_stubReadContent = encrypted;

    bool ok = jm.readEntryForExport("/journal/2026/05/secret.md", out, false);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(plaintext.c_str(), out.c_str());
}

void test_read_entry_for_export_forced_raw_keeps_ciphertext(void) {
    X4Storage storage;
    X4Clock clock;
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));
    String plaintext = "---\ntitle: Secret\ndate: 2026-05-22 01:00:00\n---\nHidden\n";
    String encrypted = vault.encrypt(plaintext);
    JournalManager jm(storage, clock, &vault);
    String out;
    SDCardManager::_stubReadContent = encrypted;

    bool ok = jm.readEntryForExport("/journal/2026/05/secret.md", out, true);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(encrypted.c_str(), out.c_str());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_label_standard_filename);
    RUN_TEST(test_label_full_path);
    RUN_TEST(test_label_different_datetime);
    RUN_TEST(test_label_filename_too_short);
    RUN_TEST(test_label_filename_exactly_14_chars);
    RUN_TEST(test_label_empty_string);
    RUN_TEST(test_label_trailing_slash);
    RUN_TEST(test_label_filename_no_extension);
    RUN_TEST(test_list_all_paths_empty_when_not_ready);
    RUN_TEST(test_list_all_paths_empty_no_files);
    RUN_TEST(test_read_entry_for_export_plaintext_returns_raw_content);
    RUN_TEST(test_read_entry_for_export_encrypted_stays_raw_when_locked);
    RUN_TEST(test_read_entry_for_export_encrypted_decrypts_when_unlocked);
    RUN_TEST(test_read_entry_for_export_forced_raw_keeps_ciphertext);

    return UNITY_END();
}
