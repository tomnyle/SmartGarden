#include "relay_manager.h"

#include "pins.h"

namespace
{
bool relayWriteLevel(bool enabled)
{
    return RELAY_ACTIVE_LOW ? !enabled : enabled;
}
}

void RelayManager::begin()
{
    for (int i = 0; i < RELAY_COUNT; ++i)
    {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], relayWriteLevel(false));
        relayState[i] = false;
        pendingCommands[i] = {static_cast<uint8_t>(i), false, false};
    }

    Serial.println("Relay manager initialized");
}

void RelayManager::setRelay(int index, bool state)
{
    if (index < 0 || index >= RELAY_COUNT)
    {
        return;
    }

    relayState[index] = state;
    digitalWrite(RELAY_PINS[index], relayWriteLevel(state));

    Serial.print("Relay ");
    Serial.print(index + 1);
    Serial.print(" -> ");
    Serial.println(state ? "ON" : "OFF");
}

bool RelayManager::getRelayState(int index) const
{
    if (index < 0 || index >= RELAY_COUNT)
    {
        return false;
    }

    return relayState[index];
}

void RelayManager::queueCommand(uint8_t index, bool state)
{
    if (index >= RELAY_COUNT)
    {
        return;
    }

    pendingCommands[index].relayIndex = index;
    pendingCommands[index].state = state;
    pendingCommands[index].active = true;
}

void RelayManager::applyQueuedCommands()
{
    for (int i = 0; i < RELAY_COUNT; ++i)
    {
        if (!pendingCommands[i].active)
        {
            continue;
        }

        setRelay(pendingCommands[i].relayIndex, pendingCommands[i].state);
        pendingCommands[i].active = false;
    }
}

void RelayManager::allOff()
{
    for (int i = 0; i < RELAY_COUNT; ++i)
    {
        setRelay(i, false);
        pendingCommands[i].active = false;
    }
}

void RelayManager::printRelayStatus() const
{
    Serial.println("\n===== RELAY STATUS =====");
    for (int i = 0; i < RELAY_COUNT; ++i)
    {
        Serial.printf("Relay %d: %s\n", i + 1, relayState[i] ? "ON" : "OFF");
    }
    Serial.println("========================");
}

int RelayManager::getRelayCount() const
{
    return RELAY_COUNT;
}
