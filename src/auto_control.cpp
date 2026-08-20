#include "auto_control.h"
#include <Arduino.h>

void AutoControlSystem::addCommand(CommandQueue& queue, uint8_t relayIndex, bool state)
{
    if (queue.count < MAX_COMMANDS)
    {
        queue.commands[queue.count].relayIndex = relayIndex;
        queue.commands[queue.count].state = state;
        queue.count++;
    }
}

bool AutoControlSystem::isTempInRange(float temp, const Range& range)
{
    return (temp >= range.min && temp <= range.max);
}

bool AutoControlSystem::isHumidityInRange(float humidity, const Range& range)
{
    return (humidity >= range.min && humidity <= range.max);
}

bool AutoControlSystem::isSoilMoistureInRange(float moisture, const Range& range)
{
    return (moisture >= range.min && moisture <= range.max);
}

RelayCommand AutoControlSystem::getTempControlCommand(float temp, const Range& range)
{
    RelayCommand cmd = {0, false};
    
    if (temp < range.min)
    {
        // Temperature too low - activate heater (relay 1)
        cmd.relayIndex = CLIMATE_HEATER_RELAY_INDEX;
        cmd.state = RELAY_ON;
    }
    else if (temp > range.max)
    {
        // Temperature too high - activate cooler (relay 2)
        cmd.relayIndex = CLIMATE_COOLER_RELAY_INDEX;
        cmd.state = RELAY_ON;
    }
    else
    {
        // Temperature in range - deactivate both
        cmd.relayIndex = CLIMATE_HEATER_RELAY_INDEX;
        cmd.state = RELAY_OFF;
    }
    
    return cmd;
}

RelayCommand AutoControlSystem::getHumidityControlCommand(float humidity, const Range& range)
{
    RelayCommand cmd = {0, false};
    
    if (humidity < range.min)
    {
        // Humidity too low - activate humidifier (relay 3)
        cmd.relayIndex = CLIMATE_HUMIDIFIER_RELAY_INDEX;
        cmd.state = RELAY_ON;
    }
    else if (humidity > range.max)
    {
        // Humidity too high - activate dehumidifier (relay 4)
        cmd.relayIndex = CLIMATE_DEHUMIDIFIER_RELAY_INDEX;
        cmd.state = RELAY_ON;
    }
    else
    {
        // Humidity in range - deactivate both
        cmd.relayIndex = CLIMATE_HUMIDIFIER_RELAY_INDEX;
        cmd.state = RELAY_OFF;
    }
    
    return cmd;
}

RelayCommand AutoControlSystem::getSoilMoistureControlCommand(float moisture, const Range& range)
{
    RelayCommand cmd = {0, false};
    
    if (moisture < range.min)
    {
        // Soil too dry - activate irrigation (relay 5)
        cmd.relayIndex = IRRIGATION_RELAY_INDEX;
        cmd.state = RELAY_ON;
    }
    else if (moisture > range.max)
    {
        // Soil too wet - deactivate irrigation
        cmd.relayIndex = IRRIGATION_RELAY_INDEX;
        cmd.state = RELAY_OFF;
    }
    else
    {
        // Soil moisture in range - maintain current state
        cmd.relayIndex = IRRIGATION_RELAY_INDEX;
        cmd.state = RELAY_OFF;
    }
    
    return cmd;
}

void AutoControlSystem::evaluateAndControl(const CropProfile& profile,
                                          const SensorSnapshot& snapshot,
                                          CommandQueue& outCommands)
{
    // Initialize command queue
    outCommands.count = 0;
    memset(outCommands.commands, 0, sizeof(outCommands.commands));
    
    Serial.println("[AutoControl] Evaluating conditions...");
    
    // Temperature control
    if (!isTempInRange(snapshot.airTemp, profile.temperature))
    {
        RelayCommand cmd = getTempControlCommand(snapshot.airTemp, profile.temperature);
        addCommand(outCommands, cmd.relayIndex, cmd.state);
        Serial.printf("[AutoControl] Temp out of range: %.1f°C (target: %.1f-%.1f°C)\n",
                     snapshot.airTemp, profile.temperature.min, profile.temperature.max);
    }
    
    // Humidity control
    if (!isHumidityInRange(snapshot.airHumidity, profile.airHumidity))
    {
        RelayCommand cmd = getHumidityControlCommand(snapshot.airHumidity, profile.airHumidity);
        addCommand(outCommands, cmd.relayIndex, cmd.state);
        Serial.printf("[AutoControl] Humidity out of range: %.1f%% (target: %.1f-%.1f%%)\n",
                     snapshot.airHumidity, profile.airHumidity.min, profile.airHumidity.max);
    }
    
    // Soil moisture control
    if (!isSoilMoistureInRange(snapshot.soilMoisture, profile.soilHumidity))
    {
        RelayCommand cmd = getSoilMoistureControlCommand(snapshot.soilMoisture, profile.soilHumidity);
        addCommand(outCommands, cmd.relayIndex, cmd.state);
        Serial.printf("[AutoControl] Soil moisture out of range: %.1f%% (target: %.1f-%.1f%%)\n",
                     snapshot.soilMoisture, profile.soilHumidity.min, profile.soilHumidity.max);
    }
    
    if (outCommands.count == 0)
    {
        Serial.println("[AutoControl] All conditions optimal, no commands needed");
    }
    else
    {
        Serial.printf("[AutoControl] Generated %d control commands\n", outCommands.count);
    }
}
