#ifndef ZONE_MANAGER_H
#define ZONE_MANAGER_H

#include <stdint.h>
#include <string.h>
#include "relay_manager.h"
#include "garden_profile.h"
#include "sensor_manager.h"
#include "climate_manager.h"
#include "app_config.h"

typedef struct {
    uint8_t id;                      // Zone ID
    char name[32];                   // Zone name
    uint8_t relayIndex;              // Associated relay index
    const CropProfile* profile;      // Current crop profile
    bool active;                     // Is zone active
} Zone;

class ZoneManager {
public:
    ZoneManager();
    
    void begin(RelayManager* relayMgr);
    
    // Add a new zone
    bool addZone(const char* name, uint8_t relayIndex, const CropProfile* profile);
    
    // Get zone by ID
    Zone* getZone(uint8_t zoneId);
    
    // Set zone active/inactive
    bool setZoneActive(uint8_t zoneId, bool active);
    
    // Change zone crop
    bool setZoneCrop(uint8_t zoneId, const CropProfile* profile);
    
    // Control zone based on sensor data
    bool controlZone(uint8_t zoneId, const SensorSnapshot& snapshot, ClimateManager* climateManager);
    
    // Get zone count
    uint8_t getZoneCount() const;
    
    // Print status
    void printStatus() const;
    
private:
    Zone zones[MAX_ZONES];
    uint8_t zoneCount;
    RelayManager* relayManager;
};

#endif // ZONE_MANAGER_H
