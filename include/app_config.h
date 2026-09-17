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
#define MQTT_DEVICE_MANUFACTURER "DIY"
#define MQTT_DEVICE_MODEL "ESP32 SmartGarden Controller"

// Home Assistant MQTT Broker
#define MQTT_BROKER "192.168.100.168"
#define MQTT_PORT 1883
#define MQTT_USERNAME "homer"
#define MQTT_PASSWORD "Danh@@@1992"
#define MQTT_RECONNECT_INTERVAL 5000
#define MQTT_BUFFER_SIZE 1024

// ==================== MQTT TOPICS ====================
#define MQTT_TOPIC_AVAILABILITY "smartgarden/status"
#define MQTT_TOPIC_AIR_TEMP "smartgarden/sensors/air_temp"
#define MQTT_TOPIC_AIR_HUMIDITY "smartgarden/sensors/air_humidity"
#define MQTT_TOPIC_SOIL_MOISTURE "smartgarden/sensors/soil_moisture"
#define MQTT_TOPIC_SOIL_TEMP "smartgarden/sensors/soil_temp"
#define MQTT_TOPIC_PH "smartgarden/sensors/ph"
#define MQTT_TOPIC_EC "smartgarden/sensors/ec"
#define MQTT_TOPIC_NITROGEN "smartgarden/sensors/nitrogen"
#define MQTT_TOPIC_PHOSPHORUS "smartgarden/sensors/phosphorus"
#define MQTT_TOPIC_POTASSIUM "smartgarden/sensors/potassium"
#define MQTT_TOPIC_SALINITY "smartgarden/sensors/salinity"
#define MQTT_TOPIC_TDS "smartgarden/sensors/tds"
#define MQTT_TOPIC_RSSI "smartgarden/system/rssi"
#define MQTT_TOPIC_UPTIME "smartgarden/system/uptime"
#define MQTT_TOPIC_FREE_HEAP "smartgarden/system/free_heap"
#define MQTT_TOPIC_MODE_STATE "smartgarden/mode/state"
#define MQTT_TOPIC_MODE_SET "smartgarden/mode/set"
#define MQTT_TOPIC_CROP_LIST "smartgarden/crop/list"
#define MQTT_TOPIC_CROP_CURRENT "smartgarden/crop/current"
#define MQTT_TOPIC_CROP_SELECT "smartgarden/crop/set"
#define MQTT_TOPIC_STATUS "smartgarden/status"

// Relay/Switch Topics - State & Command
#define MQTT_TOPIC_FAN "smartgarden/relay/1/state"
#define MQTT_TOPIC_HEATER "smartgarden/relay/2/state"
#define MQTT_TOPIC_COOLER "smartgarden/relay/3/state"
#define MQTT_TOPIC_HUMIDIFIER "smartgarden/relay/4/state"
#define MQTT_TOPIC_DEHUMIDIFIER "smartgarden/relay/5/state"
#define MQTT_TOPIC_IRRIGATION "smartgarden/relay/6/state"
#define MQTT_TOPIC_RELAY7 "smartgarden/relay/7/state"
#define MQTT_TOPIC_RELAY8 "smartgarden/relay/8/state"

#define MQTT_TOPIC_CONTROL_FAN "smartgarden/relay/1/set"
#define MQTT_TOPIC_CONTROL_HEATER "smartgarden/relay/2/set"
#define MQTT_TOPIC_CONTROL_COOLER "smartgarden/relay/3/set"
#define MQTT_TOPIC_CONTROL_HUMIDIFIER "smartgarden/relay/4/set"
#define MQTT_TOPIC_CONTROL_DEHUMIDIFIER "smartgarden/relay/5/set"
#define MQTT_TOPIC_CONTROL_IRRIGATION "smartgarden/relay/6/set"
#define MQTT_TOPIC_CONTROL_RELAY7 "smartgarden/relay/7/set"
#define MQTT_TOPIC_CONTROL_RELAY8 "smartgarden/relay/8/set"

// ==================== SENSOR CONFIGURATION ====================
#define SENSOR_READ_INTERVAL 5000
#define DHT_READ_TIMEOUT 2000
#define RS485_BAUD_RATE 4800
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
