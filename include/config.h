#ifndef CONFIG_H
#define CONFIG_H

#include "app_config.h"
#include "pins.h"

// System configuration
typedef struct {
    char deviceName[32];
    char wifiSSID[64];
    char wifiPassword[64];
    char mqttBroker[128];
    uint16_t mqttPort;
    char mqttUsername[64];
    char mqttPassword[64];
    uint32_t sensorReadInterval;
    uint32_t climateControlInterval;
    uint32_t publishInterval;
} SystemConfig;

// Get default system configuration
SystemConfig getDefaultConfig();

// Load configuration from EEPROM
bool loadConfig(SystemConfig& config);

// Save configuration to EEPROM
bool saveConfig(const SystemConfig& config);

// Print configuration to serial
void printConfig(const SystemConfig& config);

#endif // CONFIG_H
