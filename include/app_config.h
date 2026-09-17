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

// ==================== MQTT TOPICS ====================
// Runtime Topics Prefix
#define MQTT_RUNTIME_PREFIX "smartgarden"

// Availability
#define MQTT_TOPIC_AVAILABILITY MQTT_RUNTIME_PREFIX "/availability"

// Sensor Topics - State
#define MQTT_TOPIC_AIR_TEMP MQTT_RUNTIME_PREFIX "/sensors/air_temp"
#define MQTT_TOPIC_AIR_HUMIDITY MQTT_RUNTIME_PREFIX "/sensors/air_humidity"
#define MQTT_TOPIC_SOIL_MOISTURE MQTT_RUNTIME_PREFIX "/sensors/soil_moisture"
#define MQTT_TOPIC_SOIL_TEMP MQTT_RUNTIME_PREFIX "/sensors/soil_temp"
#define MQTT_TOPIC_PH MQTT_RUNTIME_PREFIX "/sensors/ph"
#define MQTT_TOPIC_EC MQTT_RUNTIME_PREFIX "/sensors/ec"
#define MQTT_TOPIC_NITROGEN MQTT_RUNTIME_PREFIX "/sensors/nitrogen"
#define MQTT_TOPIC_PHOSPHORUS MQTT_RUNTIME_PREFIX "/sensors/phosphorus"
#define MQTT_TOPIC_POTASSIUM MQTT_RUNTIME_PREFIX "/sensors/potassium"

// Relay/Switch Topics - State & Command
#define MQTT_TOPIC_FAN MQTT_RUNTIME_PREFIX "/relay/1/state"
#define MQTT_TOPIC_HEATER MQTT_RUNTIME_PREFIX "/relay/2/state"
#define MQTT_TOPIC_COOLER MQTT_RUNTIME_PREFIX "/relay/3/state"
#define MQTT_TOPIC_HUMIDIFIER MQTT_RUNTIME_PREFIX "/relay/4/state"
#define MQTT_TOPIC_DEHUMIDIFIER MQTT_RUNTIME_PREFIX "/relay/5/state"
#define MQTT_TOPIC_IRRIGATION MQTT_RUNTIME_PREFIX "/relay/6/state"

#define MQTT_TOPIC_CONTROL_FAN MQTT_RUNTIME_PREFIX "/relay/1/set"
#define MQTT_TOPIC_CONTROL_HEATER MQTT_RUNTIME_PREFIX "/relay/2/set"
#define MQTT_TOPIC_CONTROL_COOLER MQTT_RUNTIME_PREFIX "/relay/3/set"
#define MQTT_TOPIC_CONTROL_HUMIDIFIER MQTT_RUNTIME_PREFIX "/relay/4/set"
#define MQTT_TOPIC_CONTROL_DEHUMIDIFIER MQTT_RUNTIME_PREFIX "/relay/5/set"
#define MQTT_TOPIC_CONTROL_IRRIGATION MQTT_RUNTIME_PREFIX "/relay/6/set"

// Status, Crop Select and Operation Mode
#define MQTT_TOPIC_STATUS MQTT_RUNTIME_PREFIX "/status"
#define MQTT_TOPIC_CROP_SELECT MQTT_RUNTIME_PREFIX "/crop/set"
#define MQTT_TOPIC_CROP_STATE MQTT_RUNTIME_PREFIX "/crop/state"
#define MQTT_TOPIC_CROP_LIST MQTT_RUNTIME_PREFIX "/crop/list"
#define MQTT_TOPIC_MODE_SET MQTT_RUNTIME_PREFIX "/mode/set"
#define MQTT_TOPIC_MODE_STATE MQTT_RUNTIME_PREFIX "/mode/state"
#define MQTT_TOPIC_UPTIME MQTT_RUNTIME_PREFIX "/uptime"

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
