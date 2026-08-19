#ifndef CLIMATE_MANAGER_H
#define CLIMATE_MANAGER_H

#include "relay_manager.h"
#include "sensor_manager.h"
#include "garden_profile.h"

class ClimateManager {
public:
    ClimateManager();
    
    void begin(RelayManager* relayMgr);
    
    // Set current crop profile for climate control
    bool setCurrentProfile(const CropProfile* profile);
    
    // Main control function - evaluates sensors and controls relays
    void control(const SensorSnapshot& snapshot);
    
    // Individual control functions
    void controlFan(bool on);
    void controlHeating(bool on);
    void controlCooling(bool on);
    void controlHumidification(bool on);
    void controlDehumidification(bool on);
    
    // Get current profile
    const CropProfile* getCurrentProfile() const { return currentProfile; }
    
    // Print status
    void printStatus() const;
    
private:
    RelayManager* relayManager;
    const CropProfile* currentProfile;
    unsigned long lastControlTime;
    unsigned long controlInterval;
};

#endif // CLIMATE_MANAGER_H
