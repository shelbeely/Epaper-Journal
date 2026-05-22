// ─────────────────────────────────────────────────────────────────────────────
// test/test_title_prompt_screen/test_title_prompt_screen.cpp
//
// Unit tests for the pure formatting helpers in TitlePromptScreen.
// ─────────────────────────────────────────────────────────────────────────────

#include "Arduino.h"
#include "ui/TitlePromptScreen.h"

#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_format_date_option_uses_iso_date(void) {
    struct tm now;
    memset(&now, 0, sizeof(now));
    now.tm_year = 126;  // 2026
    now.tm_mon  = 4;    // May
    now.tm_mday = 22;

    const String result = TitlePromptScreen::formatDateOption(now);
    TEST_ASSERT_EQUAL_STRING("2026-05-22", result.c_str());
}

void test_format_prompt_option_keeps_short_prompt(void) {
    const String result = TitlePromptScreen::formatPromptOption("Write about joy");
    TEST_ASSERT_EQUAL_STRING("Write about joy", result.c_str());
}

void test_format_prompt_option_keeps_exactly_28_chars(void) {
    const String prompt = "1234567890123456789012345678";
    const String result = TitlePromptScreen::formatPromptOption(prompt);
    TEST_ASSERT_EQUAL_STRING("1234567890123456789012345678", result.c_str());
}

void test_format_prompt_option_truncates_to_28_chars(void) {
    const String result = TitlePromptScreen::formatPromptOption(
            "12345678901234567890123456789abcdef");
    TEST_ASSERT_EQUAL_STRING("1234567890123456789012345678", result.c_str());
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_format_date_option_uses_iso_date);
    RUN_TEST(test_format_prompt_option_keeps_short_prompt);
    RUN_TEST(test_format_prompt_option_keeps_exactly_28_chars);
    RUN_TEST(test_format_prompt_option_truncates_to_28_chars);

    return UNITY_END();
}
