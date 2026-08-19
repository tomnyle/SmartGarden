#pragma once

#include "auto_control.h"
#include "relay_manager.h"

class ClimateManager
{
public:
    explicit ClimateManager(RelayManager &relayManager) : _relayManager(relayManager) {}

    void evaluate(const CropProfile &profile,
                  const SensorSnapshot &snapshot,
                  AutoControlSystem::CommandQueue &commands) const
    {
        AutoControlSystem::evaluateAndControl(profile, snapshot, commands);
    }

    void apply(const AutoControlSystem::CommandQueue &commands)
    {
        for (size_t i = 0; i < commands.count; ++i)
        {
            _relayManager.setRelay(commands.commands[i].relayIndex, commands.commands[i].state);
        }
    }

private:
    RelayManager &_relayManager;
};