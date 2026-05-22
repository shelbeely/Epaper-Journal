// ─────────────────────────────────────────────────────────────────────────────
// WifiProvisioning.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "WifiProvisioning.h"

#include <Preferences.h>

#include "../config.h"

namespace {
constexpr const char* WIFI_CREDS_NAMESPACE = "wifi_creds";
constexpr const char* WIFI_SSID_KEY = "ssid";
constexpr const char* WIFI_PASSWORD_KEY = "password";
} // namespace

WifiCredentials WifiProvisioning::loadPreferredCredentials() {
    WifiCredentials creds;
    if (_loadStoredCredentials(creds)) {
        creds.fromNvs = true;
        return creds;
    }

    creds.ssid = WIFI_SSID;
    creds.password = WIFI_PASSWORD;
    return creds;
}

bool WifiProvisioning::saveCredentials(const String& ssid, const String& password) {
    if (ssid.isEmpty()) return false;

    Preferences prefs;
    if (!prefs.begin(WIFI_CREDS_NAMESPACE, false)) return false;

    size_t savedSsid = prefs.putString(WIFI_SSID_KEY, ssid);
    size_t savedPassword = prefs.putString(WIFI_PASSWORD_KEY, password);
    prefs.end();

    return savedSsid == ssid.length() && savedPassword == password.length();
}

bool WifiProvisioning::_loadStoredCredentials(WifiCredentials& creds) {
    Preferences prefs;
    if (!prefs.begin(WIFI_CREDS_NAMESPACE, true)) return false;

    String ssid = prefs.getString(WIFI_SSID_KEY, "");
    if (ssid.isEmpty()) {
        prefs.end();
        return false;
    }

    creds.ssid = ssid;
    creds.password = prefs.getString(WIFI_PASSWORD_KEY, "");
    prefs.end();
    return true;
}
