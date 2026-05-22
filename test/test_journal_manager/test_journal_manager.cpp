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
#include <algorithm>

static void setTestEpoch(time_t epoch) {
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
}

void setUp(void) {
    setenv("TZ", "UTC", 1);
    tzset();
    Preferences::reset();
    SDCardManager::_stubReady = false;
    SDCardManager::_stubEnsureDir = false;
    SDCardManager::_stubWrite = false;
    SDCardManager::_stubRename = false;
    SDCardManager::_stubRemove = false;
    SDCardManager::_stubReadContent = String("");
    SDCardManager::_stubListFilesDefault.clear();
    SDCardManager::_stubListFilesByPath.clear();
    SDCardManager::_stubFileContents.clear();
    SDCardManager::_stubLastWritePath = String("");
    SDCardManager::_stubLastWriteContent = String("");
    SDCardManager::_stubWriteCallCount = 0;
    SDCardManager::_stubLastRenameSrc = String("");
    SDCardManager::_stubLastRenameDst = String("");
    SDCardManager::_stubRenameCallCount = 0;
    SDCardManager::_stubRemovedPaths.clear();
    setMillis(0);
    setMockConfigTimeEpoch(0);
    setTestEpoch(0);
}
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

void test_save_entry_writes_tmp_then_renames(void) {
    SDCardManager::_stubWrite = true;
    SDCardManager::_stubRename = true;
    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);

    JournalEntry e;
    e.title = "Atomic";
    e.date = "2026-05-22T00:00:00Z";
    e.body = "Body";

    const String path = "/journal/2026/05/20260522-000000.md";
    const bool ok = jm.saveEntry(path, e);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, SDCardManager::_stubWriteCallCount);
    TEST_ASSERT_EQUAL_INT(1, SDCardManager::_stubRenameCallCount);
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260522-000000.md.tmp",
                             SDCardManager::_stubLastWritePath.c_str());
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260522-000000.md.tmp",
                             SDCardManager::_stubLastRenameSrc.c_str());
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260522-000000.md",
                             SDCardManager::_stubLastRenameDst.c_str());
}

void test_save_entry_cleans_tmp_when_rename_fails(void) {
    SDCardManager::_stubWrite = true;
    SDCardManager::_stubRename = false;
    SDCardManager::_stubRemove = true;
    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);

    JournalEntry e;
    e.title = "Atomic";
    e.date = "2026-05-22T00:00:00Z";
    e.body = "Body";

    const String path = "/journal/2026/05/20260522-000000.md";
    const bool ok = jm.saveEntry(path, e);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(1, SDCardManager::_stubRenameCallCount);
    TEST_ASSERT_EQUAL_INT(1, (int)SDCardManager::_stubRemovedPaths.size());
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260522-000000.md.tmp",
                             SDCardManager::_stubRemovedPaths[0].c_str());
}

void test_begin_removes_orphaned_tmp_files(void) {
    SDCardManager::_stubReady = true;
    SDCardManager::_stubRemove = true;

    X4Storage storage;
    X4Clock clock;
    uint16_t year; uint8_t month;
    clock.currentYearMonth(year, month);

    char monthDir[24];
    snprintf(monthDir, sizeof(monthDir), "/journal/%04u/%02u/", year, month);
    SDCardManager::_stubListFilesByPath[std::string(monthDir)] = {
            "20260522-000001.md.tmp", "20260522-000002.md", "stale.tmp"};

    JournalManager jm(storage, clock, nullptr);
    jm.begin();

    TEST_ASSERT_EQUAL_INT(2, (int)SDCardManager::_stubRemovedPaths.size());
    String expectedA = String(monthDir) + "20260522-000001.md.tmp";
    String expectedB = String(monthDir) + "stale.tmp";
    TEST_ASSERT_TRUE(std::find(SDCardManager::_stubRemovedPaths.begin(),
                               SDCardManager::_stubRemovedPaths.end(),
                               expectedA) !=
                     SDCardManager::_stubRemovedPaths.end());
    TEST_ASSERT_TRUE(std::find(SDCardManager::_stubRemovedPaths.begin(),
                               SDCardManager::_stubRemovedPaths.end(),
                               expectedB) !=
                     SDCardManager::_stubRemovedPaths.end());
}

void test_load_entry_migrates_legacy_vault_to_v2(void) {
    SDCardManager::_stubReady = true;
    SDCardManager::_stubWrite = true;
    SDCardManager::_stubLastWritePath = "";
    SDCardManager::_stubLastWriteContent = "";

    X4Storage storage;
    X4Clock clock;
    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));

    const String plain = "---\ntitle: Legacy\ndate: 2026-05-22 10:00:00\n---\nBody\n";
    SDCardManager::_stubReadContent = makeLegacyCiphertext("1234", plain);
    TEST_ASSERT_TRUE(SDCardManager::_stubReadContent.startsWith(VaultManager::VAULT_HEADER_V1));

    JournalManager jm(storage, clock, &vault);
    JournalEntry out;
    TEST_ASSERT_TRUE(jm.loadEntry("/journal/2026/05/20260522-100000.md", out));
    TEST_ASSERT_FALSE(out.locked);
    TEST_ASSERT_EQUAL_STRING("Legacy", out.title.c_str());
    TEST_ASSERT_TRUE(SDCardManager::_stubLastWriteContent.startsWith(VaultManager::VAULT_HEADER_V2));
}

