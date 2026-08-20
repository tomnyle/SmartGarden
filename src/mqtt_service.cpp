#include "mqtt_service.h"
#include "garden_profile.h"
#include <WiFi.h>

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
        publishDiscoveryMessages();
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
    client->publish((String("smartgarden/") + deviceId + "/sensors/air_temperature").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    client->publish((String("smartgarden/") + deviceId + "/sensors/air_humidity").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    client->publish((String("smartgarden/") + deviceId + "/sensors/soil_moisture").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    client->publish((String("smartgarden/") + deviceId + "/sensors/soil_temperature").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    client->publish((String("smartgarden/") + deviceId + "/sensors/ph").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    client->publish((String("smartgarden/") + deviceId + "/sensors/ec").c_str(), payload);
    
    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    client->publish((String("smartgarden/") + deviceId + "/sensors/nitrogen").c_str(), payload);
    
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
    
    String topic = String("smartgarden/") + deviceId + "/relays/" + relayNames[relayIndex];
    return client->publish(topic.c_str(), state ? "ON" : "OFF");
}

bool MQTTService::publishAllRelayStatus(const RelayManager* relayMgr)
{
    if (!relayMgr) return false;
    
    for (uint8_t i = 0; i < 6; i++) {
        bool state = relayMgr->getRelayState(i);
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
    
    String discoveryPrefix = "homeassistant";
    String deviceName = "SmartGarden";
    
    // Device info for grouping
    char deviceInfo[256];
    snprintf(deviceInfo, sizeof(deviceInfo),
        "\"identifiers\":[\"smartgarden\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32\",\"name\":\"%s\",\"sw_version\":\"1.0.0\"",
        deviceName.c_str());
    
    // ===== SENSORS =====
    
    // Air Temperature Sensor
    char tempConfig[512];
    snprintf(tempConfig, sizeof(tempConfig),
        "{\"device\":{%s},\"device_class\":\"temperature\",\"name\":\"SmartGarden Air Temperature\","
        "\"state_class\":\"measurement\",\"state_topic\":\"smartgarden/%s/sensors/air_temperature\","
        "\"unique_id\":\"smartgarden_air_temp\",\"unit_of_measurement\":\"°C\",\"value_template\":\"{{ value }}\",\"platform\":\"mqtt\"}",
        deviceInfo, deviceId);
    client->publish((discoveryPrefix + "/sensor/smartgarden_air_temp/config").c_str(), tempConfig, true);
    
    // Air Humidity Sensor
    char humidConfig[512];
    snprintf(humidConfig, sizeof(humidConfig),
        "{\"device\":{%s},\"device_class\":\"humidity\",\"name\":\"SmartGarden Air Humidity\","
        "\"state_class\":\"measurement\",\"state_topic\":\"smartgarden/%s/sensors/air_humidity\","
        "\"unique_id\":\"smartgarden_air_humidity\",\"unit_of_measurement\":\"%%\",\"value_template\":\"{{ value }}\",\"platform\":\"mqtt\"}",
        deviceInfo, deviceId);
    client->publish((discoveryPrefix + "/sensor/smartgarden_air_humidity/config").c_str(), humidConfig, true);
    
    // Soil Moisture Sensor
    char soilConfig[512];
    snprintf(soilConfig, sizeof(soilConfig),
        "{\"device\":{%s},\"device_class\":\"moisture\",\"name\":\"SmartGarden Soil Moisture\","
        "\"state_class\":\"measurement\",\"state_topic\":\"smartgarden/%s/sensors/soil_moisture\","
        "\"unique_id\":\"smartgarden_soil_moisture\",\"unit_of_measurement\":\"%%\",\"value_template\":\"{{ value }}\",\"platform\":\"mqtt\"}",
        deviceInfo, deviceId);
    client->publish((discoveryPrefix + "/sensor/smartgarden_soil_moisture/config").c_str(), soilConfig, true);
    
    // Soil Temperature Sensor
    char soilTempConfig[512];
    snprintf(soilTempConfig, sizeof(soilTempConfig),
        "{\"device\":{%s},\"device_class\":\"temperature\",\"name\":\"SmartGarden Soil Temperature\","
        "\"state_class\":\"measurement\",\"state_topic\":\"smartgarden/%s/sensors/soil_temperature\","
        "\"unique_id\":\"smartgarden_soil_temp\",\"unit_of_measurement\":\"°C\",\"value_template\":\"{{ value }}\",\"platform\":\"mqtt\"}",
        deviceInfo, deviceId);
    client->publish((discoveryPrefix + "/sensor/smartgarden_soil_temp/config").c_str(), soilTempConfig, true);
    
    // ===== SWITCHES (RELAYS) =====
    
    const char* relayNames[] = {"Circulation Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation"};
    const char* relayIds[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation"};
    
    for (uint8_t i = 0; i < 6; i++) {
        char switchConfig[512];
        snprintf(switchConfig, sizeof(switchConfig),
            "{\"device\":{%s},\"name\":\"SmartGarden %s\",\"payload_off\":\"OFF\",\"payload_on\":\"ON\","
            "\"state_topic\":\"smartgarden/%s/relays/%s\",\"command_topic\":\"smartgarden/%s/relays/%s/set\","
            "\"unique_id\":\"smartgarden_%s\",\"platform\":\"mqtt\"}",
            deviceInfo, relayNames[i], deviceId, relayIds[i], deviceId, relayIds[i], relayIds[i]);
        
        String switchTopic = discoveryPrefix + "/switch/smartgarden_" + relayIds[i] + "/config";
        client->publish(switchTopic.c_str(), switchConfig, true);
    }
    
    // ===== SELECT (Crop Profile) =====
    
    char selectConfig[1024];
    snprintf(selectConfig, sizeof(selectConfig),
        "{\"device\":{%s},\"name\":\"SmartGarden Crop Profile\",\"command_topic\":\"smartgarden/%s/crop/select\","
        "\"state_topic\":\"smartgarden/%s/crop/current\",\"unique_id\":\"smartgarden_crop\","
        "\"options\":[\"Sâm\",\"Cà chua\",\"Dâu tây\",\"Rau mầm\",\"Cải kale\",\"Bánh chua\","
        "\"Thơm\",\"Xà lách\",\"Ớt\",\"Cúc hoa mi\",\"Chanh\",\"Bạc hà\",\"Tỏi\"],\"platform\":\"mqtt\"}",
        deviceInfo, deviceId, deviceId);
    
    client->publish((discoveryPrefix + "/select/smartgarden_crop/config").c_str(), selectConfig, true);
    
    Serial.println("[MQTTService] Discovery messages published!");
}