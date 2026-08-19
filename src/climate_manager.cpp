#include "climate_manager.h"

ClimateManager::ClimateManager(RelayManager &relayManager)
    : _relayMgr(relayManager)
{
}

void ClimateManager::begin()
{
    _relayMgr.begin();
    Serial.println("[ClimateManager] Initialised");
}

void ClimateManager::applyCommands(const AutoControlSystem::CommandQueue &commands)
{
    for (size_t i = 0; i < commands.count; ++i)
    {
        const RelayCommand &cmd = commands.commands[i];
        _relayMgr.setRelay(cmd.relayIndex, cmd.state);
    }
}

void ClimateManager::controlFan(bool on)
{
    Serial.printf("[ClimateManager] Fan -> %s\n", on ? "ON" : "OFF");
    _relayMgr.setRelay(FAN_RELAY_INDEX, on);
}

void ClimateManager::controlIrrigation(bool on)
{
    Serial.printf("[ClimateManager] Irrigation -> %s\n", on ? "ON" : "OFF");
    _relayMgr.setRelay(IRRIGATION_RELAY_INDEX, on);
}

void ClimateManager::controlHeating(bool on)
{
    Serial.printf("[ClimateManager] Heating -> %s\n", on ? "ON" : "OFF");
    _relayMgr.setRelay(HEATER_RELAY_INDEX, on);
}