void test_search_entries_matches_title_body_and_is_case_insensitive(void) {
    SDCardManager::_stubReady = true;
    SDCardManager::_stubListFilesByPath.clear();
    SDCardManager::_stubFileContents.clear();
    SDCardManager::_stubListFilesByPath["/journal/2026/05/"] = {
        "20260520-103000.md",
        "20260521-103000.md"
    };
    SDCardManager::_stubFileContents["/journal/2026/05/20260520-103000.md"] =
        "---\n"
        "title: Grocery List\n"
        "date: 2026-05-20 10:30:00\n"
        "---\n"
        "Need to buy Milk and bread.\n";
    SDCardManager::_stubFileContents["/journal/2026/05/20260521-103000.md"] =
        "---\n"
        "title: Workout\n"
        "date: 2026-05-21 10:30:00\n"
        "---\n"
        "Morning run.\n";

    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);

    auto matches = jm.searchEntries("mIlK");
    TEST_ASSERT_EQUAL(1, (int)matches.size());
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260520-103000.md", matches[0].c_str());

    SDCardManager::_stubReady = false;
    SDCardManager::_stubListFilesByPath.clear();
    SDCardManager::_stubFileContents.clear();
}

void test_search_entries_only_uses_filename_for_locked_entries(void) {
    SDCardManager::_stubReady = true;
    SDCardManager::_stubListFilesByPath.clear();
    SDCardManager::_stubFileContents.clear();
    SDCardManager::_stubListFilesByPath["/journal/2026/05/"] = {
        "20260522-103000.md"
    };

    VaultManager vault;
    TEST_ASSERT_TRUE(vault.deriveKeyFromPin("1234"));
    String encrypted = vault.encrypt(
        "---\n"
        "title: Hidden Secret\n"
        "date: 2026-05-22 10:30:00\n"
        "---\n"
        "vault-only keyword\n");
    vault.lock();

    SDCardManager::_stubFileContents["/journal/2026/05/20260522-103000.md"] = encrypted;

    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, &vault);

    auto secretMatches = jm.searchEntries("keyword");
    TEST_ASSERT_EQUAL(0, (int)secretMatches.size());

    auto dateMatches = jm.searchEntries("2026-05-22");
    TEST_ASSERT_EQUAL(1, (int)dateMatches.size());
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260522-103000.md", dateMatches[0].c_str());

    SDCardManager::_stubReady = false;
    SDCardManager::_stubListFilesByPath.clear();
    SDCardManager::_stubFileContents.clear();
}

void test_search_entries_returns_empty_for_blank_query(void) {
    SDCardManager::_stubReady = true;
    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);
    auto matches = jm.searchEntries("   ");
    TEST_ASSERT_EQUAL(0, (int)matches.size());
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

// When the RTC is behind but a newer persisted epoch exists, listAllPaths must
// still scan far enough ahead to include recent entries.
void test_list_all_paths_uses_persisted_year_floor(void) {
    Preferences prefs;
    prefs.begin("system", false);
    prefs.putUInt("last_epoch", 1779235200UL);  // 2026-05-19 00:00:00 UTC
    prefs.end();

    setTestEpoch(1704067200UL);  // 2024-01-01 00:00:00 UTC
    SDCardManager::_stubReady = true;
    SDCardManager::_stubListFilesByPath["/journal/2026/05/"] = {
        "20260520-103000.md"
    };

    X4Storage storage;
    X4Clock clock;
    JournalManager jm(storage, clock, nullptr);
    auto paths = jm.listAllPaths();

    TEST_ASSERT_EQUAL(1, (int)paths.size());
    TEST_ASSERT_EQUAL_STRING("/journal/2026/05/20260520-103000.md",
                             paths[0].c_str());
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
    RUN_TEST(test_save_entry_writes_tmp_then_renames);
    RUN_TEST(test_save_entry_cleans_tmp_when_rename_fails);
    RUN_TEST(test_begin_removes_orphaned_tmp_files);
    RUN_TEST(test_load_entry_migrates_legacy_vault_to_v2);
    RUN_TEST(test_search_entries_matches_title_body_and_is_case_insensitive);
    RUN_TEST(test_search_entries_only_uses_filename_for_locked_entries);
    RUN_TEST(test_search_entries_returns_empty_for_blank_query);
    RUN_TEST(test_read_entry_for_export_plaintext_returns_raw_content);
    RUN_TEST(test_read_entry_for_export_encrypted_stays_raw_when_locked);
    RUN_TEST(test_read_entry_for_export_encrypted_decrypts_when_unlocked);
    RUN_TEST(test_read_entry_for_export_forced_raw_keeps_ciphertext);
    RUN_TEST(test_list_all_paths_uses_persisted_year_floor);

    return UNITY_END();
}
