#include "mqtt_service.h"
#include "garden_profile.h"
#include <WiFi.h>
#include <ArduinoJson.h>

// Global MQTT service pointer for callback
static MQTTService* g_mqttService = nullptr;

// Global MQTT callback
void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
    if (g_mqttService) {
        g_mqttService->onMessageReceived(topic, payload, length);
    }
}

MQTTService::MQTTService(const char* broker, int port)
    : client(nullptr), mqttBroker(broker), mqttPort(port), connected(false),
      lastPublishTime(0), publishInterval(5000), lastDiscoveryTime(0),
      relayCallback(nullptr), cropCallback(nullptr)
{
    strncpy(deviceId, "SmartGarden_", sizeof(deviceId) - 1);
    strncat(deviceId, String(ESP.getEfuseMac()).c_str(), sizeof(deviceId) - strlen(deviceId) - 1);
    memset(mqttUsername, 0, sizeof(mqttUsername));
    memset(mqttPassword, 0, sizeof(mqttPassword));
    g_mqttService = this;
}

void MQTTService::setClient(PubSubClient* c) {
    client = c;
}

void MQTTService::begin(const char* username, const char* password)
{
    if (!client) {
        Serial.println("[MQTTService] ERROR: PubSubClient not set!");
        return;
    }

    client->setServer(mqttBroker, mqttPort);
    client->setCallback(mqttMessageCallback);
    
    strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
    strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);
    
    Serial.println("[MQTTService] Initialized");
}

void MQTTService::setRelayCommandCallback(RelayCommandCallback callback) {
    relayCallback = callback;
}

void MQTTService::setCropSelectCallback(CropSelectCallback callback) {
    cropCallback = callback;
}

bool MQTTService::connect()
{
    if (!client) return false;
    
    if (client->connected()) {
        return true;
    }
    
    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);
    
    if (client->connect(deviceId, mqttUsername, mqttPassword))
    {
        Serial.println("[MQTTService] Connected to MQTT broker");
        connected = true;
        subscribeToTopics();
        publishStatus("online");
        publishCropList();
        return true;
    }
    else
    {
        Serial.printf("[MQTTService] Connection failed, code=%d\n", client->state());
        connected = false;
        return false;
    }
}

bool MQTTService::isConnected() const
{
    return client && client->connected();
}

void MQTTService::loop()
{
    if (!client) return;
    
    if (!client->connected())
    {
        if (millis() % 5000 == 0) {
            connect();
        }
    }
    else
    {
        client->loop();
        
        // Publish discovery messages once per connection
        if (millis() - lastDiscoveryTime > 30000) {
            lastDiscoveryTime = millis();
            publishDiscoveryMessages();
        }
    }
}

