// ─────────────────────────────────────────────────────────────────────────────
// test/test_markdown_parser/test_markdown_parser.cpp
//
// Unit tests for MarkdownParser::parse() and MarkdownParser::stripInline().
// No hardware required; no display mocks needed.
// ─────────────────────────────────────────────────────────────────────────────

// Arduino stub must come first.
#include "Arduino.h"

// Compile the source under test directly.
#include "ui/MarkdownParser.cpp"  // NOLINT(bugprone-suspicious-include)

#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// ── stripInline ───────────────────────────────────────────────────────────────

void test_strip_plain_text_unchanged(void) {
    String result = MarkdownParser::stripInline("hello world");
    TEST_ASSERT_EQUAL_STRING("hello world", result.c_str());
}

void test_strip_bold(void) {
    String result = MarkdownParser::stripInline("**bold**");
    TEST_ASSERT_EQUAL_STRING("bold", result.c_str());
}

void test_strip_italic_star(void) {
    String result = MarkdownParser::stripInline("*italic*");
    TEST_ASSERT_EQUAL_STRING("italic", result.c_str());
}

void test_strip_italic_underscore(void) {
    String result = MarkdownParser::stripInline("_italic_");
    TEST_ASSERT_EQUAL_STRING("italic", result.c_str());
}

void test_strip_code(void) {
    String result = MarkdownParser::stripInline("`code`");
    TEST_ASSERT_EQUAL_STRING("code", result.c_str());
}

void test_strip_link(void) {
    String result = MarkdownParser::stripInline("[link text](https://example.com)");
    TEST_ASSERT_EQUAL_STRING("link text", result.c_str());
}

void test_strip_image(void) {
    String result = MarkdownParser::stripInline("![alt text](image.png)");
    TEST_ASSERT_EQUAL_STRING("alt text", result.c_str());
}

void test_strip_mixed_inline(void) {
    String result = MarkdownParser::stripInline("**bold** and *italic*");
    TEST_ASSERT_EQUAL_STRING("bold and italic", result.c_str());
}

void test_strip_unclosed_marker_passthrough(void) {
    // Unclosed ** — no match found, returned as-is
    String result = MarkdownParser::stripInline("**unclosed");
    TEST_ASSERT_EQUAL_STRING("**unclosed", result.c_str());
}

