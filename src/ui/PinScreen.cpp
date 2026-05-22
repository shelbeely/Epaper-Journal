// ─────────────────────────────────────────────────────────────────────────────
// PinScreen.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "PinScreen.h"
#include <string.h>
#include <stdio.h>

PinScreen::PinScreen(X4Display& display, X4Input& input)
    : _display(display), _input(input)
{}

bool PinScreen::run(VaultManager& vault) {
    uint8_t digits[PIN_LEN] = {0, 0, 0, 0};
    uint8_t cursor = 0;

    bool needRedraw = true;
    bool fullRefreshPending = true;

    while (true) {
        _input.tick();

        if (_input.isPowerButtonPressed()) {
            return false;
        }

        bool changed = false;

        if (_input.wasUp()) {
            digits[cursor] = (digits[cursor] + 1) % 10;
            changed = true;
        }
        if (_input.wasDown()) {
            digits[cursor] = (digits[cursor] + 9) % 10;  // wrap 0→9
            changed = true;
        }
        if (_input.wasRight()) {
            if (cursor < PIN_LEN - 1) { cursor++; changed = true; }
        }
        if (_input.wasLeft()) {
            if (cursor > 0) { cursor--; changed = true; }
        }

        if (_input.wasConfirm()) {
            // Build a C-string from the 4 digits
            char pin[PIN_LEN + 1];
            for (uint8_t i = 0; i < PIN_LEN; i++) pin[i] = '0' + digits[i];
            pin[PIN_LEN] = '\0';

            _showFeedback("UNLOCKING...", "", 0);

            bool ok = vault.deriveKeyFromPin(pin);

            if (ok) {
                return true;
            } else {
                char retryText[32] = {0};
                switch (vault.lastUnlockResult()) {
                case VaultManager::UnlockResult::InvalidPin:
                    _showFeedback("INVALID PIN", "TRY AGAIN", 1500);
                    break;
                case VaultManager::UnlockResult::LockedOut:
                    _formatRetryAfter(vault.unlockRetryAfterSeconds(), retryText, sizeof(retryText));
                    _showFeedback("VAULT LOCKED", retryText, 2000);
                    break;
                default:
                    _showFeedback("KEY DERIVATION FAILED", "", 1500);
                    break;
                }
                needRedraw = true;
                fullRefreshPending = true;
            }
        }

        if (_input.wasBack()) {
            return false;
        }

        if (changed) needRedraw = true;

        if (needRedraw) {
            _render(digits, cursor);
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

void PinScreen::_render(const uint8_t digits[PIN_LEN], uint8_t cursor) {
    uint8_t* fb    = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    memset(fb, 0xFF, (size_t)(dispW / 8) * dispH);

    // Title
    _display.drawText(fb, 4, 8, "ENTER PIN", false, 3);
    _display.drawText(fb, 4, 44, "UP/DN:change  LR:move  CONFIRM:submit  BACK:cancel",
                      false, 1);

    // Separator
    _display.fillRect(fb, 0, 58, dispW, 1, true);

    // Digit boxes — centered horizontally
    uint16_t totalW = PIN_LEN * BOX_W + (PIN_LEN - 1) * BOX_GAP;
    uint16_t startX = (dispW - totalW) / 2;
    uint16_t boxY   = (dispH - BOX_H) / 2;

    for (uint8_t i = 0; i < PIN_LEN; i++) {
        uint16_t x = startX + i * (BOX_W + BOX_GAP);
        bool sel = (i == cursor);

        if (sel) {
            _display.fillRect(fb, x, boxY, BOX_W, BOX_H, true);
        } else {
            // Draw border
            _display.fillRect(fb, x, boxY, BOX_W, 2, true);
            _display.fillRect(fb, x, boxY + BOX_H - 2, BOX_W, 2, true);
            _display.fillRect(fb, x, boxY, 2, BOX_H, true);
            _display.fillRect(fb, x + BOX_W - 2, boxY, 2, BOX_H, true);
        }

        // Draw digit
        char d[2] = {'0' + digits[i], '\0'};
        uint16_t charW = _display.charAdvance(SCALE);
        uint16_t charH = _display.lineHeight(SCALE);
        uint16_t charX = x + (BOX_W - charW) / 2;
        uint16_t charY = boxY + (BOX_H - charH) / 2;
        _display.drawText(fb, charX, charY, d, sel, SCALE);
    }

    // "PIN: ••••" indicator at the bottom
    _display.drawText(fb, 4, dispH - 20, "PIN: ****", false, 1);
}

void PinScreen::_showFeedback(const char* line1, const char* line2, uint16_t delayMs) {
    uint8_t* fb    = _display.getFrameBuffer();
    uint16_t dispW = _display.width();
    uint16_t dispH = _display.height();

    memset(fb, 0xFF, (size_t)(dispW / 8) * dispH);
    _display.drawText(fb, 4, (dispH / 2) - 18, line1, false, 2);
    if (line2 && line2[0] != '\0') {
        _display.drawText(fb, 4, (dispH / 2) + 14, line2, false, 2);
    }
    _display.fastRefresh();

    if (delayMs > 0) {
        delay(delayMs);
    }
}

void PinScreen::_formatRetryAfter(uint32_t retryAfterSeconds, char* out, size_t len) const {
    if (!out || len == 0) return;

    if (retryAfterSeconds >= 3600) {
        snprintf(out, len, "TRY AGAIN IN %luh %lum",
                 (unsigned long)(retryAfterSeconds / 3600),
                 (unsigned long)((retryAfterSeconds % 3600) / 60));
    } else if (retryAfterSeconds >= 60) {
        snprintf(out, len, "TRY AGAIN IN %lum %lus",
                 (unsigned long)(retryAfterSeconds / 60),
                 (unsigned long)(retryAfterSeconds % 60));
    } else {
        snprintf(out, len, "TRY AGAIN IN %lus", (unsigned long)retryAfterSeconds);
    }
}
