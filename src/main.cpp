// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — Xteink X4 E-Paper Journal Firmware
// Phase 0 — Hardware-Safe Prototype
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <WiFi.h>
#include <BatteryMonitor.h>

#include "config.h"
#include "diagnostics/X4Log.h"
#include "diagnostics/X4Diagnostics.h"
#include "display/X4Display.h"
#include "input/X4Input.h"
#include "storage/X4Storage.h"
#include "ota/OtaManager.h"
#include "web/WebApi.h"

// ─────────────────────────────────────────────────────────────────────────────
// Global objects
// ─────────────────────────────────────────────────────────────────────────────

static X4Display     gDisplay;
static X4Input       gInput;
static X4Storage     gStorage;
static BatteryMonitor gBattery(BATTERY_ADC_PIN, BATTERY_DIVIDER);
static OtaManager    gOta;
static X4Diagnostics gDiag;
static WebApi        gWebApi(gDiag, gDisplay, gOta);

static bool gSafeModeActive = false;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Simple menu-item renderer using raw framebuffer byte writes.
// Writes the string as a row of filled 6×8 "blocks" (placeholder until a
// GFX library is integrated in a later phase).
static void renderMenuLabel(X4Display& disp, const char* label, uint16_t x, uint16_t y) {
    uint8_t* fb     = disp.getFrameBuffer();
    uint16_t wBytes = disp.width() / 8;
    uint16_t h      = disp.height();
    const uint8_t BLOCK_W = 8;
    const uint8_t BLOCK_H = 10;

    for (size_t i = 0; label[i] != '\0'; i++) {
        uint16_t bx = x + i * (BLOCK_W + 2);
        // Draw a filled block for each character
        for (uint8_t row = 0; row < BLOCK_H && (y + row) < h; row++) {
            uint16_t byteIdx = (y + row) * wBytes + bx / 8;
            fb[byteIdx] &= ~(0xFF >> (bx % 8)); // simplistic; fine for Phase 0
        }
    }
}

static void connectWifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
        delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
        X4_LOG(X4M_WIFI_OK);
    } else {
        X4_LOG(X4M_WIFI_FAILED);
    }
}

