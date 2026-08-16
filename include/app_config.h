#pragma once

// ========================================
// Smart Garden Configuration
// ========================================

#define DEVICE_NAME "SmartGarden"
#define DEVICE_ID   "smartgarden_01"

// WiFi
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT
#define MQTT_HOST     "192.168.1.100"
#define MQTT_PORT     1883

#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

#define MQTT_ROOT_TOPIC "smartgarden"

// Garden
#define ZONE_COUNT 8

// Sensor
#define SENSOR_UPDATE_MS 5000

// RS485
#define RS485_BAUDRATE 4800
#define SOIL_SENSOR_ID 1