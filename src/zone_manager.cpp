#include <Arduino.h>
#include "zone_manager.h"

ZoneManager::ZoneManager()
    : zoneCount(0), relayManager(nullptr)
{
    memset(zones, 0, sizeof(zones));
}

void ZoneManager::begin(RelayManager* relayMgr)
{
    relayManager = relayMgr;
    Serial.println("[ZoneManager] Initialized");
}

bool ZoneManager::addZone(const char* name, uint8_t relayIndex, const CropProfile* profile)
{
    if (zoneCount >= MAX_ZONES)
    {
        Serial.printf("[ZoneManager] ERROR: Max zones (%u) reached!\n", MAX_ZONES);
        return false;
    }
    
    Zone& zone = zones[zoneCount];
    zone.id = zoneCount + 1;
    zone.relayIndex = relayIndex;
    zone.profile = profile;
    zone.active = false;
    
    strncpy(zone.name, name, sizeof(zone.name) - 1);
    
    zoneCount++;
    Serial.printf("[ZoneManager] Zone added: ID=%u, Name=%s, Relay=%u, Crop=%s\n",
                 zone.id, zone.name, zone.relayIndex,
                 profile ? profile->name : "None");
    
    return true;
}

Zone* ZoneManager::getZone(uint8_t zoneId)
{
    if (zoneId < 1 || zoneId > zoneCount)
    {
        Serial.printf("[ZoneManager] ERROR: Invalid zone ID %u\n", zoneId);
        return nullptr;
    }
    
    return &zones[zoneId - 1];
}

bool ZoneManager::setZoneActive(uint8_t zoneId, bool active)
{
    Zone* zone = getZone(zoneId);
    if (zone == nullptr)
    {
        return false;
    }
    
    if (active && !zone->active)
    {
        // Activate zone
        if (relayManager != nullptr)
        {
            relayManager->setRelay(zone->relayIndex, true);
        }
        zone->active = true;
        Serial.printf("[ZoneManager] Zone %u activated\n", zoneId);
    }
    else if (!active && zone->active)
    {
        // Deactivate zone
        if (relayManager != nullptr)
        {
            relayManager->setRelay(zone->relayIndex, false);
        }
        zone->active = false;
        Serial.printf("[ZoneManager] Zone %u deactivated\n", zoneId);
    }
    
    return true;
}

bool ZoneManager::setZoneCrop(uint8_t zoneId, const CropProfile* profile)
{
    Zone* zone = getZone(zoneId);
    if (zone == nullptr)
    {
        return false;
    }
    
    zone->profile = profile;
    Serial.printf("[ZoneManager] Zone %u crop set to: %s\n",
                 zoneId, profile ? profile->name : "None");
    return true;
}

bool ZoneManager::controlZone(uint8_t zoneId, const SensorSnapshot& snapshot, ClimateManager* climateManager)
{
    Zone* zone = getZone(zoneId);
    if (zone == nullptr || zone->profile == nullptr)
    {
        Serial.printf("[ZoneManager] ERROR: Cannot control zone %u\n", zoneId);
        return false;
    }
    
    if (!zone->active)
    {
        return false; // Zone not active
    }
    
    // Use climate manager to control this zone's climate
    if (climateManager != nullptr)
    {
        climateManager->setCurrentProfile(zone->profile);
        climateManager->control(snapshot);
    }
    
    return true;
}

uint8_t ZoneManager::getZoneCount() const
{
    return zoneCount;
}

void ZoneManager::printStatus() const
{
    Serial.println("\n========== Zone Manager Status ==========");
    Serial.printf("Total Zones: %u\n", zoneCount);
    
    for (uint8_t i = 0; i < zoneCount; i++)
    {
        const Zone& zone = zones[i];
        Serial.printf("\n[Zone %u] %s\n", zone.id, zone.name);
        Serial.printf("  Status: %s\n", zone.active ? "Active" : "Inactive");
        Serial.printf("  Relay: %u\n", zone.relayIndex);
        Serial.printf("  Crop: %s\n", zone.profile ? zone.profile->name : "None");
        if (zone.profile)
        {
            Serial.printf("  Optimal Temp: %.1f-%.1f°C\n",
                         zone.profile->temperature.min, zone.profile->temperature.max);
        }
    }
    Serial.println("\n=========================================");
}
