// ─────────────────────────────────────────────────────────────────────────────
// ThemeScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "ThemeScreen.h"
#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

ThemeScreen::ThemeScreen(JournalManager& jm, X4Display& display, X4Input& input, X4Clock& clock)
    : _jm(jm), _display(display), _input(input), _clock(clock)
{}

void ThemeScreen::run() {
    DisplayOrientation oldOrientation = _display.getOrientation();
    _display.setOrientation(DisplayOrientation::PORTRAIT_CW);

    ThemeScreenData data = _loadData();
    int noteTop = 0;
    int ratingWeekOffset = 0; // weeks back from newest window
    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        _input.tick();

        if (_input.isPowerButtonPressed() || _input.wasBack()) {
            _display.setOrientation(oldOrientation);
            return;
        }
        if (_input.wasUp()) {
            if (noteTop > 0) {
                noteTop--;
                needRedraw = true;
            }
        }
        if (_input.wasDown()) {
            int visibleRows = (int)((Z4_Y1 - (228 + 3)) / 38);
            int maxTop = (int)data.notes.size() - visibleRows;
            if (maxTop < 0) maxTop = 0;
            if (noteTop < maxTop) {
                noteTop++;
                needRedraw = true;
            }
        }
        if (_input.wasLeft()) {
            int maxWeeksBack = (int)data.ratings.size() / RATING_WINDOW_DAYS;
            if (ratingWeekOffset < maxWeeksBack) {
                ratingWeekOffset++;
                needRedraw = true;
            }
        }
        if (_input.wasRight()) {
            if (ratingWeekOffset > 0) {
                ratingWeekOffset--;
                needRedraw = true;
            }
        }
        if (_input.wasConfirm()) {
            data = _loadData();
            noteTop = 0;
            ratingWeekOffset = 0;
            needRedraw = true;
            fullRefreshPending = true;
        }

        if (needRedraw) {
            _render(data, noteTop, ratingWeekOffset);
            if (fullRefreshPending) {
                _display.fullRefresh();
                fullRefreshPending = false;
            } else {
                _display.fastRefresh();
            }
            needRedraw = false;
        }

        delay(10);
    }
}

