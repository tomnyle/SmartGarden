#include "mqtt_service.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <string.h>

extern WiFiClient espClient;
extern PubSubClient client;

MQTTService::MQTTService(const char* broker, int port)
    : mqttBroker(broker), mqttPort(port), connected(false),
      lastPublishTime(0), publishInterval(5000),
      relayCallback(nullptr), cropCallback(nullptr)
{
    snprintf(deviceId, sizeof(deviceId), "smartgarden_%llX", ESP.getEfuseMac());
    mqttUsername[0] = '\0';
    mqttPassword[0] = '\0';
}

void MQTTService::begin(const char* username, const char* password)
{
    client.setServer(mqttBroker, mqttPort);
    client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->onMessageReceived(topic, payload, length);
    });

    strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
    mqttUsername[sizeof(mqttUsername) - 1] = '\0';
    strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);
    mqttPassword[sizeof(mqttPassword) - 1] = '\0';

    Serial.println("[MQTTService] Initialized");
}

bool MQTTService::connect()
{
    if (client.connected())
    {
        connected = true;
        return true;
    }

    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);

    const char* willTopic = "smartgarden/status/online";
    if (client.connect(deviceId, mqttUsername, mqttPassword, willTopic, 0, true, "OFF"))
    {
        connected = true;
        Serial.println("[MQTTService] Connected to MQTT broker");

        client.subscribe("smartgarden/relays/fan/set");
        client.subscribe("smartgarden/relays/heater/set");
        client.subscribe("smartgarden/relays/cooler/set");
        client.subscribe("smartgarden/relays/humidifier/set");
        client.subscribe("smartgarden/relays/dehumidifier/set");
        client.subscribe("smartgarden/relays/irrigation/set");
        client.subscribe("smartgarden/crop/select");

        publishStatus(true);
        return true;
    }

    connected = false;
    Serial.printf("[MQTTService] Connection failed, code=%d\n", client.state());
    return false;
}

bool MQTTService::isConnected() const
{
    return client.connected();
}

void MQTTService::loop()
{
    if (client.connected())
    {
        client.loop();
    }
}

bool MQTTService::publish(const char* topic, const char* payload)
{
    if (!client.connected())
    {
        return false;
    }
    return client.publish(topic, payload, true);
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client.connected())
    {
        return false;
    }

    unsigned long now = millis();
    if (now - lastPublishTime < publishInterval)
    {
        return false;
    }
    lastPublishTime = now;

    char payload[32];
    bool ok = true;

    snprintf(payload, sizeof(payload), "%.2f", snapshot.airTemp);
    ok &= publish("smartgarden/sensors/temperature", payload);

    snprintf(payload, sizeof(payload), "%.2f", snapshot.airHumidity);
    ok &= publish("smartgarden/sensors/humidity", payload);

    snprintf(payload, sizeof(payload), "%.2f", snapshot.soilMoisture);
    ok &= publish("smartgarden/sensors/soil_moisture", payload);

    snprintf(payload, sizeof(payload), "%lu", static_cast<unsigned long>(snapshot.timestamp));
    ok &= publish("smartgarden/sensors/timestamp", payload);

    return ok;
}

bool MQTTService::publishRelayState(const char* relayName, bool state)
{
    char topic[96];
    snprintf(topic, sizeof(topic), "smartgarden/relays/%s/state", relayName);
    return publish(topic, state ? "ON" : "OFF");
}

bool MQTTService::publishCropList()
{
    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);
    if (crops == nullptr || count == 0)
    {
        return false;
    }

    char payload[1024];
    size_t offset = 0;
    offset += snprintf(payload + offset, sizeof(payload) - offset, "[");
    for (uint8_t i = 0; i < count && (offset + 2) < sizeof(payload); ++i)
    {
        offset += snprintf(payload + offset, sizeof(payload) - offset,
                           "%s\"%s\"", (i == 0) ? "" : ",", crops[i].name);
    }
    snprintf(payload + offset, sizeof(payload) - offset, "]");

    return publish("smartgarden/crop/available", payload);
}

bool MQTTService::publishCurrentCrop(const char* cropName)
{
    return publish("smartgarden/crop/current", cropName);
}

bool MQTTService::publishStatus(bool online)
{
    return publish("smartgarden/status/online", online ? "ON" : "OFF");
}

bool MQTTService::publishUptime(uint32_t uptimeSeconds)
{
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", static_cast<unsigned long>(uptimeSeconds));
    return publish("smartgarden/status/uptime", payload);
}

