#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// EntryScreen.h — On-device journal entry viewer
//
// Displays a JournalEntry with title and word-wrapped body.
// Up/Down pages through long entries; Back returns to the caller.
// Power button triggers the sleep screen.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>
#include "../journal/JournalEntry.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "SleepScreen.h"

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
    static constexpr uint8_t TITLE_H  = FONT5X7_LINE_H * SCALE * 2; // title row height
    static constexpr uint8_t BODY_Y   = TITLE_H + 4;                 // y where body starts
    static constexpr uint8_t ITEM_H   = FONT5X7_LINE_H * SCALE;      // line height

    // Word-wrap `text` to at most `maxCharsPerLine` characters per line.
    static std::vector<String> wrapText(const String& text, uint16_t maxCharsPerLine);

    // Render one page of lines starting at `topLine`.
    void _renderPage(const String& title, const std::vector<String>& lines,
                     int topLine, int totalLines);
};
