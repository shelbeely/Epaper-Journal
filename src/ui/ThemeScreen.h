#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ThemeScreen.h — Theme System dashboard (portrait 480×800)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <vector>
#include "../journal/JournalManager.h"
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "../system/X4Clock.h"

class ThemeScreen {
public:
    ThemeScreen(JournalManager& jm, X4Display& display, X4Input& input, X4Clock& clock);

    // Show Theme dashboard until user presses Back/Confirm.
    void run();

private:
    struct RatingSample {
        uint32_t dateKey = 0;   // YYYYMMDD
        uint8_t  rating  = 0;   // 1..5
    };

    struct ThemeNote {
        uint32_t dateKey = 0;   // YYYYMMDD
        String   text;
    };

    struct ThemeScreenData {
        String theme;
        String season;
        std::vector<RatingSample> ratings; // oldest -> newest
        std::vector<ThemeNote> notes;      // newest -> oldest
    };

    JournalManager& _jm;
    X4Display&      _display;
    X4Input&        _input;
    X4Clock&        _clock;

    static constexpr uint8_t TITLE_SCALE = 2;
    static constexpr uint8_t BODY_SCALE  = 1;
    static constexpr uint16_t Z1_Y0 = 0,   Z1_Y1 = 60;
    static constexpr uint16_t Z2_Y0 = 61,  Z2_Y1 = 110;
    static constexpr uint16_t Z3_Y0 = 111, Z3_Y1 = 210;
    static constexpr uint16_t Z4_Y0 = 211, Z4_Y1 = 620;
    static constexpr uint16_t Z5_Y0 = 621;
    static constexpr uint8_t  MAX_RATINGS = 90;
    static constexpr uint8_t  MAX_NOTES   = 50;
    static constexpr uint8_t  RATING_WINDOW_DAYS = 7;

    ThemeScreenData _loadData();
    void _render(const ThemeScreenData& data, int noteTop, int ratingWeekOffset);

    static String _seasonName(uint8_t month);
    static uint32_t _dateKey(uint16_t y, uint8_t m, uint8_t d);
    static bool _parseDateFromPath(const String& path, uint16_t& y, uint8_t& m, uint8_t& d);
    static String _formatDateTag(uint32_t dateKey);   // MM-DD
    static String _formatDayLabel(uint32_t dateKey);  // M/T/W/T/F/S/S
    static void _seasonDateRange(uint16_t curYear, uint8_t curMonth, uint32_t& startKey, uint32_t& endKey);
    static bool _startsWith(const String& s, const char* prefix);
    static String _trimmedAfterPrefix(const String& s, size_t prefixLen);
    static int _parseRating(const String& s);
    static std::vector<String> _wrapWords(const String& text, uint16_t maxChars, uint8_t maxLines);
};
