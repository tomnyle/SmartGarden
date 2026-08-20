#include "config.h"
#include <Arduino.h>
#include <EEPROM.h>

// Default configuration
static const SystemConfig DEFAULT_CONFIG = {
    .deviceName = "SmartGarden",
    .wifiSSID = "Le Danh",
    .wifiPassword = "123456789",
    .mqttBroker = "192.168.100.166",
    .mqttPort = 1883,
    .mqttUsername = "homer",
    .mqttPassword = "Danh@@@1992",
    .sensorReadInterval = 5000,
    .climateControlInterval = 10000,
    .publishInterval = 5000
};

#define CONFIG_EEPROM_ADDR 0
#define CONFIG_EEPROM_SIZE sizeof(SystemConfig)

SystemConfig getDefaultConfig()
{
    return DEFAULT_CONFIG;
}

bool loadConfig(SystemConfig& config)
{
    // Initialize EEPROM
    EEPROM.begin(512);
    
    // Read from EEPROM
    EEPROM.readBytes(CONFIG_EEPROM_ADDR, (uint8_t*)&config, CONFIG_EEPROM_SIZE);
    
    // Verify if config is valid (simple check)
    if (config.sensorReadInterval == 0 || config.sensorReadInterval > 60000)
    {
        Serial.println("[Config] Invalid EEPROM data, using default config");
        config = DEFAULT_CONFIG;
        return false;
    }
    
    Serial.println("[Config] Configuration loaded from EEPROM");
    return true;
}

bool saveConfig(const SystemConfig& config)
{
    // Initialize EEPROM
    EEPROM.begin(512);
    
    // Write to EEPROM
    EEPROM.writeBytes(CONFIG_EEPROM_ADDR, (const uint8_t*)&config, CONFIG_EEPROM_SIZE);
    
    // Commit changes
    bool result = EEPROM.commit();
    
    if (result)
    {
        Serial.println("[Config] Configuration saved to EEPROM");
    }
    else
    {
        Serial.println("[Config] Failed to save configuration");
    }
    
    return result;
}

void printConfig(const SystemConfig& config)
{
    Serial.println("\n========== System Configuration ==========");
    Serial.printf("Device Name       : %s\n", config.deviceName);
    Serial.printf("WiFi SSID         : %s\n", config.wifiSSID);
    Serial.printf("MQTT Broker       : %s:%u\n", config.mqttBroker, config.mqttPort);
    Serial.printf("MQTT Username     : %s\n", config.mqttUsername);
    Serial.printf("Sensor Interval   : %u ms\n", config.sensorReadInterval);
    Serial.printf("Climate Interval  : %u ms\n", config.climateControlInterval);
    Serial.printf("Publish Interval  : %u ms\n", config.publishInterval);
    Serial.println("==========================================\n");
}
