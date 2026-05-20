// ─────────────────────────────────────────────────────────────────────────────
// test/test_ota_manager/test_ota_manager.cpp
//
// Unit tests for the pure static methods of OtaManager:
//   • OtaManager::isNewerVersion()  — semantic version comparison
//   • OtaManager::resultString()    — OtaResult → human-readable string
//
// We compile OtaManager.cpp directly; hardware-specific headers (esp_ota_ops,
// mbedtls, HTTPClient, etc.) are satisfied by stubs in test/mocks/.
// ─────────────────────────────────────────────────────────────────────────────

// Hardware stubs — must be included before any ESP32-specific header is
// pulled in transitively through OtaManager.h / OtaManager.cpp.
#include "Arduino.h"
#include "Preferences.h"
#include "HTTPClient.h"
#include "WiFiClientSecure.h"
#include "esp_ota_ops.h"
// mbedtls/md.h is included via angle brackets in OtaManager.cpp; the
// -I test/mocks flag ensures our stub directory is searched first.

// Compile the source under test directly.
#include "ota/OtaManager.cpp"   // NOLINT(bugprone-suspicious-include)

#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// ── isNewerVersion() ─────────────────────────────────────────────────────────

void test_version_newer_patch(void) {
    TEST_ASSERT_TRUE(OtaManager::isNewerVersion("1.0.1", "1.0.0"));
}

void test_version_older_patch(void) {
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("1.0.0", "1.0.1"));
}

void test_version_newer_minor(void) {
    TEST_ASSERT_TRUE(OtaManager::isNewerVersion("1.1.0", "1.0.9"));
}

void test_version_older_minor(void) {
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("1.0.9", "1.1.0"));
}

void test_version_newer_major(void) {
    TEST_ASSERT_TRUE(OtaManager::isNewerVersion("2.0.0", "1.9.9"));
}

void test_version_older_major(void) {
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("1.9.9", "2.0.0"));
}

void test_version_equal(void) {
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("1.0.0", "1.0.0"));
}

void test_version_from_zero(void) {
    TEST_ASSERT_TRUE(OtaManager::isNewerVersion("0.0.1", "0.0.0"));
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("0.0.0", "0.0.1"));
}

void test_version_large_numbers(void) {
    TEST_ASSERT_TRUE(OtaManager::isNewerVersion("10.0.0", "9.99.99"));
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("9.99.99", "10.0.0"));
}

void test_version_minor_carries_over_patch(void) {
    // 1.2.0 > 1.1.100 because minor is higher
    TEST_ASSERT_TRUE(OtaManager::isNewerVersion("1.2.0", "1.1.100"));
    TEST_ASSERT_FALSE(OtaManager::isNewerVersion("1.1.100", "1.2.0"));
}

// ── resultString() ────────────────────────────────────────────────────────────

void test_result_string_ok(void) {
    TEST_ASSERT_EQUAL_STRING("ok", OtaManager::resultString(OtaResult::OK));
}

void test_result_string_already_current(void) {
    TEST_ASSERT_EQUAL_STRING("already_current",
                             OtaManager::resultString(OtaResult::ALREADY_CURRENT));
}

void test_result_string_wrong_device(void) {
    TEST_ASSERT_EQUAL_STRING("wrong_device",
                             OtaManager::resultString(OtaResult::WRONG_DEVICE));
}

void test_result_string_wrong_channel(void) {
    TEST_ASSERT_EQUAL_STRING("wrong_channel",
                             OtaManager::resultString(OtaResult::WRONG_CHANNEL));
}

void test_result_string_low_battery(void) {
    TEST_ASSERT_EQUAL_STRING("low_battery",
                             OtaManager::resultString(OtaResult::LOW_BATTERY));
}

void test_result_string_insufficient_space(void) {
    TEST_ASSERT_EQUAL_STRING("insufficient_space",
                             OtaManager::resultString(OtaResult::INSUFFICIENT_SPACE));
}

void test_result_string_download_failed(void) {
    TEST_ASSERT_EQUAL_STRING("download_failed",
                             OtaManager::resultString(OtaResult::DOWNLOAD_FAILED));
}

void test_result_string_hash_mismatch(void) {
    TEST_ASSERT_EQUAL_STRING("hash_mismatch",
                             OtaManager::resultString(OtaResult::HASH_MISMATCH));
}

void test_result_string_flash_failed(void) {
    TEST_ASSERT_EQUAL_STRING("flash_failed",
                             OtaManager::resultString(OtaResult::FLASH_FAILED));
}

void test_result_string_manifest_fetch_failed(void) {
    TEST_ASSERT_EQUAL_STRING("manifest_fetch_failed",
                             OtaManager::resultString(OtaResult::MANIFEST_FETCH_FAILED));
}

void test_result_string_manifest_parse_failed(void) {
    TEST_ASSERT_EQUAL_STRING("manifest_parse_failed",
                             OtaManager::resultString(OtaResult::MANIFEST_PARSE_FAILED));
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    // isNewerVersion
    RUN_TEST(test_version_newer_patch);
    RUN_TEST(test_version_older_patch);
    RUN_TEST(test_version_newer_minor);
    RUN_TEST(test_version_older_minor);
    RUN_TEST(test_version_newer_major);
    RUN_TEST(test_version_older_major);
    RUN_TEST(test_version_equal);
    RUN_TEST(test_version_from_zero);
    RUN_TEST(test_version_large_numbers);
    RUN_TEST(test_version_minor_carries_over_patch);

    // resultString
    RUN_TEST(test_result_string_ok);
    RUN_TEST(test_result_string_already_current);
    RUN_TEST(test_result_string_wrong_device);
    RUN_TEST(test_result_string_wrong_channel);
    RUN_TEST(test_result_string_low_battery);
    RUN_TEST(test_result_string_insufficient_space);
    RUN_TEST(test_result_string_download_failed);
    RUN_TEST(test_result_string_hash_mismatch);
    RUN_TEST(test_result_string_flash_failed);
    RUN_TEST(test_result_string_manifest_fetch_failed);
    RUN_TEST(test_result_string_manifest_parse_failed);

    return UNITY_END();
}
