// ─────────────────────────────────────────────────────────────────────────────
// CalendarScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "CalendarScreen.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

CalendarScreen::CalendarScreen(JournalManager& jm, X4Display& display,
                                X4Input& input, X4Clock& clock)
    : _jm(jm), _display(display), _input(input), _clock(clock)
{}

void CalendarScreen::run() {
    uint16_t year; uint8_t month;
    _clock.currentYearMonth(year, month);

    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        _input.tick();

        if (_input.isPowerButtonPressed()) {
            // Power button: just return to caller; main loop handles sleep
            return;
        }

        bool monthChanged = false;

        if (_input.wasUp() || _input.wasLeft()) {
            // Previous month
            if (month == 1) { month = 12; year--; }
            else            { month--;              }
            monthChanged = true;
        }
        if (_input.wasDown() || _input.wasRight()) {
            // Next month
            if (month == 12) { month = 1; year++; }
            else             { month++;             }
            monthChanged = true;
        }
        if (_input.wasBack() || _input.wasConfirm()) {
            return;
        }

        if (monthChanged) needRedraw = true;

        if (needRedraw) {
            std::vector<String> paths = _jm.listEntries(year, month);
            _render(year, month, paths);
            _display.displayGrayscale();
            fullRefreshPending = false;
            needRedraw = false;
        }

        delay(10);
    }
}

/*static*/
uint8_t CalendarScreen::_daysInMonth(uint16_t year, uint8_t month) {
    static const uint8_t DAYS[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2) {
        // Leap year: divisible by 4, except centuries not divisible by 400
        bool leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        return leap ? 29 : 28;
    }
    return DAYS[month];
}

/*static*/
uint8_t CalendarScreen::_firstWeekday(uint16_t year, uint8_t month) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = 1;
    t.tm_isdst = -1;
    mktime(&t);
    // tm_wday: 0=Sun … 6=Sat → convert to 0=Mon … 6=Sun
    return (uint8_t)((t.tm_wday + 6) % 7);
}

/*static*/
bool CalendarScreen::_hasEntry(const std::vector<String>& paths,
                                uint8_t day, uint16_t year, uint8_t month) {
    char prefix[13];
    snprintf(prefix, sizeof(prefix), "%04u%02u%02u-", year, month, day);
    for (const auto& path : paths) {
        int slash = path.lastIndexOf('/');
        String fn = (slash >= 0) ? path.substring(slash + 1) : path;
        if (fn.startsWith(prefix)) return true;
    }
    return false;
}