void test_strip_empty_string(void) {
    String result = MarkdownParser::stripInline("");
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ── _isHRule (tested via parse) ───────────────────────────────────────────────

void test_hline_dashes(void) {
    auto lines = MarkdownParser::parse("---", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_HLINE, lines[0].type);
}

void test_hline_asterisks(void) {
    auto lines = MarkdownParser::parse("***", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_HLINE, lines[0].type);
}

void test_hline_underscores(void) {
    auto lines = MarkdownParser::parse("___", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_HLINE, lines[0].type);
}

void test_hline_spaced_dashes(void) {
    auto lines = MarkdownParser::parse("- - -", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_HLINE, lines[0].type);
}

void test_hline_too_short_is_normal(void) {
    // "- item" has dash followed by space + text → bullet, not HLINE
    auto lines = MarkdownParser::parse("- item", 40);
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
}

// ── parse: headings ───────────────────────────────────────────────────────────

void test_parse_h1(void) {
    auto lines = MarkdownParser::parse("# Heading One", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_H1, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("Heading One", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_h2(void) {
    auto lines = MarkdownParser::parse("## Heading Two", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_H2, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("Heading Two", lines[0].text.c_str());
}

void test_parse_h3(void) {
    auto lines = MarkdownParser::parse("### Heading Three", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_H3, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("Heading Three", lines[0].text.c_str());
}

void test_parse_hash_no_space_is_normal(void) {
    // "#nospace" — no space after hash → treated as normal text
    auto lines = MarkdownParser::parse("#nospace", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_NORMAL, lines[0].type);
}

// ── parse: bullet list ────────────────────────────────────────────────────────

void test_parse_bullet_dash(void) {
    auto lines = MarkdownParser::parse("- item text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("item text", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_bullet_star(void) {
    auto lines = MarkdownParser::parse("* item text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
}

void test_parse_bullet_plus(void) {
    auto lines = MarkdownParser::parse("+ item text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
}

void test_parse_bullet_wraps_with_continuation(void) {
    // maxCharsNormal=10, bullet budget=8; "a b c d e f" > 8 chars → wraps
    auto lines = MarkdownParser::parse("- a b c d e f", 10);
    TEST_ASSERT_GREATER_THAN(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
    TEST_ASSERT_FALSE(lines[0].continuation);
    TEST_ASSERT_EQUAL(MD_BULLET, lines[1].type);
    TEST_ASSERT_TRUE(lines[1].continuation);
}

// ── parse: ordered list ───────────────────────────────────────────────────────

void test_parse_ordered(void) {
    auto lines = MarkdownParser::parse("1. First item", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_ORDERED, lines[0].type);
    // First line includes the "1. " prefix
    TEST_ASSERT_EQUAL_STRING("1. First item", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_ordered_multi_digit(void) {
    auto lines = MarkdownParser::parse("10. Tenth item", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_ORDERED, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("10. Tenth item", lines[0].text.c_str());
}

// ── parse: blockquote ─────────────────────────────────────────────────────────

void test_parse_blockquote(void) {
    auto lines = MarkdownParser::parse("> quoted text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BLOCKQUOTE, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("quoted text", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_blockquote_no_space(void) {
    auto lines = MarkdownParser::parse(">no space", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BLOCKQUOTE, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("no space", lines[0].text.c_str());
}

// ── parse: blank lines ────────────────────────────────────────────────────────

void test_parse_blank_line(void) {
    auto lines = MarkdownParser::parse("\n", 40);
    // One blank before the newline, one trailing (from the loop reaching len)
    bool foundBlank = false;
    for (auto& l : lines) {
        if (l.type == MD_BLANK) { foundBlank = true; break; }
    }
    TEST_ASSERT_TRUE(foundBlank);
}

// ── parse: normal text ────────────────────────────────────────────────────────

void test_parse_normal_short_line(void) {
    auto lines = MarkdownParser::parse("Hello world", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_NORMAL, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("Hello world", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_normal_wraps(void) {
    // "one two three" = 13 chars; maxChars=5 → must wrap
    auto lines = MarkdownParser::parse("one two three", 5);
    TEST_ASSERT_GREATER_THAN(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_NORMAL, lines[0].type);
    TEST_ASSERT_FALSE(lines[0].continuation);
    for (int i = 1; i < (int)lines.size(); i++) {
        TEST_ASSERT_EQUAL(MD_NORMAL, lines[i].type);
        TEST_ASSERT_TRUE(lines[i].continuation);
    }
}

void test_parse_inline_stripped_in_normal(void) {
    auto lines = MarkdownParser::parse("Hello **world**", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("Hello world", lines[0].text.c_str());
}

void test_parse_inline_stripped_in_heading(void) {
    auto lines = MarkdownParser::parse("# **Bold** title", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_H1, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("Bold title", lines[0].text.c_str());
}

// ── parse: multi-line body ────────────────────────────────────────────────────

void test_parse_multiline(void) {
    auto lines = MarkdownParser::parse("# Title\n\nSome body text.", 40);
    // Expected: H1, BLANK, NORMAL
    TEST_ASSERT_GREATER_OR_EQUAL(3, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_H1,     lines[0].type);
    TEST_ASSERT_EQUAL(MD_BLANK,  lines[1].type);
    TEST_ASSERT_EQUAL(MD_NORMAL, lines[2].type);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_strip_plain_text_unchanged);
    RUN_TEST(test_strip_bold);
    RUN_TEST(test_strip_italic_star);
    RUN_TEST(test_strip_italic_underscore);
    RUN_TEST(test_strip_code);
    RUN_TEST(test_strip_link);
    RUN_TEST(test_strip_image);
    RUN_TEST(test_strip_mixed_inline);
    RUN_TEST(test_strip_unclosed_marker_passthrough);
    RUN_TEST(test_strip_empty_string);

    RUN_TEST(test_hline_dashes);
    RUN_TEST(test_hline_asterisks);
    RUN_TEST(test_hline_underscores);
    RUN_TEST(test_hline_spaced_dashes);
    RUN_TEST(test_hline_too_short_is_normal);

    RUN_TEST(test_parse_h1);
    RUN_TEST(test_parse_h2);
    RUN_TEST(test_parse_h3);
    RUN_TEST(test_parse_hash_no_space_is_normal);

    RUN_TEST(test_parse_bullet_dash);
    RUN_TEST(test_parse_bullet_star);
    RUN_TEST(test_parse_bullet_plus);
    RUN_TEST(test_parse_bullet_wraps_with_continuation);

    RUN_TEST(test_parse_ordered);
    RUN_TEST(test_parse_ordered_multi_digit);

    RUN_TEST(test_parse_blockquote);
    RUN_TEST(test_parse_blockquote_no_space);

    RUN_TEST(test_parse_blank_line);

    RUN_TEST(test_parse_normal_short_line);
    RUN_TEST(test_parse_normal_wraps);
    RUN_TEST(test_parse_inline_stripped_in_normal);
    RUN_TEST(test_parse_inline_stripped_in_heading);

    RUN_TEST(test_parse_multiline);

    return UNITY_END();
}
