#ifndef CONFIG_H
#define CONFIG_H

#include "app_config.h"
#include "pins.h"

#define SMARTGARDEN_TOPIC_PREFIX "smartgarden"
#define SMARTGARDEN_MAX_RELAYS 8
#define SMARTGARDEN_MAX_CROPS 13

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

SystemConfig getDefaultConfig();
bool loadConfig(SystemConfig& config);
bool saveConfig(const SystemConfig& config);
void printConfig(const SystemConfig& config);

#endif // CONFIG_H
