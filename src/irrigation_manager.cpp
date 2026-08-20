#include "irrigation_manager.h"
#include <Arduino.h>

IrrigationManager::IrrigationManager()
    : active(false), startTime(0), maxDuration(3600000), // 1 hour max by default
      relayManager(nullptr), relayIndex(IRRIGATION_RELAY_INDEX)
{
}

void IrrigationManager::begin(RelayManager* relayMgr, uint8_t relayIdx)
{
    relayManager = relayMgr;
    relayIndex = relayIdx;
    active = false;
    Serial.printf("[IrrigationManager] Initialized on relay %u\n", relayIndex);
}

bool IrrigationManager::start(unsigned long maxDurationMs)
{
    if (active)
    {
        Serial.println("[IrrigationManager] Already active!");
        return false;
    }
    
    if (relayManager == nullptr)
    {
        Serial.println("[IrrigationManager] ERROR: RelayManager not set!");
        return false;
    }
    
    active = true;
    startTime = millis();
    maxDuration = maxDurationMs;
    relayManager->setRelay(relayIndex, true);
    
    Serial.printf("[IrrigationManager] Started (max duration: %lu ms)\n", maxDurationMs);
    return true;
}

bool IrrigationManager::stop()
{
    if (!active)
    {
        return false;
    }
    
    if (relayManager != nullptr)
    {
        relayManager->setRelay(relayIndex, false);
    }
    
    unsigned long duration = millis() - startTime;
    active = false;
    
    Serial.printf("[IrrigationManager] Stopped after %lu ms\n", duration);
    return true;
}

bool IrrigationManager::isActive() const
{
    return active;
}

void IrrigationManager::loop()
{
    if (!active)
    {
        return; // Nothing to do if not active
    }
    
    unsigned long elapsed = millis() - startTime;
    
    // Check for timeout
    if (elapsed >= maxDuration)
    {
        Serial.printf("[IrrigationManager] Max duration reached (%lu ms), stopping...\n", elapsed);
        stop();
    }
}

unsigned long IrrigationManager::getActiveDuration() const
{
    if (!active)
    {
        return 0;
    }
    return millis() - startTime;
}

void IrrigationManager::setMaxDuration(unsigned long maxDurationMs)
{
    maxDuration = maxDurationMs;
    Serial.printf("[IrrigationManager] Max duration set to %lu ms\n", maxDurationMs);
}

void IrrigationManager::printStatus() const
{
    Serial.println("=== Irrigation Status ===");
    Serial.printf("Status: %s\n", active ? "Active" : "Idle");
    if (active)
    {
        Serial.printf("Duration: %lu ms\n", getActiveDuration());
        Serial.printf("Max Duration: %lu ms\n", maxDuration);
    }
    Serial.printf("Relay Index: %u\n", relayIndex);
    Serial.println("=========================");
}
