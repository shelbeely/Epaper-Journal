// ─────────────────────────────────────────────────────────────────────────────
// test/test_x4clock/test_x4clock.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "Preferences.h"
#include "WiFi.h"

#include "system/X4Clock.cpp"  // NOLINT(bugprone-suspicious-include)

#include <unity.h>

static void setTestEpoch(time_t epoch) {
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
}

void setUp(void) {
    setenv("TZ", "UTC", 1);
    tzset();
    Preferences::reset();
    setMockMillis(0);
    setMockConfigTimeEpoch(0);
    WiFi.setStatus(WL_IDLE_STATUS);
    setTestEpoch(0);
}

void tearDown(void) {}

void test_sync_persists_epoch_and_retries_hourly(void) {
    const uint32_t firstEpoch = 1735689600UL;   // 2025-01-01 00:00:00 UTC
    const uint32_t secondEpoch = 1767225600UL;  // 2026-01-01 00:00:00 UTC

    X4Clock clock;
    WiFi.setStatus(WL_CONNECTED);

    setMockConfigTimeEpoch(firstEpoch);
    TEST_ASSERT_TRUE(clock.sync(100));

    Preferences prefs;
    prefs.begin("system", true);
    TEST_ASSERT_EQUAL_UINT32(firstEpoch, prefs.getUInt("last_epoch", 0));
    prefs.end();

    setMockConfigTimeEpoch(secondEpoch);
    advanceMockMillis(30000);
    TEST_ASSERT_FALSE(clock.syncIfNeeded(100));

    prefs.begin("system", true);
    TEST_ASSERT_EQUAL_UINT32(firstEpoch, prefs.getUInt("last_epoch", 0));
    prefs.end();

    advanceMockMillis(3600000);
    TEST_ASSERT_TRUE(clock.syncIfNeeded(100));

    prefs.begin("system", true);
    TEST_ASSERT_EQUAL_UINT32(secondEpoch, prefs.getUInt("last_epoch", 0));
    prefs.end();
}

void test_sync_restores_persisted_epoch_after_failed_ntp(void) {
    const uint32_t fallbackEpoch = 1767225600UL;  // 2026-01-01 00:00:00 UTC
    const uint32_t staleRtcEpoch = 1703980800UL;  // 2023-12-31 00:00:00 UTC

    Preferences prefs;
    prefs.begin("system", false);
    prefs.putUInt("last_epoch", fallbackEpoch);
    prefs.end();

    setTestEpoch(staleRtcEpoch);

    X4Clock clock;
    TEST_ASSERT_FALSE(clock.sync(200));

    uint16_t year = 0;
    uint8_t month = 0;
    clock.currentYearMonth(year, month);
    TEST_ASSERT_EQUAL_UINT16(2026, year);
    TEST_ASSERT_EQUAL_UINT8(1, month);
    TEST_ASSERT_EQUAL_UINT16(2026, clock.effectiveCurrentYear());
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_sync_persists_epoch_and_retries_hourly);
    RUN_TEST(test_sync_restores_persisted_epoch_after_failed_ntp);

    return UNITY_END();
}
