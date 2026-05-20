// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — Xteink X4 E-Paper Journal Firmware
// Phase 1 — Plaintext Diary
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <WiFi.h>
#include <BatteryMonitor.h>
#include <esp_sleep.h>

#include "config.h"
#include "diagnostics/X4Log.h"
#include "diagnostics/X4Diagnostics.h"
#include "display/X4Display.h"
#include "input/X4Input.h"
#include "storage/X4Storage.h"
#include "ota/OtaManager.h"
#include "system/X4Clock.h"
#include "journal/JournalManager.h"
#include "web/WebApi.h"
#include "ui/BrowseScreen.h"
#include "ui/EntryScreen.h"
#include "ui/SleepScreen.h"
#include "ui/CalendarScreen.h"

// ─────────────────────────────────────────────────────────────────────────────
// Global objects (construction order matches dependency order)
// ─────────────────────────────────────────────────────────────────────────────

static X4Display      gDisplay;
static X4Input        gInput;
static X4Storage      gStorage;
static BatteryMonitor gBattery(BATTERY_ADC_PIN, BATTERY_DIVIDER);
static OtaManager     gOta;
static X4Diagnostics  gDiag;
static X4Clock        gClock;
static JournalManager gJournalMgr(gStorage, gClock);
static WebApi         gWebApi(gDiag, gDisplay, gOta, gJournalMgr);
static SleepScreen    gSleepScreen(gDisplay, gClock);
static BrowseScreen   gBrowse(gJournalMgr, gDisplay, gInput, gClock, gSleepScreen);
static EntryScreen    gEntryScreen(gDisplay, gInput, gSleepScreen);
static CalendarScreen gCalendar(gJournalMgr, gDisplay, gInput, gClock);

static bool gSafeModeActive = false;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void connectWifi() {
    // Combo mode: soft-AP always on (reachable at 192.168.4.1), STA attempted
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD);
    X4_LOGF(X4M_WIFI_AP_OK, "ssid=%s ip=192.168.4.1", SOFTAP_SSID);

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

    // 1. Display initialised
    if (!gDiag.display.initialized) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=display");
        ok = false;
    }
    // 2. Storage ready
    if (!gStorage.ready()) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=storage");
        ok = false;
    }
    // 3. Battery sane — only checked when ADC is wired up
    if (BATTERY_ADC_PIN != 0 && gDiag.batteryPercent == 0) {
        X4_LOGF(X4M_HEALTH_FAILED, "check=battery");
        ok = false;
    }
    // 4. Wi-Fi connected (STA or AP)
    if (WiFi.status() != WL_CONNECTED && WiFi.softAPgetStationNum() == 0) {
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
    delay(100);

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
        uint8_t* fb = gDisplay.getFrameBuffer();
        memset(fb, 0xFF, (gDisplay.width() / 8) * gDisplay.height()); // clear white
        if (gSafeModeActive) {
            gDisplay.drawText(fb, 10, 20, "SAFE MODE", false, 2);
        } else {
            gDisplay.drawText(fb, 10, 20, "JOURNAL", false, 2);
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
    }

    // ── Battery ───────────────────────────────────────────────────────────
    if (BATTERY_ADC_PIN != 0) {
        gDiag.batteryPercent = gBattery.readPercentage();
        gDiag.batteryMv      = gBattery.readMillivolts();
    }

    // ── Wi-Fi ─────────────────────────────────────────────────────────────
    connectWifi();

    // ── Clock sync (best-effort; falls back to NVS) ───────────────────────
    if (!gSafeModeActive) {
        gClock.sync();
    }

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
        if (!gSafeModeActive) {
            gOta.markBootHealthy();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    // Battery refresh every 30 seconds
    static uint32_t lastBatRead = 0;
    if (BATTERY_ADC_PIN != 0 && millis() - lastBatRead > 30000) {
        gDiag.batteryPercent = gBattery.readPercentage();
        gDiag.batteryMv      = gBattery.readMillivolts();
        lastBatRead = millis();
    }

    // Run the browse screen (blocks until user selects or presses Back)
    String selectedPath;
    BrowseResult result = gBrowse.run(selectedPath);

    if (result == BrowseResult::NEW_ENTRY) {
        String path = gJournalMgr.createEntry("New Entry");
        if (!path.isEmpty()) {
            JournalEntry e;
            if (gJournalMgr.loadEntry(path, e)) {
                gEntryScreen.show(e);
            }
        }
    } else if (result == BrowseResult::CALENDAR) {
        gCalendar.run();
    } else if (result == BrowseResult::OPEN_ENTRY && !selectedPath.isEmpty()) {
        JournalEntry e;
        if (gJournalMgr.loadEntry(selectedPath, e)) {
            gEntryScreen.show(e);
        }
    }
    // BACK: fall through → browse screen runs again on next iteration
}
