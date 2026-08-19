#pragma once

#include <Arduino.h>
#include "pins.h"

class RelayManager
{
private:
    static constexpr int kRelayCount = RELAY_COUNT;
    bool relayState[kRelayCount] = {false};

    struct PendingCommand
    {
        uint8_t relayIndex;
        bool state;
        bool active;
    };

    PendingCommand pendingCommands[kRelayCount] = {};

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