bool MQTTService::publish(const char* topic, const char* payload)
{
    if (!client || !client->connected()) {
        return false;
    }
    
    String fullTopic = String("smartgarden/") + String(deviceId) + "/" + String(topic);
    bool result = client->publish(fullTopic.c_str(), payload);
    
    if (!result) {
        Serial.printf("[MQTTService] Publish failed: %s\n", fullTopic.c_str());
    }
    
    return result;
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client || !client->connected()) {
        return false;
    }
    
    unsigned long now = millis();
    if (now - lastPublishTime < publishInterval) {
        return false;
    }
    lastPublishTime = now;
    
    // Publish individual sensor topics for Home Assistant
    char payload[64];
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    client->publish((String("smartgarden/") + deviceId + "/sensors/temperature").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    client->publish((String("smartgarden/") + deviceId + "/sensors/humidity").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    client->publish((String("smartgarden/") + deviceId + "/sensors/soil_moisture").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    client->publish((String("smartgarden/") + deviceId + "/sensors/soil_temp").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    client->publish((String("smartgarden/") + deviceId + "/sensors/ph").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    client->publish((String("smartgarden/") + deviceId + "/sensors/ec").c_str(), payload);
    
    Serial.println("[MQTTService] Sensor data published");
    return true;
}

bool MQTTService::publishRelayStatus(uint8_t relayIndex, bool state)
{
    if (!client || !client->connected()) {
        return false;
    }
    
    const char* relayNames[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation"};
    if (relayIndex >= 6) return false;
    
    String topic = String("smartgarden/") + deviceId + "/relays/" + relayNames[relayIndex] + "/state";
    return client->publish(topic.c_str(), state ? "ON" : "OFF");
}

bool MQTTService::publishAllRelayStatus(const RelayManager* relayMgr)
{
    if (!relayMgr) return false;
    
    for (uint8_t i = 0; i < 6; i++) {
        bool state = relayMgr->getState(i);
        publishRelayStatus(i, state);
    }
    
    return true;
}

bool MQTTService::publishCropList()
{
    if (!client || !client->connected()) {
        return false;
    }
    
    CropProfileStore::initialize();
    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);
    
    char payload[1024];
    strcpy(payload, "[");
    
    for (uint8_t i = 0; i < count; i++) {
        if (i > 0) strcat(payload, ",");
        char crop[128];
        snprintf(crop, sizeof(crop), "{\"id\":%u,\"name\":\"%s\"}", crops[i].id, crops[i].name);
        strcat(payload, crop);
    }
    strcat(payload, "]");
    
    String topic = String("smartgarden/") + deviceId + "/crop/available";
    bool result = client->publish(topic.c_str(), payload);
    
    if (result) {
        Serial.println("[MQTTService] Crop list published");
    }
    return result;
}

bool MQTTService::publishCurrentCrop(const CropProfile* profile)
{
    if (!client || !client->connected() || !profile) {
        return false;
    }
    
    String topic = String("smartgarden/") + deviceId + "/crop/current";
    return client->publish(topic.c_str(), profile->name);
}

bool MQTTService::publishStatus(const char* status)
{
    if (!client || !client->connected()) {
        return false;
    }
    
    String topic = String("smartgarden/") + deviceId + "/status";
    return client->publish(topic.c_str(), status);
}

bool MQTTService::publishUptime(unsigned long uptime)
{
    if (!client || !client->connected()) {
        return false;
    }
    
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", uptime / 1000);
    
    String topic = String("smartgarden/") + deviceId + "/uptime";
    return client->publish(topic.c_str(), payload);
}

void MQTTService::subscribeToTopics()
{
    if (!client) return;
    
    String baseTopic = String("smartgarden/") + deviceId + "/";
    
    // Subscribe to relay commands
    client->subscribe((baseTopic + "relays/fan/set").c_str());
    client->subscribe((baseTopic + "relays/heater/set").c_str());
    client->subscribe((baseTopic + "relays/cooler/set").c_str());
    client->subscribe((baseTopic + "relays/humidifier/set").c_str());
    client->subscribe((baseTopic + "relays/dehumidifier/set").c_str());
    client->subscribe((baseTopic + "relays/irrigation/set").c_str());
    
    // Subscribe to crop selection
    client->subscribe((baseTopic + "crop/select").c_str());
    
    Serial.println("[MQTTService] Subscribed to topics");
}

void MQTTService::handleRelayCommand(const char* relayName, const char* payload)
{
    const char* relayNames[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation"};
    uint8_t relayIndex = 0xFF;
    
    for (uint8_t i = 0; i < 6; i++) {
        if (strcmp(relayName, relayNames[i]) == 0) {
            relayIndex = i;
            break;
        }
    }
    
    if (relayIndex == 0xFF) {
        Serial.printf("[MQTTService] Unknown relay: %s\n", relayName);
        return;
    }
    
    bool state = (strcmp(payload, "ON") == 0 || strcmp(payload, "1") == 0);
    
    Serial.printf("[MQTTService] Relay command: %s = %s\n", relayName, state ? "ON" : "OFF");
    
    if (relayCallback) {
        relayCallback(relayIndex, state);
    }
}

void MQTTService::handleCropSelect(const char* payload)
{
    Serial.printf("[MQTTService] Crop select: %s\n", payload);
    
    if (cropCallback) {
        cropCallback(payload);
    }
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length)
{
    // Null-terminate payload
    char message[256];
    if (length >= sizeof(message)) {
        length = sizeof(message) - 1;
    }
    strncpy(message, (char*)payload, length);
    message[length] = '\0';
    
    Serial.printf("[MQTTService] Message: %s = %s\n", topic, message);
    
    // Parse topic and route to handlers
    String topicStr(topic);
    
    if (topicStr.indexOf("/relays/") > 0) {
        if (topicStr.indexOf("/fan/set") > 0) {
            handleRelayCommand("fan", message);
        } else if (topicStr.indexOf("/heater/set") > 0) {
            handleRelayCommand("heater", message);
        } else if (topicStr.indexOf("/cooler/set") > 0) {
            handleRelayCommand("cooler", message);
        } else if (topicStr.indexOf("/humidifier/set") > 0) {
            handleRelayCommand("humidifier", message);
        } else if (topicStr.indexOf("/dehumidifier/set") > 0) {
            handleRelayCommand("dehumidifier", message);
        } else if (topicStr.indexOf("/irrigation/set") > 0) {
            handleRelayCommand("irrigation", message);
        }
    } else if (topicStr.indexOf("/crop/select") > 0) {
        handleCropSelect(message);
    }
}

void MQTTService::publishDiscoveryMessages()
{
    if (!client || !client->connected()) {
        return;
    }
    
    Serial.println("[MQTTService] Publishing Home Assistant discovery messages");
    
    // Discovery prefix for Home Assistant
    String discoveryPrefix = "homeassistant/sensor/" + String(deviceId);
    
    // Temperature sensor
    char tempConfig[512];
    snprintf(tempConfig, sizeof(tempConfig),
        "{\"name\":\"Garden Temperature\",\"unique_id\":\"%s_temp\",\"state_topic\":\"smartgarden/%s/sensors/temperature\","
        "\"unit_of_measurement\":\"°C\",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"SmartGarden\",\"model\":\"ESP32\",\"manufacturer\":\"DIY\"}}",
        deviceId, deviceId, deviceId);
    client->publish((discoveryPrefix + "_temp/config").c_str(), tempConfig, true);
    
    // Humidity sensor
    char humidityConfig[512];
    snprintf(humidityConfig, sizeof(humidityConfig),
        "{\"name\":\"Garden Humidity\",\"unique_id\":\"%s_humidity\",\"state_topic\":\"smartgarden/%s/sensors/humidity\","
        "\"unit_of_measurement\":\"%%\",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"SmartGarden\",\"model\":\"ESP32\",\"manufacturer\":\"DIY\"}}",
        deviceId, deviceId, deviceId);
    client->publish((discoveryPrefix + "_humidity/config").c_str(), humidityConfig, true);
    
    // Soil moisture sensor
    char soilConfig[512];
    snprintf(soilConfig, sizeof(soilConfig),
        "{\"name\":\"Soil Moisture\",\"unique_id\":\"%s_soil\",\"state_topic\":\"smartgarden/%s/sensors/soil_moisture\","
        "\"unit_of_measurement\":\"%%\",\"device\":{\"identifiers\":[\"SmartGarden\"],\"name\":\"SmartGarden\",\"model\":\"ESP32\",\"manufacturer\":\"DIY\"}}",
        deviceId, deviceId);
    client->publish((discoveryPrefix + "_soil/config").c_str(), soilConfig, true);
}
