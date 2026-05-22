// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — Xteink X4 E-Paper Journal Firmware
// Phase 1 — Plaintext Diary
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <WiFi.h>
#include <BatteryMonitor.h>
#include <Preferences.h>
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
#include "journal/PromptPack.h"
#include "web/WebApi.h"
#include "wifi/WifiProvisioning.h"
#include "ui/BrowseScreen.h"
#include "ui/EntryScreen.h"
#include "ui/SleepScreen.h"
#include "ui/CalendarScreen.h"
#include "ui/SearchScreen.h"
#include "ui/PinScreen.h"
#include "ui/TitlePromptScreen.h"
#include "vault/VaultManager.h"

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
static VaultManager   gVault;
static JournalManager gJournalMgr(gStorage, gClock, &gVault);
static WebApi         gWebApi(gDiag, gDisplay, gOta, gJournalMgr, gVault);
static SleepScreen    gSleepScreen(gDisplay, gClock);
static BrowseScreen   gBrowse(gJournalMgr, gDisplay, gInput, gClock,
                               gSleepScreen, &gVault);
static EntryScreen    gEntryScreen(gDisplay, gInput, gSleepScreen);
static CalendarScreen gCalendar(gJournalMgr, gDisplay, gInput, gClock);
static SearchScreen   gSearchScreen(gJournalMgr, gDisplay, gInput, gSleepScreen);
static PinScreen      gPinScreen(gDisplay, gInput);

static bool gSafeModeActive = false;
static bool gWifiProvisioningActive = false;
static bool gWifiEnabled = WIFI_AUTO_ON != 0;
static constexpr const char* WIFI_STATE_NAMESPACE = "system";
static constexpr const char* WIFI_STATE_KEY = "wifi_on";

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool connectWifi() {
    // Combo mode: soft-AP always on (reachable at 192.168.4.1), STA attempted
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD);
    X4_LOGF(X4M_WIFI_AP_OK, "ssid=%s ip=192.168.4.1", SOFTAP_SSID);

    WifiCredentials wifiCreds = WifiProvisioning::loadPreferredCredentials();
    WiFi.begin(wifiCreds.ssid.c_str(), wifiCreds.password.c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
        delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
        X4_LOG(X4M_WIFI_OK);
        return false;
    }

    X4_LOG(X4M_WIFI_FAILED);
#if WIFI_PROVISIONING_ENABLED
    return true;
#else
    return false;
#endif
}

static bool loadWifiEnabledState() {
    Preferences prefs;
    if (!prefs.begin(WIFI_STATE_NAMESPACE, true)) {
        return WIFI_AUTO_ON != 0;
    }
    bool enabled = prefs.getBool(WIFI_STATE_KEY, WIFI_AUTO_ON != 0);
    prefs.end();
    return enabled;
}

static void saveWifiEnabledState(bool enabled) {
    Preferences prefs;
    if (!prefs.begin(WIFI_STATE_NAMESPACE, false)) {
        return;
    }
    prefs.putBool(WIFI_STATE_KEY, enabled);
    prefs.end();
}

static void disableWifiRadio() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

static void setWifiEnabled(bool enabled, bool persist = true) {
    if (enabled) {
        connectWifi();
    } else {
        disableWifiRadio();
    }
    gWifiEnabled = enabled;
    if (persist) {
        saveWifiEnabledState(enabled);
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
    // 4. Wi-Fi connected (STA or AP) when Wi-Fi is enabled
    if (gWifiEnabled && WiFi.status() != WL_CONNECTED && !(WiFi.getMode() & WIFI_AP)) {
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
        gJournalMgr.begin();
    }

    // ── Battery ───────────────────────────────────────────────────────────
    if (BATTERY_ADC_PIN != 0) {
        gDiag.batteryPercent = gBattery.readPercentage();
        gDiag.batteryMv      = gBattery.readMillivolts();
    }

    // ── Wi-Fi ─────────────────────────────────────────────────────────────
    gWifiEnabled = loadWifiEnabledState();
    if (gWifiEnabled) {
        gWifiProvisioningActive = connectWifi();
    } else {
        disableWifiRadio();
    }

    // ── Clock sync (best-effort; falls back to NVS) ───────────────────────
    if (!gSafeModeActive) {
        gClock.sync();
    }

    // ── HTTP server ───────────────────────────────────────────────────────
    gWebApi.setWifiProvisioningMode(gWifiProvisioningActive);
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
    // Periodic background maintenance every 30 seconds
    static uint32_t lastPeriodicCheck = 0;
    if (millis() - lastPeriodicCheck > 30000) {
        if (BATTERY_ADC_PIN != 0) {
            gDiag.batteryPercent = gBattery.readPercentage();
            gDiag.batteryMv      = gBattery.readMillivolts();
        }
        gClock.syncIfNeeded();
        lastPeriodicCheck = millis();
    }

    // Run the browse screen (blocks until user selects or presses Back)
    String selectedPath;
    BrowseResult result = gBrowse.run(selectedPath);

    if (result == BrowseResult::NEW_ENTRY) {
        const String dailyPrompt = String(PromptPack::today(gClock.now()));
        const String title = TitlePromptScreen::run(gDisplay, gInput, gClock, dailyPrompt);
        if (!title.isEmpty()) {
            String path = gJournalMgr.createEntry(title);
            if (!path.isEmpty()) {
                JournalEntry e;
                if (gJournalMgr.loadEntry(path, e)) {
                    gEntryScreen.show(e);
                }
            }
        }
    } else if (result == BrowseResult::CALENDAR) {
        gCalendar.run();
    } else if (result == BrowseResult::SEARCH) {
        String path = gSearchScreen.run();
        if (!path.isEmpty()) {
            JournalEntry e;
            if (gJournalMgr.loadEntry(path, e)) {
                gEntryScreen.show(e);
            }
        }
    } else if (result == BrowseResult::VAULT_TOGGLE) {
        if (gVault.isUnlocked()) {
            gVault.lock();
        } else {
            gPinScreen.run(gVault);
        }
    } else if (result == BrowseResult::WIFI_TOGGLE) {
        setWifiEnabled(!gWifiEnabled);
    } else if (result == BrowseResult::OPEN_ENTRY && !selectedPath.isEmpty()) {
        JournalEntry e;
        if (gJournalMgr.loadEntry(selectedPath, e)) {
            gEntryScreen.show(e);
        }
    }
    // BACK: fall through → browse screen runs again on next iteration
}
