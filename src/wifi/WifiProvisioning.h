#pragma once

#include <Arduino.h>

struct WifiCredentials {
    String ssid;
    String password;
    bool   fromNvs = false;
};

class WifiProvisioning {
public:
    static WifiCredentials loadPreferredCredentials();
    static bool saveCredentials(const String& ssid, const String& password);

private:
    static bool _loadStoredCredentials(WifiCredentials& creds);
};