bool MQTTService::publishDiscoverySensor(const char* objectId, const char* name,
                                         const char* stateTopic, const char* unit,
                                         const char* deviceClass)
{
    char topic[160];
    char payload[512];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/%s/config", deviceId, objectId);
    if (deviceClass != nullptr && deviceClass[0] != '\0')
    {
        snprintf(payload, sizeof(payload),
                 "{\"name\":\"%s\",\"unique_id\":\"%s_%s\",\"state_topic\":\"%s\","
                 "\"unit_of_measurement\":\"%s\",\"device_class\":\"%s\","
                 "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"SmartGarden\"}}",
                 name, deviceId, objectId, stateTopic, unit, deviceClass, deviceId);
    }
    else
    {
        snprintf(payload, sizeof(payload),
                 "{\"name\":\"%s\",\"unique_id\":\"%s_%s\",\"state_topic\":\"%s\","
                 "\"unit_of_measurement\":\"%s\","
                 "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"SmartGarden\"}}",
                 name, deviceId, objectId, stateTopic, unit, deviceId);
    }
    return publish(topic, payload);
}

bool MQTTService::publishDiscoverySwitch(const char* relayName, const char* displayName)
{
    char topic[160];
    char payload[640];
    snprintf(topic, sizeof(topic), "homeassistant/switch/%s/%s/config", deviceId, relayName);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"unique_id\":\"%s_%s\","
             "\"state_topic\":\"smartgarden/relays/%s/state\","
             "\"command_topic\":\"smartgarden/relays/%s/set\","
             "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"state_on\":\"ON\",\"state_off\":\"OFF\","
             "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"SmartGarden\"}}",
             displayName, deviceId, relayName, relayName, relayName, deviceId);
    return publish(topic, payload);
}

bool MQTTService::publishDiscoverySelect()
{
    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);
    if (crops == nullptr || count == 0)
    {
        return false;
    }

    char options[700];
    size_t offset = 0;
    offset += snprintf(options + offset, sizeof(options) - offset, "[");
    for (uint8_t i = 0; i < count && (offset + 2) < sizeof(options); ++i)
    {
        offset += snprintf(options + offset, sizeof(options) - offset,
                           "%s\"%s\"", (i == 0) ? "" : ",", crops[i].name);
    }
    snprintf(options + offset, sizeof(options) - offset, "]");

    char topic[160];
    char payload[1024];
    snprintf(topic, sizeof(topic), "homeassistant/select/%s/crop/config", deviceId);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"Crop Profile\",\"unique_id\":\"%s_crop_profile\","
             "\"state_topic\":\"smartgarden/crop/current\","
             "\"command_topic\":\"smartgarden/crop/select\","
             "\"options\":%s,"
             "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"SmartGarden\"}}",
             deviceId, options, deviceId);
    return publish(topic, payload);
}

bool MQTTService::publishDiscovery()
{
    bool ok = true;
    ok &= publishDiscoverySensor("temperature", "SmartGarden Temperature",
                                 "smartgarden/sensors/temperature", "°C", "temperature");
    ok &= publishDiscoverySensor("humidity", "SmartGarden Humidity",
                                 "smartgarden/sensors/humidity", "%", "humidity");
    ok &= publishDiscoverySensor("soil_moisture", "SmartGarden Soil Moisture",
                                 "smartgarden/sensors/soil_moisture", "%", nullptr);
    ok &= publishDiscoverySwitch("fan", "SmartGarden Fan");
    ok &= publishDiscoverySwitch("heater", "SmartGarden Heater");
    ok &= publishDiscoverySwitch("cooler", "SmartGarden Cooler");
    ok &= publishDiscoverySwitch("humidifier", "SmartGarden Humidifier");
    ok &= publishDiscoverySwitch("dehumidifier", "SmartGarden Dehumidifier");
    ok &= publishDiscoverySwitch("irrigation", "SmartGarden Irrigation");
    ok &= publishDiscoverySelect();
    return ok;
}

void MQTTService::setRelayCommandCallback(RelayCommandCallback callback)
{
    relayCallback = callback;
}

void MQTTService::setCropSelectCallback(CropSelectCallback callback)
{
    cropCallback = callback;
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length)
{
    char message[128];
    if (length >= sizeof(message))
    {
        length = sizeof(message) - 1;
    }
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("[MQTTService] Message received on %s: %s\n", topic, message);

    if (strcmp(topic, "smartgarden/crop/select") == 0)
    {
        if (cropCallback != nullptr)
        {
            cropCallback(message);
        }
        return;
    }

    static const char* relayNames[] = {
        "fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation"
    };

    for (size_t i = 0; i < sizeof(relayNames) / sizeof(relayNames[0]); ++i)
    {
        char expectedTopic[96];
        snprintf(expectedTopic, sizeof(expectedTopic), "smartgarden/relays/%s/set", relayNames[i]);
        if (strcmp(topic, expectedTopic) == 0 && relayCallback != nullptr)
        {
            bool isOn = strcmp(message, "ON") == 0;
            relayCallback(relayNames[i], isOn);
            return;
        }
    }
}
