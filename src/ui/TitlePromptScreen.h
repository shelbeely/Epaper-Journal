#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// TitlePromptScreen.h — New-entry title chooser for the e-paper display
//
// Shows three title options for a new journal entry:
//   1. Today's date (YYYY-MM-DD)
//   2. First 28 chars of today's writing prompt
//   3. "Untitled"
// Up/Down changes the selection, Confirm accepts it, Back cancels.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <stdio.h>
#include <time.h>

class X4Display;
class X4Input;
class X4Clock;

class TitlePromptScreen {
public:
    static String run(X4Display& display, X4Input& input, X4Clock& clock,
                      const String& dailyPrompt);

    static String formatDateOption(const struct tm& now) {
        char buf[11];
        const unsigned year  = (unsigned)(now.tm_year + 1900);
        const unsigned month = (unsigned)(now.tm_mon + 1);
        const unsigned day   = (unsigned)now.tm_mday;
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u",
                 year, month, day);
        return String(buf);
    }

    static String formatPromptOption(const String& dailyPrompt) {
        return dailyPrompt.length() <= 28 ? dailyPrompt : dailyPrompt.substring(0, 28);
    }

private:
    static void _render(X4Display& display, const String options[3], uint8_t selected);
};
