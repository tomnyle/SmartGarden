#pragma once

#include <Arduino.h>
#include "relay_manager.h"

// ========================================
// IrrigationManager - controls the irrigation relay with timeout
// ========================================
class IrrigationManager
{
public:
    // relayIndex : which relay in RelayManager drives the pump
    IrrigationManager(RelayManager &relayManager, uint8_t relayIndex = IRRIGATION_RELAY_INDEX);

    void begin();

    // Start irrigation for durationMs milliseconds (0 = indefinite until stopIrrigation)
    void startIrrigation(unsigned long durationMs = 0);

    // Stop irrigation immediately
    void stopIrrigation();

    bool isActive() const { return _active; }

    // Must be called regularly to enforce timeout
    void loop();

private:
    RelayManager &_relayMgr;
    uint8_t       _relayIndex;
    bool          _active;
    unsigned long _startTime;
    unsigned long _durationMs;
};
