#pragma once

#include <Arduino.h>

#ifndef SMARTGARDEN_DEVICE_NAME
#define SMARTGARDEN_DEVICE_NAME "SmartGarden"
#endif

#ifndef SMARTGARDEN_DEVICE_ID
#define SMARTGARDEN_DEVICE_ID "smartgarden_01"
#endif

#ifndef SMARTGARDEN_WIFI_SSID
#define SMARTGARDEN_WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef SMARTGARDEN_WIFI_PASSWORD
#define SMARTGARDEN_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef SMARTGARDEN_MQTT_HOST
#define SMARTGARDEN_MQTT_HOST "192.168.1.100"
#endif

#ifndef SMARTGARDEN_MQTT_PORT
#define SMARTGARDEN_MQTT_PORT 1883
#endif

#ifndef SMARTGARDEN_MQTT_USERNAME
#define SMARTGARDEN_MQTT_USERNAME ""
#endif

#ifndef SMARTGARDEN_MQTT_PASSWORD
#define SMARTGARDEN_MQTT_PASSWORD ""
#endif

#ifndef SMARTGARDEN_MQTT_ROOT_TOPIC
#define SMARTGARDEN_MQTT_ROOT_TOPIC "smartgarden"
#endif

constexpr size_t MAX_CROP_PROFILES = 8;
constexpr size_t MAX_RELAY_RULES = 8;
constexpr size_t MAX_ALERT_MESSAGES = 8;
constexpr size_t DATA_LOG_CAPACITY = 32;

constexpr uint8_t IRRIGATION_RELAY_INDEX = 0;

constexpr unsigned long SENSOR_UPDATE_MS = 5000UL;
constexpr unsigned long DASHBOARD_UPDATE_MS = 5000UL;

inline String mqttTopic(const char *suffix)
{
    String topic = SMARTGARDEN_MQTT_ROOT_TOPIC;
    topic += "/";
    topic += suffix;
    return topic;
}
