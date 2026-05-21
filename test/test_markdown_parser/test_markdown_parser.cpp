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

// ── XJL tasks: open ──────────────────────────────────────────────────────────

void test_parse_task_open(void) {
    auto lines = MarkdownParser::parse("- [ ] buy milk", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_TASK_OPEN, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("buy milk", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
    TEST_ASSERT_FALSE(lines[0].strikethrough);
}

void test_parse_task_open_empty_text(void) {
    auto lines = MarkdownParser::parse("- [ ] ", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_TASK_OPEN, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("", lines[0].text.c_str());
}

// ── XJL tasks: done ──────────────────────────────────────────────────────────

void test_parse_task_done_lowercase(void) {
    auto lines = MarkdownParser::parse("- [x] done task", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_TASK_DONE, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("done task", lines[0].text.c_str());
    TEST_ASSERT_TRUE(lines[0].strikethrough);
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_task_done_uppercase(void) {
    auto lines = MarkdownParser::parse("- [X] Done uppercase", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_TASK_DONE, lines[0].type);
    TEST_ASSERT_TRUE(lines[0].strikethrough);
}

void test_parse_task_done_wraps_all_strikethrough(void) {
    // Short maxChars forces wrap; all wrapped lines should have strikethrough
    auto lines = MarkdownParser::parse("- [x] word one two", 10);
    TEST_ASSERT_GREATER_THAN(1, (int)lines.size());
    for (auto& l : lines) {
        TEST_ASSERT_EQUAL(MD_TASK_DONE, l.type);
        TEST_ASSERT_TRUE(l.strikethrough);
    }
}

void test_parse_task_done_inline_stripped(void) {
    auto lines = MarkdownParser::parse("- [x] **done** task", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("done task", lines[0].text.c_str());
}

// ── XJL tasks: migrated / scheduled ─────────────────────────────────────────

void test_parse_task_migrated(void) {
    auto lines = MarkdownParser::parse("- [>] moved forward", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_TASK_MIGRATED, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("moved forward", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].strikethrough);
}

void test_parse_task_scheduled(void) {
    auto lines = MarkdownParser::parse("- [<] pushed back", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_TASK_SCHEDULED, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("pushed back", lines[0].text.c_str());
}

void test_parse_task_unknown_mark_falls_to_bullet(void) {
    // - [?] is not a known task mark → treated as plain bullet "[ ?] ..."
    // but actually: raw[2]='[', raw[4]=']', raw[5]=' ' so it matches the
    // task check; mark='?' is unknown → falls through to goto → generic bullet.
    // The text after generic bullet start "- " is "[?] some text".
    auto lines = MarkdownParser::parse("- [z] unknown", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("[z] unknown", lines[0].text.c_str());
}

void test_parse_task_not_matched_without_closing_bracket_space(void) {
    // "- [x]text" (no space after ]) → not a task, falls to bullet
    auto lines = MarkdownParser::parse("- [x]text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET, lines[0].type);
}

// ── XJL signifiers ────────────────────────────────────────────────────────────

void test_parse_signifier_priority(void) {
    auto lines = MarkdownParser::parse("! important thing", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_PRIORITY, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("important thing", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].strikethrough);
}

void test_parse_signifier_event(void) {
    auto lines = MarkdownParser::parse("@ dentist 10am", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_EVENT, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("dentist 10am", lines[0].text.c_str());
}

void test_parse_signifier_question(void) {
    auto lines = MarkdownParser::parse("? why did I do that", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_QUESTION, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("why did I do that", lines[0].text.c_str());
}

void test_parse_signifier_inline_stripped(void) {
    auto lines = MarkdownParser::parse("! **urgent** deadline", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_PRIORITY, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("urgent deadline", lines[0].text.c_str());
}

void test_parse_signifier_no_space_is_normal(void) {
    // "!text" (no space after !) → normal paragraph (not a signifier)
    auto lines = MarkdownParser::parse("!text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_NORMAL, lines[0].type);
}

void test_parse_signifier_question_wraps(void) {
    // Ensure continuation flag works for wrapped signifier lines
    auto lines = MarkdownParser::parse("? one two three four", 8);
    TEST_ASSERT_GREATER_THAN(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_QUESTION, lines[0].type);
    TEST_ASSERT_FALSE(lines[0].continuation);
    for (int i = 1; i < (int)lines.size(); i++) {
        TEST_ASSERT_EQUAL(MD_QUESTION, lines[i].type);
        TEST_ASSERT_TRUE(lines[i].continuation);
    }
}

// ── XJL: strikethrough field is false for non-done types ─────────────────────

void test_strikethrough_false_for_open_task(void) {
    auto lines = MarkdownParser::parse("- [ ] not done", 40);
    TEST_ASSERT_FALSE(lines[0].strikethrough);
}

void test_strikethrough_false_for_normal(void) {
    auto lines = MarkdownParser::parse("normal line", 40);
    TEST_ASSERT_FALSE(lines[0].strikethrough);
}

// ── Extended inline: ~~strikethrough~~ ───────────────────────────────────────

void test_strip_strikethrough(void) {
    String result = MarkdownParser::stripInline("~~struck~~");
    TEST_ASSERT_EQUAL_STRING("struck", result.c_str());
}

void test_strip_strikethrough_unclosed(void) {
    // No closing ~~ → emit tildes literally
    String result = MarkdownParser::stripInline("~~unclosed");
    TEST_ASSERT_EQUAL_STRING("~~unclosed", result.c_str());
}

void test_strip_strikethrough_mixed(void) {
    String result = MarkdownParser::stripInline("keep ~~gone~~ keep");
    TEST_ASSERT_EQUAL_STRING("keep gone keep", result.c_str());
}

// ── Extended inline: ==highlight== ───────────────────────────────────────────

void test_strip_highlight(void) {
    String result = MarkdownParser::stripInline("==marked==");
    TEST_ASSERT_EQUAL_STRING("marked", result.c_str());
}

void test_strip_highlight_unclosed(void) {
    // No closing == → emit equals literally
    String result = MarkdownParser::stripInline("==unclosed");
    TEST_ASSERT_EQUAL_STRING("==unclosed", result.c_str());
}

void test_strip_highlight_mixed(void) {
    String result = MarkdownParser::stripInline("plain ==hi== plain");
    TEST_ASSERT_EQUAL_STRING("plain hi plain", result.c_str());
}

// ── Fenced code blocks ────────────────────────────────────────────────────────

void test_parse_code_block_basic(void) {
    auto lines = MarkdownParser::parse("```\ncode line\n```", 40);
    // Fence markers are consumed; one code line remains
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("code line", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_code_block_with_language_tag(void) {
    // Opening ``` can have a language identifier — still toggled and skipped
    auto lines = MarkdownParser::parse("```cpp\nint x = 0;\n```", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("int x = 0;", lines[0].text.c_str());
}

void test_parse_code_block_preserves_inline_markers(void) {
    // Inline markers inside code blocks must NOT be stripped
    auto lines = MarkdownParser::parse("```\n**bold**\n```", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("**bold**", lines[0].text.c_str());
}

void test_parse_code_block_blank_line_inside(void) {
    // Blank line inside a code fence → MD_CODE_BLOCK with empty text
    auto lines = MarkdownParser::parse("```\nline1\n\nline2\n```", 40);
    TEST_ASSERT_EQUAL(3, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("line1", lines[0].text.c_str());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[1].type);
    TEST_ASSERT_EQUAL_STRING("", lines[1].text.c_str());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[2].type);
    TEST_ASSERT_EQUAL_STRING("line2", lines[2].text.c_str());
}

void test_parse_code_block_tilde_fence(void) {
    // ~~~ is an alternative fence marker
    auto lines = MarkdownParser::parse("~~~\nhello\n~~~", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_CODE_BLOCK, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("hello", lines[0].text.c_str());
}

void test_parse_code_block_multiple_lines(void) {
    auto lines = MarkdownParser::parse("```\nalpha\nbeta\ngamma\n```", 40);
    TEST_ASSERT_EQUAL(3, (int)lines.size());
    for (auto& l : lines) {
        TEST_ASSERT_EQUAL(MD_CODE_BLOCK, l.type);
    }
    TEST_ASSERT_EQUAL_STRING("alpha", lines[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("beta",  lines[1].text.c_str());
    TEST_ASSERT_EQUAL_STRING("gamma", lines[2].text.c_str());
}

// ── Nested / indented bullets ─────────────────────────────────────────────────

void test_parse_bullet_nested_two_spaces(void) {
    auto lines = MarkdownParser::parse("  - nested item", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET_NESTED, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("nested item", lines[0].text.c_str());
    TEST_ASSERT_FALSE(lines[0].continuation);
}

void test_parse_bullet_nested_tab(void) {
    auto lines = MarkdownParser::parse("\t- tab indented", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET_NESTED, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("tab indented", lines[0].text.c_str());
}

void test_parse_bullet_nested_star(void) {
    auto lines = MarkdownParser::parse("  * star nested", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET_NESTED, lines[0].type);
}

void test_parse_bullet_nested_inline_stripped(void) {
    auto lines = MarkdownParser::parse("  - **bold** text", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET_NESTED, lines[0].type);
    TEST_ASSERT_EQUAL_STRING("bold text", lines[0].text.c_str());
}

void test_parse_bullet_nested_wraps_with_continuation(void) {
    // maxCharsNormal=12, nested budget=8; long text wraps
    auto lines = MarkdownParser::parse("  - one two three four", 12);
    TEST_ASSERT_GREATER_THAN(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_BULLET_NESTED, lines[0].type);
    TEST_ASSERT_FALSE(lines[0].continuation);
    TEST_ASSERT_EQUAL(MD_BULLET_NESTED, lines[1].type);
    TEST_ASSERT_TRUE(lines[1].continuation);
}

void test_parse_bullet_nested_single_space_is_normal(void) {
    // Only one leading space → not a nested bullet; falls to normal text
    auto lines = MarkdownParser::parse(" - one space", 40);
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL(MD_NORMAL, lines[0].type);
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

    // ── XJL tasks ──
    RUN_TEST(test_parse_task_open);
    RUN_TEST(test_parse_task_open_empty_text);
    RUN_TEST(test_parse_task_done_lowercase);
    RUN_TEST(test_parse_task_done_uppercase);
    RUN_TEST(test_parse_task_done_wraps_all_strikethrough);
    RUN_TEST(test_parse_task_done_inline_stripped);
    RUN_TEST(test_parse_task_migrated);
    RUN_TEST(test_parse_task_scheduled);
    RUN_TEST(test_parse_task_unknown_mark_falls_to_bullet);
    RUN_TEST(test_parse_task_not_matched_without_closing_bracket_space);

    // ── XJL signifiers ──
    RUN_TEST(test_parse_signifier_priority);
    RUN_TEST(test_parse_signifier_event);
    RUN_TEST(test_parse_signifier_question);
    RUN_TEST(test_parse_signifier_inline_stripped);
    RUN_TEST(test_parse_signifier_no_space_is_normal);
    RUN_TEST(test_parse_signifier_question_wraps);

    RUN_TEST(test_strikethrough_false_for_open_task);
    RUN_TEST(test_strikethrough_false_for_normal);

    // ── Extended inline: ~~strikethrough~~ ──
    RUN_TEST(test_strip_strikethrough);
    RUN_TEST(test_strip_strikethrough_unclosed);
    RUN_TEST(test_strip_strikethrough_mixed);

    // ── Extended inline: ==highlight== ──
    RUN_TEST(test_strip_highlight);
    RUN_TEST(test_strip_highlight_unclosed);
    RUN_TEST(test_strip_highlight_mixed);

    // ── Fenced code blocks ──
    RUN_TEST(test_parse_code_block_basic);
    RUN_TEST(test_parse_code_block_with_language_tag);
    RUN_TEST(test_parse_code_block_preserves_inline_markers);
    RUN_TEST(test_parse_code_block_blank_line_inside);
    RUN_TEST(test_parse_code_block_tilde_fence);
    RUN_TEST(test_parse_code_block_multiple_lines);

    // ── Nested bullets ──
    RUN_TEST(test_parse_bullet_nested_two_spaces);
    RUN_TEST(test_parse_bullet_nested_tab);
    RUN_TEST(test_parse_bullet_nested_star);
    RUN_TEST(test_parse_bullet_nested_inline_stripped);
    RUN_TEST(test_parse_bullet_nested_wraps_with_continuation);
    RUN_TEST(test_parse_bullet_nested_single_space_is_normal);

    return UNITY_END();
}