ThemeScreen::ThemeScreenData ThemeScreen::_loadData() {
    ThemeScreenData data;
    data.theme = "No theme set";

    uint16_t curYear;
    uint8_t  curMonth;
    _clock.currentYearMonth(curYear, curMonth);

    uint32_t startKey, endKey;
    _seasonDateRange(curYear, curMonth, startKey, endKey);
    data.season = _seasonName(curMonth) + " " + String(curYear);

    auto paths = _jm.listAllPaths(); // oldest -> newest
    for (const auto& path : paths) {
        uint16_t y;
        uint8_t m, d;
        if (!_parseDateFromPath(path, y, m, d)) continue;
        uint32_t dk = _dateKey(y, m, d);
        if (dk < startKey || dk > endKey) continue;

        JournalEntry e;
        if (!_jm.loadEntry(path, e) || e.locked) continue;

        int lineStart = 0;
        while (lineStart <= (int)e.body.length()) {
            int lineEnd = e.body.indexOf('\n', lineStart);
            if (lineEnd < 0) lineEnd = (int)e.body.length();
            String line = e.body.substring(lineStart, lineEnd);
            line.trim();

            if (_startsWith(line, "::theme ")) {
                String v = _trimmedAfterPrefix(line, 8);
                if (!v.isEmpty()) data.theme = v;
            } else if (_startsWith(line, "::season ")) {
                String v = _trimmedAfterPrefix(line, 9);
                if (!v.isEmpty()) data.season = v;
            } else if (_startsWith(line, "::rating ")) {
                int n = _parseRating(_trimmedAfterPrefix(line, 9));
                if (n > 0) {
                    bool replaced = false;
                    for (auto& r : data.ratings) {
                        if (r.dateKey == dk) {
                            r.rating = (uint8_t)n;
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) {
                        RatingSample rs;
                        rs.dateKey = dk;
                        rs.rating = (uint8_t)n;
                        data.ratings.push_back(rs);
                    }
                }
            } else if (_startsWith(line, "~ ")) {
                String v = _trimmedAfterPrefix(line, 2);
                if (!v.isEmpty()) {
                    ThemeNote tn;
                    tn.dateKey = dk;
                    tn.text = v;
                    data.notes.push_back(tn);
                }
            }

            if (lineEnd >= (int)e.body.length()) break;
            lineStart = lineEnd + 1;
        }
    }

    std::sort(data.ratings.begin(), data.ratings.end(),
              [](const RatingSample& a, const RatingSample& b) { return a.dateKey < b.dateKey; });
    if (data.ratings.size() > MAX_RATINGS) {
        data.ratings.erase(data.ratings.begin(), data.ratings.end() - MAX_RATINGS);
    }

    std::reverse(data.notes.begin(), data.notes.end()); // newest first
    if (data.notes.size() > MAX_NOTES) {
        data.notes.resize(MAX_NOTES);
    }

    return data;
}

void ThemeScreen::_render(const ThemeScreenData& data, int noteTop, int ratingWeekOffset) {
    uint8_t* fb    = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();
    (void)dispH;

    memset(fb, 0xFF, (size_t)(dispW / 8) * dispH);

    // Zone 1 — Theme banner
    _display.fillRect(fb, 0, Z1_Y0, dispW, Z1_Y1 - Z1_Y0, true);
    _display.drawText(fb, 8, 8, "THEME", true, TITLE_SCALE);
    _display.drawTextWrapped(fb, 92, 10, dispW - 98, data.theme.c_str(), true, TITLE_SCALE);
    _display.fillRect(fb, 0, Z1_Y1, dispW, 1, true);

    // Zone 2 — Season header
    _display.drawText(fb, 8, 76, "SEASON", false, TITLE_SCALE);
    _display.drawTextWrapped(fb, 116, 78, dispW - 122, data.season.c_str(), false, TITLE_SCALE);
    _display.fillRect(fb, 0, Z2_Y1, dispW, 1, true);

    // Zone 3 — Rating history strip
    _display.drawText(fb, 4, 115, "DAILY RATINGS", false, BODY_SCALE);
    _display.fillRect(fb, 0, 128, dispW, 1, true);

    int totalRatings = (int)data.ratings.size();
    int end = totalRatings - (ratingWeekOffset * RATING_WINDOW_DAYS);
    if (end < 0) end = 0;
    int start = end - RATING_WINDOW_DAYS;
    if (start < 0) start = 0;
    if (end < start) end = start;
    int visible = end - start;
    uint16_t colW = (dispW - 8) / RATING_WINDOW_DAYS;

    struct tm now = _clock.now();
    uint32_t todayKey = _dateKey((uint16_t)(now.tm_year + 1900), (uint8_t)(now.tm_mon + 1), (uint8_t)now.tm_mday);

    for (int i = 0; i < visible; i++) {
        const RatingSample& rs = data.ratings[start + i];
        uint16_t x = 4 + (uint16_t)(i * colW);
        bool isToday = (rs.dateKey == todayKey);
        if (isToday) {
            _display.fillRect(fb, x, 132, colW - 1, 74, true);
        }

        String dow = _formatDayLabel(rs.dateKey);
        _display.drawText(fb, x + (colW / 2) - 3, 136, dow.c_str(), isToday, BODY_SCALE);

        const uint8_t box = 8;
        const uint8_t gap = 2;
        uint16_t stripW = (uint16_t)(5 * box + 4 * gap);
        uint16_t bx = x + (colW - stripW) / 2;
        for (uint8_t k = 1; k <= 5; k++) {
            uint16_t by = 154;
            _display.fillRect(fb, bx, by, box, 1, !isToday);
            _display.fillRect(fb, bx, by + box - 1, box, 1, !isToday);
            _display.fillRect(fb, bx, by, 1, box, !isToday);
            _display.fillRect(fb, bx + box - 1, by, 1, box, !isToday);
            if (k <= rs.rating) {
                _display.fillRect(fb, bx + 2, by + 2, box - 4, box - 4, !isToday);
            }
            bx += (uint16_t)(box + gap);
        }

        char dayBuf[3];
        uint8_t d = (uint8_t)(rs.dateKey % 100);
        snprintf(dayBuf, sizeof(dayBuf), "%u", d);
        uint16_t dayW = (uint16_t)(strlen(dayBuf) * _display.charAdvance(BODY_SCALE));
        _display.drawText(fb, x + (colW - dayW) / 2, 196, dayBuf, isToday, BODY_SCALE);
    }

    // Zone 4 — Theme notes feed
    _display.drawText(fb, 4, 215, "THEME NOTES", false, BODY_SCALE);
    _display.fillRect(fb, 0, 228, dispW, 1, true);

    const uint16_t feedY = 231;
    const uint16_t feedH = Z4_Y1 - feedY;
    const uint16_t rowH  = 38;
    int rows = (int)(feedH / rowH);
    int maxTop = (int)data.notes.size() - rows;
    if (maxTop < 0) maxTop = 0;
    if (noteTop > maxTop) noteTop = maxTop;

    for (int r = 0; r < rows; r++) {
        int idx = noteTop + r;
        if (idx >= (int)data.notes.size()) break;
        const ThemeNote& n = data.notes[idx];
        uint16_t y = feedY + (uint16_t)(r * rowH);

        _display.drawChar(fb, 4, y + 2, '~', false, TITLE_SCALE);

        String tag = _formatDateTag(n.dateKey);
        uint16_t tagW = (uint16_t)(tag.length() * _display.charAdvance(BODY_SCALE) + 6);
        uint16_t tagX = dispW - tagW - 4;
        _display.fillRect(fb, tagX, y + 2, tagW, _display.lineHeight(BODY_SCALE), true);
        _display.drawText(fb, tagX + 3, y + 2, tag.c_str(), true, BODY_SCALE);

        uint16_t textX = 22;
        uint16_t maxW = (tagX > textX + 6) ? (uint16_t)(tagX - textX - 6) : 40;
        uint16_t maxChars = (uint16_t)(maxW / _display.charAdvance(BODY_SCALE));
        auto lines = _wrapWords(n.text, maxChars, 2);
        for (uint8_t li = 0; li < lines.size(); li++) {
            _display.drawText(fb, textX, y + 2 + li * _display.lineHeight(BODY_SCALE),
                              lines[li].c_str(), false, BODY_SCALE);
        }

        _display.fillRect(fb, 0, y + rowH - 1, dispW, 1, true);
    }

    // Zone 5 — Footer/nav
    _display.fillRect(fb, 0, Z5_Y0, dispW, 1, true);
    uint16_t colW3 = dispW / 3;
    const char* left = "[ WEEK <- ]";
    const char* mid  = "[ REFRESH ]";
    const char* right= "[ -> WEEK ]";
    uint16_t leftW = (uint16_t)(strlen(left) * _display.charAdvance(BODY_SCALE));
    uint16_t midW  = (uint16_t)(strlen(mid)  * _display.charAdvance(BODY_SCALE));
    uint16_t rightW= (uint16_t)(strlen(right)* _display.charAdvance(BODY_SCALE));
    _display.drawText(fb, (colW3 - leftW) / 2, Z5_Y0 + 8, left, false, BODY_SCALE);
    _display.drawText(fb, colW3 + (colW3 - midW) / 2, Z5_Y0 + 8, mid, false, BODY_SCALE);
    _display.drawText(fb, colW3 * 2 + (colW3 - rightW) / 2, Z5_Y0 + 8, right, false, BODY_SCALE);
    _display.drawText(fb, 4, Z5_Y0 + 24, "UP/DN: notes   BACK: exit", false, BODY_SCALE);
}

String ThemeScreen::_seasonName(uint8_t month) {
    if (month >= 3 && month <= 5) return "Spring";
    if (month >= 6 && month <= 8) return "Summer";
    if (month >= 9 && month <= 11) return "Fall";
    return "Winter";
}

uint32_t ThemeScreen::_dateKey(uint16_t y, uint8_t m, uint8_t d) {
    return (uint32_t)y * 10000UL + (uint32_t)m * 100UL + (uint32_t)d;
}

bool ThemeScreen::_parseDateFromPath(const String& path, uint16_t& y, uint8_t& m, uint8_t& d) {
    int slash = path.lastIndexOf('/');
    String fn = (slash >= 0) ? path.substring(slash + 1) : path;
    if (fn.length() < 8) return false;
    for (uint8_t i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)fn[i])) return false;
    }
    y = (uint16_t)atoi(fn.substring(0, 4).c_str());
    m = (uint8_t)atoi(fn.substring(4, 6).c_str());
    d = (uint8_t)atoi(fn.substring(6, 8).c_str());
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    return true;
}

