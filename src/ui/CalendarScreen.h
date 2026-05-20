#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// CalendarScreen.h — Monthly "streak" calendar for the e-paper display
//
// Renders a 7-column month grid where each day that has a journal entry is
// marked with a filled black square. The user can navigate between months
// with Up (prev) / Down (next) and exit with Back.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>
#include "../journal/JournalManager.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "../system/X4Clock.h"

class CalendarScreen {
public:
    CalendarScreen(JournalManager& jm, X4Display& display,
                   X4Input& input, X4Clock& clock);

    // Show the streak calendar (blocking). Returns when user presses Back.
    void run();

private:
    JournalManager& _jm;
    X4Display&      _display;
    X4Input&        _input;
    X4Clock&        _clock;

    static constexpr uint8_t SCALE   = 2;
    static constexpr uint8_t HDR_H   = 40;   // pixels for "STREAK YYYY-MM" header
    static constexpr uint8_t DOW_H   = 30;   // day-of-week label row height
    static constexpr uint8_t DOW_Y   = HDR_H;
    static constexpr uint8_t GRID_Y  = HDR_H + DOW_H;
    static constexpr uint8_t NCOLS   = 7;
    static constexpr uint8_t NROWS   = 6;    // max weeks in a month

    // Return the number of days in month (1-12) for the given year.
    static uint8_t _daysInMonth(uint16_t year, uint8_t month);

    // Return the weekday (0=Mon … 6=Sun) for the 1st of the given month/year.
    static uint8_t _firstWeekday(uint16_t year, uint8_t month);

    // True if any path in `paths` corresponds to the given day.
    static bool _hasEntry(const std::vector<String>& paths,
                          uint8_t day, uint16_t year, uint8_t month);

    // Render the full calendar view.
    void _render(uint16_t year, uint8_t month,
                 const std::vector<String>& paths);
};
