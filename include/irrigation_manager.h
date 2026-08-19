#pragma once

#include "crop_profiles.h"
#include "relay_manager.h"

class IrrigationManager
{
public:
    explicit IrrigationManager(RelayManager &relayManager) : _relayManager(relayManager) {}

    void setEnabled(bool enabled)
    {
        _relayManager.setRelay(IRRIGATION_RELAY_INDEX, enabled);
    }

private:
    RelayManager &_relayManager;
};