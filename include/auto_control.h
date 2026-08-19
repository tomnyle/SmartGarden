#ifndef AUTO_CONTROL_H
#define AUTO_CONTROL_H

#include <stdint.h>
#include <string.h>
#include "garden_profile.h"
#include "sensor_manager.h"
#include "relay_manager.h"

#define MAX_COMMANDS 8

typedef struct {
    RelayCommand commands[MAX_COMMANDS];
    uint8_t count;
} CommandQueue;

class AutoControlSystem {
public:
    // Evaluate current conditions and generate control commands
    static void evaluateAndControl(const CropProfile& profile,
                                  const SensorSnapshot& snapshot,
                                  CommandQueue& outCommands);
    
    // Check if temperature is within range
    static bool isTempInRange(float temp, const Range& range);
    
    // Check if humidity is within range
    static bool isHumidityInRange(float humidity, const Range& range);
    
    // Check if soil moisture is within range
    static bool isSoilMoistureInRange(float moisture, const Range& range);
    
    // Get recommended relay state for temperature control
    static RelayCommand getTempControlCommand(float temp, const Range& range);
    
    // Get recommended relay state for humidity control
    static RelayCommand getHumidityControlCommand(float humidity, const Range& range);
    
    // Get recommended relay state for soil moisture control
    static RelayCommand getSoilMoistureControlCommand(float moisture, const Range& range);
    
private:
    static void addCommand(CommandQueue& queue, uint8_t relayIndex, bool state);
};

#endif // AUTO_CONTROL_H
