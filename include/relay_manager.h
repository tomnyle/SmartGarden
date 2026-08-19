#pragma once

#include <Arduino.h>

class RelayManager
{
private:
    static const int RELAY_COUNT = 8;
    int relayPins[RELAY_COUNT] = {5, 18, 19, 27, 32, 33, 25, 26};
    bool relayState[RELAY_COUNT] = {false};

public:
    RelayManager() {}

    void begin()
    {
        for (int i = 0; i < RELAY_COUNT; i++)
        {
            pinMode(relayPins[i], OUTPUT);
            digitalWrite(relayPins[i], HIGH); // Relay OFF (active low)
            relayState[i] = false;
        }
        Serial.println("Relay Manager initialized");
    }

    void setRelay(int index, bool state)
    {
        if (index < 0 || index >= RELAY_COUNT)
            return;

        relayState[index] = state;
        digitalWrite(relayPins[index], state ? LOW : HIGH);

        Serial.print("Relay ");
        Serial.print(index + 1);
        Serial.print(" -> ");
        Serial.println(state ? "ON" : "OFF");
    }

    bool getRelayState(int index)
    {
        if (index < 0 || index >= RELAY_COUNT)
            return false;
        return relayState[index];
    }

    void allOff()
    {
        for (int i = 0; i < RELAY_COUNT; i++)
        {
            setRelay(i, false);
        }
    }

    void printRelayStatus()
    {
        Serial.println("\n===== RELAY STATUS =====");
        for (int i = 0; i < RELAY_COUNT; i++)
        {
            Serial.printf("Relay %d: %s\n", i + 1, relayState[i] ? "ON" : "OFF");
        }
        Serial.println("========================");
    }
};
