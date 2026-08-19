#pragma once

#include <Arduino.h>

class RelayManager
{
private:
    static const int RELAY_COUNT = 8;
    bool relayState[RELAY_COUNT] = {false};

    struct PendingCommand
    {
        uint8_t relayIndex;
        bool state;
        bool active;
    };

    PendingCommand pendingCommands[RELAY_COUNT] = {};

public:
    RelayManager() = default;

    void begin();
    void setRelay(int index, bool state);
    bool getRelayState(int index) const;
    void queueCommand(uint8_t index, bool state);
    void applyQueuedCommands();
    void allOff();
    void printRelayStatus() const;
    int getRelayCount() const;
};
