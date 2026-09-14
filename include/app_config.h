#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <time.h>

#define APP_VERSION "1.0.0"
#define APP_NAME "SmartGarden"
#define DEBUG_MODE 1

#define WIFI_SSID "Le Danh"
#define WIFI_PASSWORD "123456789"
#define WIFI_TIMEOUT 20000

// ==================== HOME ASSISTANT MQTT DISCOVERY ====================
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_DISCOVERY_ENABLED true

// MQTT Device Configuration
#define MQTT_DEVICE_ID "smartgarden_esp32"
#define MQTT_DEVICE_NAME "Smart Garden"

// Home Assistant MQTT Broker
#define MQTT_BROKER "192.168.100.168"
#define MQTT_PORT 1883
#define MQTT_USERNAME "homer"
#define MQTT_PASSWORD "Danh@@@1992"
#define MQTT_RECONNECT_INTERVAL 5000
#define MQTT_BUFFER_SIZE 512

// ==================== MQTT TOPICS (Home Assistant Format) ====================
// Sensor Topics - State
#define MQTT_TOPIC_AIR_TEMP HA_DISCOVERY_PREFIX "/sensor/smartgarden_air_temp/state"
#define MQTT_TOPIC_AIR_HUMIDITY HA_DISCOVERY_PREFIX "/sensor/smartgarden_air_humidity/state"
#define MQTT_TOPIC_SOIL_MOISTURE HA_DISCOVERY_PREFIX "/sensor/smartgarden_soil_moisture/state"
#define MQTT_TOPIC_SOIL_TEMP HA_DISCOVERY_PREFIX "/sensor/smartgarden_soil_temp/state"
#define MQTT_TOPIC_PH HA_DISCOVERY_PREFIX "/sensor/smartgarden_ph/state"
#define MQTT_TOPIC_EC HA_DISCOVERY_PREFIX "/sensor/smartgarden_ec/state"
#define MQTT_TOPIC_NITROGEN HA_DISCOVERY_PREFIX "/sensor/smartgarden_nitrogen/state"
#define MQTT_TOPIC_PHOSPHORUS HA_DISCOVERY_PREFIX "/sensor/smartgarden_phosphorus/state"
#define MQTT_TOPIC_POTASSIUM HA_DISCOVERY_PREFIX "/sensor/smartgarden_potassium/state"

// Relay/Switch Topics - State & Command
#define MQTT_TOPIC_FAN HA_DISCOVERY_PREFIX "/switch/smartgarden_fan/state"
#define MQTT_TOPIC_HEATER HA_DISCOVERY_PREFIX "/switch/smartgarden_heater/state"
#define MQTT_TOPIC_COOLER HA_DISCOVERY_PREFIX "/switch/smartgarden_cooler/state"
#define MQTT_TOPIC_HUMIDIFIER HA_DISCOVERY_PREFIX "/switch/smartgarden_humidifier/state"
#define MQTT_TOPIC_DEHUMIDIFIER HA_DISCOVERY_PREFIX "/switch/smartgarden_dehumidifier/state"
#define MQTT_TOPIC_IRRIGATION HA_DISCOVERY_PREFIX "/switch/smartgarden_irrigation/state"

#define MQTT_TOPIC_CONTROL_FAN HA_DISCOVERY_PREFIX "/switch/smartgarden_fan/command"
#define MQTT_TOPIC_CONTROL_HEATER HA_DISCOVERY_PREFIX "/switch/smartgarden_heater/command"
#define MQTT_TOPIC_CONTROL_COOLER HA_DISCOVERY_PREFIX "/switch/smartgarden_cooler/command"
#define MQTT_TOPIC_CONTROL_HUMIDIFIER HA_DISCOVERY_PREFIX "/switch/smartgarden_humidifier/command"
#define MQTT_TOPIC_CONTROL_DEHUMIDIFIER HA_DISCOVERY_PREFIX "/switch/smartgarden_dehumidifier/command"
#define MQTT_TOPIC_CONTROL_IRRIGATION HA_DISCOVERY_PREFIX "/switch/smartgarden_irrigation/command"

// Status & Crop Select
#define MQTT_TOPIC_STATUS HA_DISCOVERY_PREFIX "/switch/smartgarden_status/state"
#define MQTT_TOPIC_CROP_SELECT HA_DISCOVERY_PREFIX "/select/smartgarden_crop/command"

// ==================== SENSOR CONFIGURATION ====================
#define SENSOR_READ_INTERVAL 5000
#define DHT_READ_TIMEOUT 2000
#define RS485_BAUD_RATE 9600
#define RS485_READ_TIMEOUT 1000

// ==================== CLIMATE CONTROL ====================
#define CLIMATE_CONTROL_INTERVAL 10000
#define CLIMATE_FAN_RELAY_INDEX 0
#define CLIMATE_HEATER_RELAY_INDEX 1
#define CLIMATE_COOLER_RELAY_INDEX 2
#define CLIMATE_HUMIDIFIER_RELAY_INDEX 3
#define CLIMATE_DEHUMIDIFIER_RELAY_INDEX 4

// ==================== IRRIGATION CONTROL ====================
#define IRRIGATION_RELAY_INDEX 5
#define IRRIGATION_MAX_DURATION 3600000
#define IRRIGATION_CHECK_INTERVAL 1000

// ==================== LOGGING ====================
#define LOG_INTERVAL 60000
#define LOG_BUFFER_SIZE 256

// ==================== ZONES ====================
#define MAX_ZONES 4
#define ZONE_CONTROL_INTERVAL 10000

// ==================== MQTT PUBLISH INTERVALS ====================
#define PUBLISH_SENSOR_INTERVAL 5000
#define PUBLISH_STATUS_INTERVAL 30000

// ==================== ALERT THRESHOLDS ====================
#define TEMP_CRITICAL_LOW -10.0f
#define TEMP_CRITICAL_HIGH 50.0f
#define HUMIDITY_CRITICAL_HIGH 100.0f
#define HUMIDITY_CRITICAL_LOW 0.0f

#endif