String ThemeScreen::_formatDateTag(uint32_t dateKey) {
    uint8_t m = (uint8_t)((dateKey / 100) % 100);
    uint8_t d = (uint8_t)(dateKey % 100);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02u-%02u", m, d);
    return String(buf);
}

String ThemeScreen::_formatDayLabel(uint32_t dateKey) {
    uint16_t y = (uint16_t)(dateKey / 10000);
    uint8_t m = (uint8_t)((dateKey / 100) % 100);
    uint8_t d = (uint8_t)(dateKey % 100);
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_isdst = -1;
    mktime(&t);
    static const char DOW[] = {'S','M','T','W','T','F','S'}; // tm_wday 0=Sun
    char c[2] = {DOW[(t.tm_wday >= 0 && t.tm_wday <= 6) ? t.tm_wday : 0], '\0'};
    return String(c);
}

void ThemeScreen::_seasonDateRange(uint16_t curYear, uint8_t curMonth, uint32_t& startKey, uint32_t& endKey) {
    if (curMonth >= 3 && curMonth <= 5) {
        startKey = _dateKey(curYear, 3, 1);
        endKey   = _dateKey(curYear, 5, 31);
        return;
    }
    if (curMonth >= 6 && curMonth <= 8) {
        startKey = _dateKey(curYear, 6, 1);
        endKey   = _dateKey(curYear, 8, 31);
        return;
    }
    if (curMonth >= 9 && curMonth <= 11) {
        startKey = _dateKey(curYear, 9, 1);
        endKey   = _dateKey(curYear, 11, 30);
        return;
    }
    // Winter (Dec-Feb)
    if (curMonth == 12) {
        startKey = _dateKey(curYear, 12, 1);
        endKey   = _dateKey(curYear + 1, 2, 29);
    } else {
        startKey = _dateKey(curYear - 1, 12, 1);
        endKey   = _dateKey(curYear, 2, 29);
    }
}

