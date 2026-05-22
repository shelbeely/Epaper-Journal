// ─────────────────────────────────────────────────────────────────────────────
// TitlePromptScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "TitlePromptScreen.h"
#include <string.h>
#include "../display/X4Display.h"
#include "../input/X4Input.h"
#include "../system/X4Clock.h"

namespace {
constexpr uint8_t OPTION_COUNT = 3;
constexpr uint8_t TITLE_SCALE  = 2;
constexpr uint8_t ITEM_SCALE   = 2;
constexpr uint16_t HEADER_H    = 56;
constexpr uint16_t ITEM_H      = 36;
}

String TitlePromptScreen::run(X4Display& display, X4Input& input, X4Clock& clock,
                              const String& dailyPrompt) {
    const String options[OPTION_COUNT] = {
        formatDateOption(clock.now()),
        formatPromptOption(dailyPrompt),
        "Untitled"
    };

    uint8_t selected = 0;
    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        input.tick();

        if (input.isPowerButtonPressed() || input.wasBack()) {
            return "";
        }

        if (input.wasUp()) {
            selected = (uint8_t)((selected + OPTION_COUNT - 1) % OPTION_COUNT);
            needRedraw = true;
        }
        if (input.wasDown()) {
            selected = (uint8_t)((selected + 1) % OPTION_COUNT);
            needRedraw = true;
        }
        if (input.wasConfirm()) {
            return options[selected];
        }

        if (needRedraw) {
            _render(display, options, selected);
            if (fullRefreshPending) {
                display.fullRefresh();
                fullRefreshPending = false;
            } else {
                display.fastRefresh();
            }
            needRedraw = false;
        }

        delay(10);
    }
}

void TitlePromptScreen::_render(X4Display& display, const String options[3], uint8_t selected) {
    uint8_t* fb    = display.getFrameBuffer();
    uint16_t dispW = display.width();
    uint16_t dispH = display.height();

    memset(fb, 0xFF, (size_t)(dispW / 8) * dispH);

    display.drawText(fb, 4, 4, "CHOOSE TITLE", false, TITLE_SCALE);
    display.drawText(fb, 4, 28, "UP/DN:choose  CONFIRM:use  BACK:cancel", false, 1);
    display.fillRect(fb, 0, HEADER_H - 2, dispW, 1, true);

    for (uint8_t i = 0; i < OPTION_COUNT; i++) {
        uint16_t itemY = HEADER_H + i * ITEM_H;
        bool sel = (i == selected);
        if (sel) {
            display.fillRect(fb, 0, itemY, dispW, ITEM_H, true);
        }

        uint16_t textW = (uint16_t)(options[i].length() * FONT5X7_ADVANCE * ITEM_SCALE);
        uint16_t textX = textW + 8 < dispW ? (dispW - textW) / 2 : 4;
        uint16_t textY = itemY + (ITEM_H - FONT5X7_LINE_H * ITEM_SCALE) / 2;
        display.drawText(fb, textX, textY, options[i].c_str(), sel, ITEM_SCALE);
    }
}