// Run all health checks required before marking OTA valid.
// Returns true if every check passes.
static bool runHealthChecks() {
    bool ok = true;

    // 1. Display initialised and framebuffer accessible
    if (!gDiag.display.initialized) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=display");
        ok = false;
    }
    // 2. Storage ready
    if (!gStorage.ready()) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=storage");
        ok = false;
    }
    // 3. Battery sane
    if (gDiag.batteryPercent == 0) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=battery");
        ok = false;
    }
    // 4. Wi-Fi connected
    if (WiFi.status() != WL_CONNECTED) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=wifi");
        ok = false;
    }

    if (ok) X4_LOG(X4M_HEALTH_OK);
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(100); // brief settle for Serial

    // ── Safe-mode pre-read (before InputManager::begin()) ──────────────────
    bool powerButtonHeld = X4Input::readPowerButtonGpio();

    // ── OTA boot hook: crash-loop detection ────────────────────────────────
    bool crashLoopDetected = gOta.onBoot();
    gDiag.bootCount = gOta.bootCount();

    if (crashLoopDetected || powerButtonHeld) {
        gSafeModeActive = true;
        gDiag.safeModeActive = true;
        X4_LOG("SAFE_MODE_ACTIVE");
    }

    // ── Display init ───────────────────────────────────────────────────────
    if (gDisplay.init()) {
        // Render boot screen
        gDisplay.getFrameBuffer(); // ensure fb pointer is valid
        gDisplay.clear();
        if (gSafeModeActive) {
            renderMenuLabel(gDisplay, "SAFE MODE", 10, 20);
        } else {
            renderMenuLabel(gDisplay, "JOURNAL", 10, 20);
        }
        gDisplay.fullRefresh();
    }
    gDiag.display = gDisplay.status();

    // ── Input init ────────────────────────────────────────────────────────
    gInput.init();

    // ── Storage init ─────────────────────────────────────────────────────
    if (!gSafeModeActive) {
        gStorage.init();
        gDiag.storageReady = gStorage.ready();

        // Phase 0 self-test: write + read a test file
        if (gStorage.ready()) {
            gStorage.ensureJournalPath(2026, 1);
            gStorage.writeEntry("/journal/2026/01/test.md", "# Phase 0 test\nOK\n");
            String v = gStorage.readEntry("/journal/2026/01/test.md");
            X4_LOGF("STORAGE_SELFTEST", "read=%s", v.isEmpty() ? "EMPTY" : "OK");
        }
    }

    // ── Battery ───────────────────────────────────────────────────────────
    if (BATTERY_ADC_PIN != 0) {
        gDiag.batteryPercent = gBattery.readPercentage();
        gDiag.batteryMv      = gBattery.readMillivolts();
    }

    // ── Wi-Fi ─────────────────────────────────────────────────────────────
    connectWifi();

    // ── HTTP server ───────────────────────────────────────────────────────
    gWebApi.begin();

    // ── Health checks + OTA verification ─────────────────────────────────
    if (gOta.isPendingVerify()) {
        bool healthy = runHealthChecks();
        if (healthy) {
            gOta.markBootHealthy();
        } else {
            X4_LOG(X4M_HEALTH_FAILED);
            gOta.requestRollback(); // does not return
        }
    } else {
        // Normal boot: just reset crash-loop counter on a clean boot
        if (!gSafeModeActive) {
            gOta.markBootHealthy();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────

// Simple Phase 0 menu state: highlight selected item
static const char* MENU_ITEMS[]  = { "JOURNAL", "SETTINGS" };
static const uint8_t MENU_COUNT  = 2;
static uint8_t gMenuIndex        = 0;
static bool    gNeedsRedraw      = true;

static void drawMenu() {
    gDisplay.getFrameBuffer();
    uint8_t* fb     = gDisplay.getFrameBuffer();
    uint16_t wBytes = gDisplay.width() / 8;
    // Clear framebuffer to white
    memset(fb, 0xFF, wBytes * gDisplay.height());

    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        uint16_t y = 20 + i * 30;
        if (i == gMenuIndex) {
            // Invert background for selection (draw black bar)
            for (uint16_t row = y - 2; row < y + 12 && row < gDisplay.height(); row++) {
                memset(fb + row * wBytes, 0x00, wBytes);
            }
        }
        renderMenuLabel(gDisplay, MENU_ITEMS[i], 10, y);
    }
    gDisplay.fastRefresh();
    gNeedsRedraw = false;
}

void loop() {
    gInput.tick();

    if (gInput.wasLeft() || gInput.wasUp()) {
        gMenuIndex = (gMenuIndex == 0) ? MENU_COUNT - 1 : gMenuIndex - 1;
        gNeedsRedraw = true;
    }
    if (gInput.wasRight() || gInput.wasDown()) {
        gMenuIndex = (gMenuIndex + 1) % MENU_COUNT;
        gNeedsRedraw = true;
    }
    if (gInput.wasConfirm()) {
        // Placeholder: log selection and do a full refresh
        X4_LOGF("MENU_SELECT", "item=%s", MENU_ITEMS[gMenuIndex]);
        gDisplay.fullRefresh();
    }

    if (gNeedsRedraw) {
        drawMenu();
    }

    // Power button: long-hold (2 s) triggers deep sleep
    if (gInput.wasConfirmHeld(2000) || gInput.isPowerButtonPressed()) {
        gDisplay.sleep();
        esp_deep_sleep_start();
    }

    // Update battery reading every 30 seconds
    static uint32_t lastBatRead = 0;
    if (BATTERY_ADC_PIN != 0 && millis() - lastBatRead > 30000) {
        gDiag.batteryPercent = gBattery.readPercentage();
        gDiag.batteryMv      = gBattery.readMillivolts();
        lastBatRead = millis();
    }

    delay(10);
}