bool ThemeScreen::_startsWith(const String& s, const char* prefix) {
    return s.startsWith(prefix);
}

String ThemeScreen::_trimmedAfterPrefix(const String& s, size_t prefixLen) {
    String out = s.substring((unsigned int)prefixLen);
    out.trim();
    return out;
}

int ThemeScreen::_parseRating(const String& s) {
    int n = atoi(s.c_str());
    if (n < 1) n = 1;
    if (n > 5) n = 5;
    return n;
}

std::vector<String> ThemeScreen::_wrapWords(const String& text, uint16_t maxChars, uint8_t maxLines) {
    std::vector<String> out;
    if (maxChars == 0 || maxLines == 0) return out;
    String remaining = text;
    remaining.trim();
    while (!remaining.isEmpty() && out.size() < maxLines) {
        if (remaining.length() <= maxChars) {
            out.push_back(remaining);
            break;
        }
        int cut = (int)maxChars;
        while (cut > 0 && remaining[cut] != ' ') cut--;
        if (cut <= 0) cut = (int)maxChars;
        String line = remaining.substring(0, cut);
        line.trim();
        out.push_back(line);
        remaining = remaining.substring(cut);
        remaining.trim();
    }
    if (!remaining.isEmpty() && !out.empty()) {
        String& last = out.back();
        if (last.length() >= 3) {
            last = last.substring(0, last.length() - 3) + "...";
        }
    }
    return out;
}
