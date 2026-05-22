#pragma once

enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_CONNECTED = 3,
};

class WiFiClass {
public:
    wl_status_t status() const { return _status; }
    void setStatus(wl_status_t status) { _status = status; }

private:
    static wl_status_t _status;
};

inline wl_status_t WiFiClass::_status = WL_IDLE_STATUS;
inline WiFiClass WiFi;
