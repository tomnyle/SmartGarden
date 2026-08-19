#pragma once

#include <Arduino.h>
#include "relay_manager.h"
#include "auto_control.h"
#include "crop_profiles.h"

// ========================================
// ClimateManager - controls fan, irrigation, heating/cooling
// based on sensor readings and the active crop profile
// ========================================
class ClimateManager
{
public:
    explicit ClimateManager(RelayManager &relayManager);

    void begin();

    // Apply relay commands produced by AutoControlSystem
    void applyCommands(const AutoControlSystem::CommandQueue &commands);

    // Convenience wrappers for individual actuators
    void controlFan(bool on);
    void controlIrrigation(bool on);
    void controlHeating(bool on);

    bool isFanOn()         const { return _relayMgr.getRelayState(FAN_RELAY_INDEX); }
    bool isIrrigationOn()  const { return _relayMgr.getRelayState(IRRIGATION_RELAY_INDEX); }
    bool isHeatingOn()     const { return _relayMgr.getRelayState(HEATER_RELAY_INDEX); }

private:
    RelayManager &_relayMgr;
};
