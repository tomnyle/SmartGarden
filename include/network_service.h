#pragma once

#include <WiFi.h>

#include "config.h"

class NetworkService
{
public:
    static bool connect(unsigned long timeoutMs = 10000UL)
    {
        WiFi.begin(SMARTGARDEN_WIFI_SSID, SMARTGARDEN_WIFI_PASSWORD);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs)
        {
            delay(250);
        }

        return WiFi.status() == WL_CONNECTED;
    }
};