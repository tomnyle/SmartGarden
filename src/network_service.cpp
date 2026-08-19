#include "network_service.h"
#include <WiFi.h>

NetworkService::NetworkService(const char* ssid, const char* password)
    : connected(false), lastCheckTime(0), checkInterval(10000)
{
    strncpy(this->ssid, ssid, sizeof(this->ssid) - 1);
    strncpy(this->password, password, sizeof(this->password) - 1);
}

void NetworkService::begin()
{
    Serial.println("[NetworkService] Starting WiFi connection...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED)
    {
        connected = true;
        Serial.println();
        Serial.println("[NetworkService] WiFi connected!");
        Serial.printf("[NetworkService] IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[NetworkService] Signal Strength: %d dBm\n", WiFi.RSSI());
    }
    else
    {
        connected = false;
        Serial.println();
        Serial.println("[NetworkService] WiFi connection failed!");
    }
}

bool NetworkService::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

int NetworkService::getSignalStrength() const
{
    return WiFi.RSSI();
}

String NetworkService::getIP() const
{
    return WiFi.localIP().toString();
}

String NetworkService::getMacAddress() const
{
    return WiFi.macAddress();
}

void NetworkService::loop()
{
    unsigned long now = millis();
    if (now - lastCheckTime < checkInterval)
    {
        return; // Not time to check yet
    }
    lastCheckTime = now;
    
    bool currentStatus = isConnected();
    
    if (currentStatus != connected)
    {
        connected = currentStatus;
        if (connected)
        {
            Serial.println("[NetworkService] WiFi reconnected!");
            Serial.printf("[NetworkService] IP Address: %s\n", WiFi.localIP().toString().c_str());
        }
        else
        {
            Serial.println("[NetworkService] WiFi disconnected!");
            Serial.println("[NetworkService] Attempting to reconnect...");
            WiFi.reconnect();
        }
    }
}

void NetworkService::printStatus() const
{
    Serial.println("=== Network Status ===");
    Serial.printf("WiFi Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    if (isConnected())
    {
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    }
    Serial.println("=======================");
}
