// ─────────────────────────────────────────────────────────────────────────────
// test/test_wifi_provisioning/test_wifi_provisioning.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "Preferences.h"

#include "wifi/WifiProvisioning.cpp" // NOLINT(bugprone-suspicious-include)

#include <unity.h>

void setUp(void) {
    Preferences::clearStore();
}

void tearDown(void) {}

void test_load_preferred_credentials_falls_back_to_compile_time_defaults(void) {
    WifiCredentials creds = WifiProvisioning::loadPreferredCredentials();
    TEST_ASSERT_EQUAL_STRING(WIFI_SSID, creds.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(WIFI_PASSWORD, creds.password.c_str());
    TEST_ASSERT_FALSE(creds.fromNvs);
}

void test_save_credentials_persists_nvs_values(void) {
    TEST_ASSERT_TRUE(WifiProvisioning::saveCredentials("HomeWiFi", "TopSecret123"));

    WifiCredentials creds = WifiProvisioning::loadPreferredCredentials();
    TEST_ASSERT_EQUAL_STRING("HomeWiFi", creds.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TopSecret123", creds.password.c_str());
    TEST_ASSERT_TRUE(creds.fromNvs);
}

void test_save_credentials_rejects_empty_ssid(void) {
    TEST_ASSERT_FALSE(WifiProvisioning::saveCredentials("", "password"));

    WifiCredentials creds = WifiProvisioning::loadPreferredCredentials();
    TEST_ASSERT_EQUAL_STRING(WIFI_SSID, creds.ssid.c_str());
    TEST_ASSERT_FALSE(creds.fromNvs);
}

void test_save_credentials_allows_open_network_password(void) {
    TEST_ASSERT_TRUE(WifiProvisioning::saveCredentials("CafeWiFi", ""));

    WifiCredentials creds = WifiProvisioning::loadPreferredCredentials();
    TEST_ASSERT_EQUAL_STRING("CafeWiFi", creds.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", creds.password.c_str());
    TEST_ASSERT_TRUE(creds.fromNvs);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_load_preferred_credentials_falls_back_to_compile_time_defaults);
    RUN_TEST(test_save_credentials_persists_nvs_values);
    RUN_TEST(test_save_credentials_rejects_empty_ssid);
    RUN_TEST(test_save_credentials_allows_open_network_password);
    return UNITY_END();
}
