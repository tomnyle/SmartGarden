#include "climate_manager.h"

#include "relay_manager.h"

void ClimateManager::begin(RelayManager &relayManagerRef)
{
    relayManager = &relayManagerRef;
    autoControlEnabled = true;
}

void ClimateManager::setAutoControlEnabled(bool enabled)
{
    autoControlEnabled = enabled;
}

bool ClimateManager::isAutoControlEnabled() const
{
    return autoControlEnabled;
}

void ClimateManager::applyProfile(const CropProfile &profile, const SensorSnapshot &snapshot)
{
    if (!autoControlEnabled || relayManager == nullptr)
    {
        return;
    }

    AutoControlSystem::CommandQueue commands = {};
    AutoControlSystem::evaluateAndControl(profile, snapshot, commands);

    for (size_t i = 0; i < commands.count; ++i)
    {
        relayManager->queueCommand(commands.commands[i].relayIndex, commands.commands[i].state);
    }

    const bool tooHot = snapshot.airTemp > profile.temperature.max;
    const bool tooCold = snapshot.airTemp < profile.temperature.min;
    const bool humidityHigh = snapshot.airHumidity > profile.airHumidity.max;

    relayManager->queueCommand(FAN_RELAY_INDEX, tooHot || humidityHigh);
    relayManager->queueCommand(HEATER_RELAY_INDEX, tooCold);
    relayManager->queueCommand(COOLER_RELAY_INDEX, tooHot);
    relayManager->applyQueuedCommands();
}