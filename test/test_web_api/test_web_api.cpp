// ─────────────────────────────────────────────────────────────────────────────
// test/test_web_api/test_web_api.cpp
//
// Unit tests for the POST /api/journal/new handler logic.
//
// WebApi itself depends on AsyncWebServer (ESP32-only), so we cannot compile
// it in the native environment.  Instead we test the core logic it invokes:
// JournalManager::createEntry() — the function called by the route handler to
// create an entry and return its path.
//
// The SDCardManager mock exposes configurable stub flags so we can exercise
// both the success path and the storage-failure path that map to the handler's
// 201 vs 503 responses.
// ─────────────────────────────────────────────────────────────────────────────

// Mocks must come before any source file that pulls in hardware headers.
#include "Arduino.h"
#include "Preferences.h"
#include "SDCardManager.h"

// Compile the sources under test directly.
#include "journal/JournalManager.cpp"  // NOLINT(bugprone-suspicious-include)
#include "storage/X4Storage.cpp"       // NOLINT
#include "system/X4Clock.cpp"          // NOLINT

#include <unity.h>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Reset mock flags to "storage not ready" state before each test.
void setUp(void) {
    SDCardManager::_stubReady     = false;
    SDCardManager::_stubEnsureDir = false;
    SDCardManager::_stubWrite     = false;
    SDCardManager::_stubReadContent = String("");
}

void tearDown(void) {}

// Build a shared JournalManager backed by the mock storage/clock.
static X4Storage  g_storage;
static X4Clock    g_clock;
static JournalManager g_jm(g_storage, g_clock);

// ── createEntry — storage failure paths ───────────────────────────────────────

// Storage not ready → createEntry returns "" (handler would respond 503).
void test_create_entry_storage_not_ready(void) {
    SDCardManager::_stubReady = false;
    String path = g_jm.createEntry("Test Entry");
    TEST_ASSERT_TRUE(path.isEmpty());
}

// Storage ready but ensureDirectory fails → createEntry returns "".
void test_create_entry_ensure_dir_fails(void) {
    SDCardManager::_stubReady     = true;
    SDCardManager::_stubEnsureDir = false;   // directory creation fails
    SDCardManager::_stubWrite     = true;
    String path = g_jm.createEntry("Test Entry");
    TEST_ASSERT_TRUE(path.isEmpty());
}

// Storage ready, directory ok, but write fails → createEntry returns "".
void test_create_entry_write_fails(void) {
    SDCardManager::_stubReady     = true;
    SDCardManager::_stubEnsureDir = true;
    SDCardManager::_stubWrite     = false;   // write fails
    String path = g_jm.createEntry("Test Entry");
    TEST_ASSERT_TRUE(path.isEmpty());
}

// ── createEntry — success path ────────────────────────────────────────────────

// Storage ready and write succeeds → path is non-empty (handler responds 201).
void test_create_entry_success_returns_path(void) {
    SDCardManager::_stubReady     = true;
    SDCardManager::_stubEnsureDir = true;
    SDCardManager::_stubWrite     = true;
    String path = g_jm.createEntry("My Entry");
    TEST_ASSERT_FALSE(path.isEmpty());
}

// Returned path must start with "/journal/".
void test_create_entry_path_has_journal_prefix(void) {
    SDCardManager::_stubReady     = true;
    SDCardManager::_stubEnsureDir = true;
    SDCardManager::_stubWrite     = true;
    String path = g_jm.createEntry("My Entry");
    TEST_ASSERT_TRUE(path.startsWith("/journal/"));
}

// Returned path must end with ".md".
void test_create_entry_path_ends_with_md(void) {
    SDCardManager::_stubReady     = true;
    SDCardManager::_stubEnsureDir = true;
    SDCardManager::_stubWrite     = true;
    String path = g_jm.createEntry("My Entry");
    TEST_ASSERT_TRUE(path.endsWith(".md"));
}

// An empty title falls back to "New Entry" (same path-generation logic).
void test_create_entry_empty_title_uses_default(void) {
    SDCardManager::_stubReady     = true;
    SDCardManager::_stubEnsureDir = true;
    SDCardManager::_stubWrite     = true;
    // createEntry with empty title should still produce a valid path
    String path = g_jm.createEntry("");
    TEST_ASSERT_FALSE(path.isEmpty());
    TEST_ASSERT_TRUE(path.endsWith(".md"));
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    // Storage failure paths
    RUN_TEST(test_create_entry_storage_not_ready);
    RUN_TEST(test_create_entry_ensure_dir_fails);
    RUN_TEST(test_create_entry_write_fails);

    // Success paths
    RUN_TEST(test_create_entry_success_returns_path);
    RUN_TEST(test_create_entry_path_has_journal_prefix);
    RUN_TEST(test_create_entry_path_ends_with_md);
    RUN_TEST(test_create_entry_empty_title_uses_default);

    return UNITY_END();
}
