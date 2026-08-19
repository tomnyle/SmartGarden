#include "climate_manager.h"
#include "auto_control.h"

ClimateManager::ClimateManager()
    : currentProfile(nullptr), lastControlTime(0), controlInterval(10000)
{
}

void ClimateManager::begin(RelayManager* relayMgr)
{
    relayManager = relayMgr;
    Serial.println("[ClimateManager] Initialized");
}

bool ClimateManager::setCurrentProfile(const CropProfile* profile)
{
    if (profile == nullptr)
    {
        Serial.println("[ClimateManager] ERROR: Profile is null");
        return false;
    }
    currentProfile = profile;
    Serial.printf("[ClimateManager] Profile set to: %s\n", profile->name);
    return true;
}

void ClimateManager::control(const SensorSnapshot& snapshot)
{
    if (currentProfile == nullptr)
    {
        Serial.println("[ClimateManager] WARNING: No profile set");
        return;
    }
    
    unsigned long now = millis();
    if (now - lastControlTime < controlInterval)
    {
        return; // Not time to control yet
    }
    lastControlTime = now;
    
    // Use AutoControlSystem to evaluate and generate commands
    AutoControlSystem::CommandQueue commandQueue;
    AutoControlSystem::evaluateAndControl(*currentProfile, snapshot, commandQueue);
    
    // Execute all commands
    for (size_t i = 0; i < commandQueue.count; ++i)
    {
        const RelayCommand& cmd = commandQueue.commands[i];
        relayManager->setRelay(cmd.relayIndex, cmd.state);
        
        Serial.printf("[ClimateManager] Relay %u -> %s\n",
                     cmd.relayIndex, cmd.state ? "ON" : "OFF");
    }
}

void ClimateManager::controlFan(bool on)
{
    if (relayManager)
    {
        relayManager->setRelay(CLIMATE_FAN_RELAY_INDEX, on);
        Serial.printf("[ClimateManager] Fan -> %s\n", on ? "ON" : "OFF");
    }
}

void ClimateManager::controlHeating(bool on)
{
    if (relayManager)
    {
        relayManager->setRelay(CLIMATE_HEATER_RELAY_INDEX, on);
        Serial.printf("[ClimateManager] Heater -> %s\n", on ? "ON" : "OFF");
    }
}

void ClimateManager::controlCooling(bool on)
{
    if (relayManager)
    {
        relayManager->setRelay(CLIMATE_COOLER_RELAY_INDEX, on);
        Serial.printf("[ClimateManager] Cooler -> %s\n", on ? "ON" : "OFF");
    }
}

void ClimateManager::controlHumidification(bool on)
{
    if (relayManager)
    {
        relayManager->setRelay(CLIMATE_HUMIDIFIER_RELAY_INDEX, on);
        Serial.printf("[ClimateManager] Humidifier -> %s\n", on ? "ON" : "OFF");
    }
}

void ClimateManager::controlDehumidification(bool on)
{
    if (relayManager)
    {
        relayManager->setRelay(CLIMATE_DEHUMIDIFIER_RELAY_INDEX, on);
        Serial.printf("[ClimateManager] Dehumidifier -> %s\n", on ? "ON" : "OFF");
    }
}

void ClimateManager::printStatus() const
{
    Serial.println("=== Climate Manager Status ===");
    if (currentProfile)
    {
        Serial.printf("Current Crop: %s\n", currentProfile->name);
        Serial.printf("Air Temp Range: %.1f-%.1f°C\n",
                     currentProfile->temperature.min, currentProfile->temperature.max);
        Serial.printf("Air Humidity Range: %.1f-%.1f%%\n",
                     currentProfile->airHumidity.min, currentProfile->airHumidity.max);
        Serial.printf("Soil Humidity Range: %.1f-%.1f%%\n",
                     currentProfile->soilHumidity.min, currentProfile->soilHumidity.max);
    }
    else
    {
        Serial.println("No profile set");
    }
    Serial.println("==============================");
}
