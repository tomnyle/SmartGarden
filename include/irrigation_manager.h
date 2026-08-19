#ifndef IRRIGATION_MANAGER_H
#define IRRIGATION_MANAGER_H

#include <stdint.h>
#include "relay_manager.h"
#include "app_config.h"

class IrrigationManager {
public:
    IrrigationManager();
    
    void begin(RelayManager* relayMgr, uint8_t relayIdx = IRRIGATION_RELAY_INDEX);
    
    // Start/stop irrigation
    bool start(unsigned long maxDurationMs = IRRIGATION_MAX_DURATION);
    bool stop();
    
    // Check if irrigation is active
    bool isActive() const;
    
    // Get active duration in milliseconds
    unsigned long getActiveDuration() const;
    
    // Set maximum duration
    void setMaxDuration(unsigned long maxDurationMs);
    
    // Periodic check for timeout
    void loop();  // Must be called frequently
    
    // Print status
    void printStatus() const;
    
private:
    RelayManager* relayManager;
    bool active;
    unsigned long startTime;
    unsigned long maxDuration;
    uint8_t relayIndex;
};

#endif // IRRIGATION_MANAGER_H
