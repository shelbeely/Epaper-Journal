#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// EntryScreen.h — On-device journal entry viewer
//
// Displays a JournalEntry with title and Markdown-styled body.
// Up/Down pages through long entries; Back returns to the caller.
// Power button triggers the sleep screen.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>
#include "../journal/JournalEntry.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "SleepScreen.h"
#include "MarkdownParser.h"

class EntryScreen {
public:
    EntryScreen(X4Display& display, X4Input& input, SleepScreen& sleepScreen);

    // Display `entry` and block until the user presses Back.
    void show(const JournalEntry& entry);

private:
    X4Display&   _display;
    X4Input&     _input;
    SleepScreen& _sleepScreen;

    static constexpr uint8_t SCALE    = 2;

    uint16_t _titleH;  ///< Title row height = lineHeight(SCALE)*2 (runtime)
    uint16_t _bodyY;   ///< Y where body starts = _titleH + 4 (runtime)
    uint16_t _itemH;   ///< Line height at SCALE (runtime)

    // Render one page of Markdown lines starting at `topLine`.
    void _renderPage(const String& title, const std::vector<MdLine>& lines,
                     int topLine, int totalLines);

    // Draw a single MdLine at pixel row `y`.
    void _renderLine(uint8_t* fb, uint16_t dispW, const MdLine& line, uint16_t y);

    // Wrapper around _display.drawText that applies faux-bold (double-draw at
    // x+1) and inline-code border box based on flags in `line`.
    void _drawLineText(uint8_t* fb, uint16_t x, uint16_t y,
                       const char* str, bool inverted, uint8_t scale,
                       const MdLine& line);
};
