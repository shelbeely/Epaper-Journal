// ─────────────────────────────────────────────────────────────────────────────
// test/test_ota/test_ota.cpp
//
// Focused unit tests for OtaManager's boot-state and rollback-safety behavior.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "Preferences.h"
#include "HTTPClient.h"
#include "WiFiClientSecure.h"
#include "esp_ota_ops.h"

#include "ota/OtaManager.cpp"  // NOLINT(bugprone-suspicious-include)

#include <unity.h>

void setUp(void) {
    Preferences::clearStore();
    esp_ota_mock_reset();
}

void tearDown(void) {}

void test_boot_count_increments_correctly(void) {
    OtaManager firstBoot;
    TEST_ASSERT_FALSE(firstBoot.onBoot());
    TEST_ASSERT_EQUAL_UINT32(1, firstBoot.bootCount());

    OtaManager secondBoot;
    TEST_ASSERT_FALSE(secondBoot.onBoot());
    TEST_ASSERT_EQUAL_UINT32(2, secondBoot.bootCount());
}

void test_crash_loop_detection_fires_at_threshold(void) {
    for (uint32_t boot = 1; boot < CRASH_LOOP_THRESHOLD; ++boot) {
        OtaManager manager;
        TEST_ASSERT_FALSE(manager.onBoot());
        TEST_ASSERT_EQUAL_UINT32(boot, manager.bootCount());
    }

    OtaManager thresholdBoot;
    TEST_ASSERT_TRUE(thresholdBoot.onBoot());
    TEST_ASSERT_EQUAL_UINT32(CRASH_LOOP_THRESHOLD, thresholdBoot.bootCount());
}

void test_is_pending_verify_only_for_pending_verify_state(void) {
    esp_ota_mock_set_running_partition_state(ESP_OTA_IMG_PENDING_VERIFY);
    OtaManager pendingVerify;
    TEST_ASSERT_FALSE(pendingVerify.onBoot());
    TEST_ASSERT_TRUE(pendingVerify.isPendingVerify());

    Preferences::clearStore();
    esp_ota_mock_reset();
    esp_ota_mock_set_running_partition_state(ESP_OTA_IMG_VALID);
    OtaManager valid;
    TEST_ASSERT_FALSE(valid.onBoot());
    TEST_ASSERT_FALSE(valid.isPendingVerify());

    Preferences::clearStore();
    esp_ota_mock_reset();
    esp_ota_mock_set_running_partition_state(ESP_OTA_IMG_UNDEFINED);
    OtaManager undefined;
    TEST_ASSERT_FALSE(undefined.onBoot());
    TEST_ASSERT_FALSE(undefined.isPendingVerify());
}

void test_mark_boot_healthy_clears_pending_verify_state(void) {
    esp_ota_mock_set_running_partition_state(ESP_OTA_IMG_PENDING_VERIFY);

    OtaManager manager;
    TEST_ASSERT_FALSE(manager.onBoot());
    TEST_ASSERT_TRUE(manager.isPendingVerify());

    manager.markBootHealthy();

    TEST_ASSERT_TRUE(esp_ota_mock_mark_valid_called);
    TEST_ASSERT_FALSE(manager.isPendingVerify());

    OtaManager nextBoot;
    TEST_ASSERT_FALSE(nextBoot.onBoot());
    TEST_ASSERT_EQUAL_UINT32(1, nextBoot.bootCount());
    TEST_ASSERT_FALSE(nextBoot.isPendingVerify());
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_boot_count_increments_correctly);
    RUN_TEST(test_crash_loop_detection_fires_at_threshold);
    RUN_TEST(test_is_pending_verify_only_for_pending_verify_state);
    RUN_TEST(test_mark_boot_healthy_clears_pending_verify_state);

    return UNITY_END();
}
