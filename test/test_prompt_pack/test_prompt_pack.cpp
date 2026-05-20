// ─────────────────────────────────────────────────────────────────────────────
// test/test_prompt_pack/test_prompt_pack.cpp
//
// Unit tests for PromptPack — no hardware required, no mocks needed.
// ─────────────────────────────────────────────────────────────────────────────

// Arduino stub must come first to satisfy any indirect includes.
#include "Arduino.h"

// PromptPack is header-only; it only needs <stdint.h> and <time.h>
#include "journal/PromptPack.h"

#include <unity.h>
#include <string.h>
#include <time.h>

void setUp(void)  {}
void tearDown(void) {}

// ── Constant checks ────────────────────────────────────────────────────────────

void test_num_prompts_is_correct(void) {
    TEST_ASSERT_EQUAL(30, PromptPack::NUM_PROMPTS);
}

void test_array_size_matches_constant(void) {
    // Verify the last element (index 29) is non-null via getPrompt().
    const char* p = PromptPack::getPrompt(PromptPack::NUM_PROMPTS - 1);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_GREATER_THAN(0, (int)strlen(p));
}

// ── getPrompt ──────────────────────────────────────────────────────────────────

void test_get_prompt_index_zero(void) {
    const char* p = PromptPack::getPrompt(0);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_GREATER_THAN(0, (int)strlen(p));
}

void test_get_prompt_wraps_at_num_prompts(void) {
    // getPrompt(N) and getPrompt(0) must return the same string pointer
    // because N % NUM_PROMPTS == 0.
    const char* p0 = PromptPack::getPrompt(0);
    const char* pN = PromptPack::getPrompt(PromptPack::NUM_PROMPTS);
    TEST_ASSERT_EQUAL_PTR(p0, pN);
}

void test_get_prompt_wraps_arbitrarily(void) {
    for (uint8_t i = 0; i < PromptPack::NUM_PROMPTS; i++) {
        const char* pa = PromptPack::getPrompt(i);
        const char* pb = PromptPack::getPrompt((uint32_t)i + PromptPack::NUM_PROMPTS * 7u);
        TEST_ASSERT_EQUAL_PTR(pa, pb);
    }
}

void test_all_prompts_non_empty(void) {
    for (uint8_t i = 0; i < PromptPack::NUM_PROMPTS; i++) {
        const char* p = PromptPack::getPrompt(i);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_GREATER_THAN(0, (int)strlen(p));
    }
}

// ── today ──────────────────────────────────────────────────────────────────────

void test_today_returns_non_null(void) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = 126;  // 2026
    t.tm_mon  = 4;    // May
    t.tm_mday = 20;
    t.tm_yday = 139;  // day 140 of 2026 (0-based)
    const char* p = PromptPack::today(t);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_GREATER_THAN(0, (int)strlen(p));
}

void test_today_is_consistent_same_day(void) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = 126;
    t.tm_mon  = 0;
    t.tm_mday = 1;
    t.tm_yday = 0;
    const char* p1 = PromptPack::today(t);
    const char* p2 = PromptPack::today(t);
    TEST_ASSERT_EQUAL_PTR(p1, p2);
}

void test_today_differs_between_years_for_same_yday(void) {
    struct tm t2026, t2027;
    memset(&t2026, 0, sizeof(t2026));
    memset(&t2027, 0, sizeof(t2027));
    t2026.tm_year = 126; t2026.tm_yday = 10;
    t2027.tm_year = 127; t2027.tm_yday = 10;
    const char* p2026 = PromptPack::today(t2026);
    const char* p2027 = PromptPack::today(t2027);
    // Same yday but different year must yield a different prompt (year offset
    // is 365 which is not a multiple of NUM_PROMPTS=30 since 365 % 30 = 5).
    TEST_ASSERT_NOT_EQUAL(p2026, p2027);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_num_prompts_is_correct);
    RUN_TEST(test_array_size_matches_constant);
    RUN_TEST(test_get_prompt_index_zero);
    RUN_TEST(test_get_prompt_wraps_at_num_prompts);
    RUN_TEST(test_get_prompt_wraps_arbitrarily);
    RUN_TEST(test_all_prompts_non_empty);
    RUN_TEST(test_today_returns_non_null);
    RUN_TEST(test_today_is_consistent_same_day);
    RUN_TEST(test_today_differs_between_years_for_same_yday);

    return UNITY_END();
}
