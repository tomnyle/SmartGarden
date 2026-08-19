#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <time.h>

// Application version
#define APP_VERSION "1.0.0"
#define APP_NAME "SmartGarden"

// Debug mode
#define DEBUG_MODE 1

// WiFi Configuration
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define WIFI_TIMEOUT 20000  // 20 seconds

// MQTT Configuration
#define MQTT_BROKER "your_mqtt_broker"
#define MQTT_PORT 1883
#define MQTT_USERNAME "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"
#define MQTT_RECONNECT_INTERVAL 5000  // 5 seconds
#define MQTT_KEEPALIVE 60

// Sensor Configuration
#define SENSOR_READ_INTERVAL 5000  // 5 seconds
#define DHT_READ_TIMEOUT 2000  // 2 seconds
#define RS485_BAUD_RATE 9600
#define RS485_READ_TIMEOUT 1000  // 1 second

// Climate Control Configuration
#define CLIMATE_CONTROL_INTERVAL 10000  // 10 seconds
#define CLIMATE_FAN_RELAY_INDEX 0
#define CLIMATE_HEATER_RELAY_INDEX 1
#define CLIMATE_COOLER_RELAY_INDEX 2
#define CLIMATE_HUMIDIFIER_RELAY_INDEX 3
#define CLIMATE_DEHUMIDIFIER_RELAY_INDEX 4

// Irrigation Configuration
#define IRRIGATION_RELAY_INDEX 5
#define IRRIGATION_MAX_DURATION 3600000  // 1 hour max
#define IRRIGATION_CHECK_INTERVAL 1000  // 1 second

// Data Logging Configuration
#define LOG_INTERVAL 60000  // 1 minute
#define LOG_BUFFER_SIZE 256

// Zone Configuration
#define MAX_ZONES 4
#define ZONE_CONTROL_INTERVAL 10000  // 10 seconds

// Publishing Configuration
#define PUBLISH_SENSOR_INTERVAL 5000  // 5 seconds
#define PUBLISH_STATUS_INTERVAL 30000  // 30 seconds

// Temperature and Humidity Thresholds
#define TEMP_CRITICAL_LOW -10.0f
#define TEMP_CRITICAL_HIGH 50.0f
#define HUMIDITY_CRITICAL_HIGH 100.0f
#define HUMIDITY_CRITICAL_LOW 0.0f

#endif // APP_CONFIG_H
