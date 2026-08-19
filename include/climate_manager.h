#pragma once

#include <Arduino.h>

#include "auto_control.h"
#include "crop_profiles.h"

class RelayManager;

class ClimateManager
{
public:
    void begin(RelayManager &relayManager);
    void setAutoControlEnabled(bool enabled);
    bool isAutoControlEnabled() const;
    void applyProfile(const CropProfile &profile, const SensorSnapshot &snapshot);

private:
    RelayManager *relayManager = nullptr;
    bool autoControlEnabled = true;
};