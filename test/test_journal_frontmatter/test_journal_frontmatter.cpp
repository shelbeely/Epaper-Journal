// ─────────────────────────────────────────────────────────────────────────────
// test/test_journal_frontmatter/test_journal_frontmatter.cpp
//
// Unit tests for JournalFrontmatter::parse() and ::serialize().
//
// JournalFrontmatter is a header-only struct; it only depends on Arduino
// String, so we can compile it directly in the native test environment with
// our String mock.
// ─────────────────────────────────────────────────────────────────────────────

// Pull in the mock first — it must shadow <Arduino.h> for all subsequent
// includes.  The -I test/mocks build flag ensures angle-bracket includes also
// resolve here.
#include "Arduino.h"

// The unit under test (header-only, no .cpp needed)
#include "journal/JournalFrontmatter.h"

#include <unity.h>

// ── Helpers ───────────────────────────────────────────────────────────────────

void setUp(void)    {}
void tearDown(void) {}

// ── parse() tests ─────────────────────────────────────────────────────────────

// Complete frontmatter with title, date, tags, and a body.
void test_parse_complete_entry(void) {
    const String content =
        "---\n"
        "title: My Journal Entry\n"
        "date: 2026-05-20 10:30:00\n"
        "tags: diary, personal\n"
        "---\n"
        "Body text here.";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("My Journal Entry",    e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-05-20 10:30:00", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("diary, personal",     e.tagsRaw.c_str());
    TEST_ASSERT_EQUAL_STRING("Body text here.",     e.body.c_str());
}

// Frontmatter present but no tags field.
void test_parse_without_tags(void) {
    const String content =
        "---\n"
        "title: No Tags Entry\n"
        "date: 2026-01-01 00:00:00\n"
        "---\n"
        "Body.";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("No Tags Entry",       e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-01-01 00:00:00", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("",                    e.tagsRaw.c_str());
    TEST_ASSERT_EQUAL_STRING("Body.",               e.body.c_str());
}

// File that does NOT start with "---\n" → entire content becomes the body.
void test_parse_no_frontmatter(void) {
    const String content = "Just plain text without any frontmatter.";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("", e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("", e.tagsRaw.c_str());
    TEST_ASSERT_EQUAL_STRING(content.c_str(), e.body.c_str());
}

// Empty string → all fields empty.
void test_parse_empty_string(void) {
    JournalEntry e;
    JournalFrontmatter::parse(String(""), e);

    TEST_ASSERT_EQUAL_STRING("", e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("", e.tagsRaw.c_str());
    TEST_ASSERT_EQUAL_STRING("", e.body.c_str());
}

// Opening "---\n" present but no closing delimiter → treated as raw body.
void test_parse_missing_closing_delimiter(void) {
    const String content =
        "---\n"
        "title: No Closing\n"
        "date: 2026-01-01 00:00:00\n";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    // No closing "---" was found, so the entire content is returned as body
    // and the structured fields stay empty.
    TEST_ASSERT_EQUAL_STRING("",             e.title.c_str());
    TEST_ASSERT_EQUAL_STRING(content.c_str(), e.body.c_str());
}

// Frontmatter with body that starts after the closing "---\n".
void test_parse_multiline_body(void) {
    const String content =
        "---\n"
        "title: Multi\n"
        "date: 2026-06-01 08:00:00\n"
        "---\n"
        "Line one.\n"
        "\n"
        "Line three.";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("Multi",               e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-06-01 08:00:00", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("Line one.\n\nLine three.", e.body.c_str());
}

// Frontmatter only — no body after closing "---\n".
void test_parse_empty_body(void) {
    const String content =
        "---\n"
        "title: Empty Body\n"
        "date: 2026-03-15 12:00:00\n"
        "---\n";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("Empty Body",          e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-03-15 12:00:00", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("",                    e.body.c_str());
}

// File ends with "---" (no trailing newline) — alt-end path in the parser.
void test_parse_closing_at_eof_no_newline(void) {
    // Manually construct string ending with \n--- (no trailing \n)
    const String content = "---\ntitle: EOF\ndate: 2026-01-01 00:00:00\n---";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("EOF",                 e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-01-01 00:00:00", e.date.c_str());
    // Body after \n--- with no trailing content — bodyStart exceeds length
    TEST_ASSERT_EQUAL_STRING("",                    e.body.c_str());
}

// Unknown frontmatter keys are silently ignored.
void test_parse_unknown_keys_ignored(void) {
    const String content =
        "---\n"
        "title: Known\n"
        "unknown_key: should be ignored\n"
        "date: 2026-01-01 00:00:00\n"
        "another_unknown: also ignored\n"
        "---\n"
        "Body.";

    JournalEntry e;
    JournalFrontmatter::parse(content, e);

    TEST_ASSERT_EQUAL_STRING("Known",               e.title.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-01-01 00:00:00", e.date.c_str());
    TEST_ASSERT_EQUAL_STRING("Body.",               e.body.c_str());
}

// ── serialize() tests ─────────────────────────────────────────────────────────

// Serializing a fully-populated entry produces the expected YAML header.
void test_serialize_full_entry(void) {
    JournalEntry e;
    e.title   = "My Entry";
    e.date    = "2026-05-20 10:30:00";
    e.tagsRaw = "diary, personal";
    e.body    = "Body content.";

    const String result = JournalFrontmatter::serialize(e);

    TEST_ASSERT_TRUE(result.startsWith("---\n"));
    TEST_ASSERT(result.indexOf("title: My Entry\n")           >= 0);
    TEST_ASSERT(result.indexOf("date: 2026-05-20 10:30:00\n") >= 0);
    TEST_ASSERT(result.indexOf("tags: diary, personal\n")     >= 0);
    TEST_ASSERT(result.indexOf("---\nBody content.")          >= 0);
}

// When tagsRaw is empty the "tags:" line must be omitted.
void test_serialize_omits_empty_tags(void) {
    JournalEntry e;
    e.title   = "No Tags";
    e.date    = "2026-01-01 00:00:00";
    e.tagsRaw = "";
    e.body    = "Body.";

    const String result = JournalFrontmatter::serialize(e);

    TEST_ASSERT_EQUAL_INT(-1, result.indexOf("tags:"));
    TEST_ASSERT(result.indexOf("title: No Tags\n")            >= 0);
    TEST_ASSERT(result.indexOf("date: 2026-01-01 00:00:00\n") >= 0);
}

// Serialize with an empty body — body section should just be empty.
void test_serialize_empty_body(void) {
    JournalEntry e;
    e.title   = "No Body";
    e.date    = "2026-01-01 00:00:00";
    e.tagsRaw = "";
    e.body    = "";

    const String result = JournalFrontmatter::serialize(e);

    // Should end with "---\n" (the closing fence followed by the empty body)
    TEST_ASSERT_TRUE(result.endsWith("---\n"));
}

// ── Round-trip tests ──────────────────────────────────────────────────────────

// serialize → parse must reproduce all original fields exactly.
void test_roundtrip_all_fields(void) {
    JournalEntry original;
    original.title   = "Round-Trip Test";
    original.date    = "2026-05-20 10:30:00";
    original.tagsRaw = "test, roundtrip";
    original.body    = "This is the body.\n\nSecond paragraph.";

    const String serialized = JournalFrontmatter::serialize(original);

    JournalEntry parsed;
    JournalFrontmatter::parse(serialized, parsed);

    TEST_ASSERT_EQUAL_STRING(original.title.c_str(),   parsed.title.c_str());
    TEST_ASSERT_EQUAL_STRING(original.date.c_str(),    parsed.date.c_str());
    TEST_ASSERT_EQUAL_STRING(original.tagsRaw.c_str(), parsed.tagsRaw.c_str());
    TEST_ASSERT_EQUAL_STRING(original.body.c_str(),    parsed.body.c_str());
}

// Round-trip without tags.
void test_roundtrip_no_tags(void) {
    JournalEntry original;
    original.title   = "No Tags Round-Trip";
    original.date    = "2026-12-31 23:59:59";
    original.tagsRaw = "";
    original.body    = "Just a body.";

    JournalEntry parsed;
    JournalFrontmatter::parse(JournalFrontmatter::serialize(original), parsed);

    TEST_ASSERT_EQUAL_STRING(original.title.c_str(), parsed.title.c_str());
    TEST_ASSERT_EQUAL_STRING(original.date.c_str(),  parsed.date.c_str());
    TEST_ASSERT_EQUAL_STRING("",                     parsed.tagsRaw.c_str());
    TEST_ASSERT_EQUAL_STRING(original.body.c_str(),  parsed.body.c_str());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_complete_entry);
    RUN_TEST(test_parse_without_tags);
    RUN_TEST(test_parse_no_frontmatter);
    RUN_TEST(test_parse_empty_string);
    RUN_TEST(test_parse_missing_closing_delimiter);
    RUN_TEST(test_parse_multiline_body);
    RUN_TEST(test_parse_empty_body);
    RUN_TEST(test_parse_closing_at_eof_no_newline);
    RUN_TEST(test_parse_unknown_keys_ignored);
    RUN_TEST(test_serialize_full_entry);
    RUN_TEST(test_serialize_omits_empty_tags);
    RUN_TEST(test_serialize_empty_body);
    RUN_TEST(test_roundtrip_all_fields);
    RUN_TEST(test_roundtrip_no_tags);

    return UNITY_END();
}
