#include "relay_manager.h"
#include <Arduino.h>

RelayManager::RelayManager()
{
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        relayStates[i] = false;
    }
}

void RelayManager::begin()
{
    Serial.println("[RelayManager] Initializing relays...");
    
    // Initialize all relay pins
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        pinMode(RELAY_PINS[i], OUTPUT);
        // Turn off all relays (HIGH = OFF for active-low relay modules)
        digitalWrite(RELAY_PINS[i], HIGH);
        relayStates[i] = false;
    }
    
    Serial.println("[RelayManager] All relays initialized");
}

bool RelayManager::setRelay(uint8_t index, bool state)
{
    if (index >= NUM_RELAYS)
    {
        Serial.printf("[RelayManager] Invalid relay index: %u\n", index);
        return false;
    }
    
    relayStates[index] = state;
    // Active-low: LOW = ON, HIGH = OFF
    digitalWrite(RELAY_PINS[index], state ? LOW : HIGH);
    
    Serial.printf("[RelayManager] Relay %u -> %s\n", index + 1, state ? "ON" : "OFF");
    return true;
}

bool RelayManager::getRelayState(uint8_t index) const
{
    if (index >= NUM_RELAYS)
    {
        return false;
    }
    return relayStates[index];
}

void RelayManager::setAllRelays(bool state)
{
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        setRelay(i, state);
    }
}

bool RelayManager::toggleRelay(uint8_t index)
{
    if (index >= NUM_RELAYS)
    {
        return false;
    }
    return setRelay(index, !relayStates[index]);
}

uint8_t RelayManager::getRelayPin(uint8_t index) const
{
    if (index >= NUM_RELAYS)
    {
        return 0;
    }
    return RELAY_PINS[index];
}

void RelayManager::printStatus() const
{
    Serial.println("\n========== Relay Status ==========");
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        Serial.printf("Relay %d (Pin %d): %s\n", i + 1, RELAY_PINS[i], 
                     relayStates[i] ? "ON" : "OFF");
    }
    Serial.println("==================================\n");
}
