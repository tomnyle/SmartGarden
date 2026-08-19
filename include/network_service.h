#pragma once

#include <Arduino.h>
#include <WiFi.h>

// ========================================
// NetworkService - manages WiFi connection and auto-reconnect
// ========================================
class NetworkService
{
public:
    NetworkService(const char *ssid, const char *password);

    // Attempt initial WiFi connection (blocking up to timeoutMs)
    bool connect(unsigned long timeoutMs = 10000);

    bool isConnected() const;

    // RSSI in dBm, or 0 if not connected
    int getSignalStrength() const;

    // Call in main loop to handle auto-reconnect
    void loop();

private:
    char _ssid[64];
    char _password[64];

    unsigned long _lastReconnectAttempt;
    static const unsigned long RECONNECT_INTERVAL_MS = 5000;
};