void CalendarScreen::_render(uint16_t year, uint8_t month,
                              const std::vector<String>& paths) {
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    // Clear to white (both BW and RED planes)
    _display.clearFrameGrayscale();

    // ── Header band (DARK_GRAY background) ───────────────────────────────────
    {
        char hdr[16];
        snprintf(hdr, sizeof(hdr), "STREAK %04u-%02u", year, month);
        _display.fillRectGray(0, 0, dispW, HDR_H - 1, GrayLevel::DARK_GRAY);
        _display.drawTextGray(4, 4, hdr, GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);

        // Navigation hint (right side)
        const char* hint = "UP/DN:month  BACK:exit";
        uint16_t hintW = (uint16_t)(strlen(hint) * _display.charAdvance(1));
        _display.drawTextGray(dispW - hintW - 4, 8, hint,
                              GrayLevel::WHITE, GrayLevel::DARK_GRAY, 1);
    }

    // ── Day-of-week labels (LIGHT_GRAY background) ────────────────────────────
    {
        static const char* const DOW_LABELS[] =
            {"Mo","Tu","We","Th","Fr","Sa","Su"};
        uint16_t colW = dispW / NCOLS;
        _display.fillRectGray(0, DOW_Y, dispW, DOW_H, GrayLevel::LIGHT_GRAY);
        for (uint8_t col = 0; col < NCOLS; col++) {
            uint16_t labelW = (uint16_t)(2 * _display.charAdvance(SCALE));
            uint16_t x = col * colW + (colW - labelW) / 2;
            _display.drawTextGray(x, DOW_Y + 4, DOW_LABELS[col],
                                  GrayLevel::BLACK, GrayLevel::LIGHT_GRAY, SCALE);
        }
        // DARK_GRAY separator below DOW row
        _display.fillRectGray(0, DOW_Y + DOW_H - 2, dispW, 1, GrayLevel::DARK_GRAY);
    }

    // ── Calendar grid ─────────────────────────────────────────────────────────
    {
        uint16_t colW   = dispW / NCOLS;
        uint16_t gridH  = dispH - GRID_Y;
        uint16_t rowH   = gridH / NROWS;

        uint8_t  firstDow   = _firstWeekday(year, month); // 0=Mon
        uint8_t  totalDays  = _daysInMonth(year, month);

        // Get current day for highlighting today
        struct tm today;
        memset(&today, 0, sizeof(today));
        {
            time_t t = time(nullptr);
            localtime_r(&t, &today);
        }
        uint8_t todayDay = (uint8_t)(today.tm_mday);
        bool isCurrentMonth = ((uint16_t)(today.tm_year + 1900) == year &&
                               (uint8_t)(today.tm_mon + 1) == month);

        uint8_t dayNum = 1;
        for (uint8_t row = 0; row < NROWS && dayNum <= totalDays; row++) {
            for (uint8_t col = 0; col < NCOLS; col++) {
                if (row == 0 && col < firstDow) continue;
                if (dayNum > totalDays) break;

                uint16_t cellX = col * colW;
                uint16_t cellY = GRID_Y + row * rowH;

                bool hasEntry = _hasEntry(paths, dayNum, year, month);
                bool isToday  = isCurrentMonth && (dayNum == todayDay);

                char dayStr[3];
                snprintf(dayStr, sizeof(dayStr), "%u", dayNum);
                uint16_t numW = (uint16_t)(strlen(dayStr) * _display.charAdvance(SCALE));

                if (hasEntry) {
                    // DARK_GRAY filled square
                    uint16_t margin = 4;
                    _display.fillRectGray(
                        cellX + margin, cellY + margin,
                        colW - margin * 2, rowH - margin * 2,
                        GrayLevel::DARK_GRAY);
                    // Day number in WHITE
                    _display.drawTextGray(
                        cellX + (colW - numW) / 2,
                        cellY + (rowH - _display.lineHeight(SCALE)) / 2,
                        dayStr, GrayLevel::WHITE, GrayLevel::DARK_GRAY, SCALE);
                } else if (isToday) {
                    // DARK_GRAY 4-sided border
                    _display.fillRectGray(cellX + 3, cellY + 3,
                                          colW - 6, 1, GrayLevel::DARK_GRAY);
                    _display.fillRectGray(cellX + 3, cellY + rowH - 4,
                                          colW - 6, 1, GrayLevel::DARK_GRAY);
                    _display.fillRectGray(cellX + 3, cellY + 3,
                                          1, rowH - 6, GrayLevel::DARK_GRAY);
                    _display.fillRectGray(cellX + colW - 4, cellY + 3,
                                          1, rowH - 6, GrayLevel::DARK_GRAY);
                    // Day number in BLACK
                    _display.drawTextGray(
                        cellX + (colW - numW) / 2,
                        cellY + (rowH - _display.lineHeight(SCALE)) / 2,
                        dayStr, GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
                } else {
                    // Empty day: just draw the day number
                    _display.drawTextGray(
                        cellX + (colW - numW) / 2,
                        cellY + (rowH - _display.lineHeight(SCALE)) / 2,
                        dayStr, GrayLevel::BLACK, GrayLevel::WHITE, SCALE);
                }

                dayNum++;
            }
        }

        // Entry count in bottom-left
        char cntBuf[24];
        snprintf(cntBuf, sizeof(cntBuf), "%u entr%s this month",
                 (unsigned)paths.size(),
                 paths.size() == 1 ? "y" : "ies");
        _display.drawTextGray(4, dispH - _display.lineHeight(1) - 4, cntBuf,
                              GrayLevel::BLACK, GrayLevel::WHITE, 1);
    }
}
