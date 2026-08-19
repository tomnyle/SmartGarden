#include "network_service.h"

NetworkService::NetworkService(const char *ssid, const char *password)
    : _lastReconnectAttempt(0)
{
    strncpy(_ssid,     ssid,     sizeof(_ssid) - 1);
    strncpy(_password, password, sizeof(_password) - 1);
    _ssid[sizeof(_ssid) - 1]         = '\0';
    _password[sizeof(_password) - 1] = '\0';
}

bool NetworkService::connect(unsigned long timeoutMs)
{
    Serial.printf("[NetworkService] Connecting to SSID: %s\n", _ssid);
    WiFi.begin(_ssid, _password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start >= timeoutMs)
        {
            Serial.println("[NetworkService] Connection timed out");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("[NetworkService] Connected! IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}

bool NetworkService::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

int NetworkService::getSignalStrength() const
{
    if (!isConnected())
        return 0;
    return WiFi.RSSI();
}

void NetworkService::loop()
{
    if (isConnected())
        return;

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < RECONNECT_INTERVAL_MS)
        return;

    _lastReconnectAttempt = now;
    Serial.println("[NetworkService] WiFi disconnected, attempting reconnect...");
    WiFi.reconnect();
}
