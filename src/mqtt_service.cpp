#include "mqtt_service.h"

#include <WiFi.h>

#include "app_config.h"
#include "crop_profiles.h"

static MQTTService* g_mqttService = nullptr;

static String escapeJson(const char* input) {
    if (!input) {
        return "";
    }

    String out;
    while (*input) {
        if (*input == '\\' || *input == '\"') {
            out += '\\';
        }
        out += *input;
        input++;
    }
    return out;
}

void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
    if (g_mqttService) {
        g_mqttService->onMessageReceived(topic, payload, length);
    }
}

MQTTService::MQTTService(const char* broker, int port)
    : client(nullptr), mqttBroker(broker), mqttPort(port), connected(false),
      lastPublishTime(0), publishInterval(PUBLISH_SENSOR_INTERVAL), lastDiscoveryTime(0),
      lastReconnectAttempt(0), reconnectDelay(MQTT_RECONNECT_INTERVAL), maxReconnectDelay(60000),
      relayCallback(nullptr), cropCallback(nullptr)
{
    memset(deviceId, 0, sizeof(deviceId));
    uint64_t chipId = ESP.getEfuseMac();
    snprintf(deviceId, sizeof(deviceId), "esp32-%04X%08X",
             (uint16_t)(chipId >> 32), (uint32_t)chipId);

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

    if (username) {
        strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
    }
    if (password) {
        strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);
    }

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
    if (!client) {
        return false;
    }

    if (client->connected()) {
        connected = true;
        return true;
    }

    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);

    bool ok = false;
    if (strlen(mqttUsername) > 0) {
        ok = client->connect(deviceId, mqttUsername, mqttPassword);
    } else {
        ok = client->connect(deviceId);
    }

    if (!ok) {
        connected = false;
        Serial.printf("[MQTTService] Connection failed, state=%d\n", client->state());
        return false;
    }

    connected = true;
    reconnectDelay = MQTT_RECONNECT_INTERVAL;
    lastReconnectAttempt = millis();

    Serial.println("[MQTTService] Connected");
    subscribeToTopics();
    publishStatus("online");
    publishCropList();
    publishDiscoveryMessages();

    return true;
}

bool MQTTService::isConnected() const
{
    return client && client->connected();
}

void MQTTService::loop()
{
    if (!client) {
        return;
    }

    unsigned long now = millis();
    if (!client->connected()) {
        connected = false;
        if (now - lastReconnectAttempt >= reconnectDelay) {
            lastReconnectAttempt = now;
            if (!connect()) {
                unsigned long nextDelay = reconnectDelay * 2;
                reconnectDelay = (nextDelay > maxReconnectDelay) ? maxReconnectDelay : nextDelay;
                Serial.printf("[MQTTService] Reconnect backoff: %lu ms\n", reconnectDelay);
            }
        }
        return;
    }

    connected = true;
    client->loop();
}

bool MQTTService::publish(const char* topic, const char* payload, bool retained)
{
    if (!client || !client->connected() || !topic || !payload) {
        return false;
    }

    String fullTopic = String("smartgarden/") + deviceId + "/" + topic;
    return client->publish(fullTopic.c_str(), payload, retained);
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

    char payload[32];
    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    publish("sensors/air_temperature", payload, false);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    publish("sensors/air_humidity", payload, false);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    publish("sensors/soil_moisture", payload, false);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    publish("sensors/soil_temperature", payload, false);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    publish("sensors/ph", payload, false);

    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    publish("sensors/ec", payload, false);

    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    publish("sensors/nitrogen", payload, false);

    snprintf(payload, sizeof(payload), "%u", snapshot.phosphorus);
    publish("sensors/phosphorus", payload, false);

    snprintf(payload, sizeof(payload), "%u", snapshot.potassium);
    publish("sensors/potassium", payload, false);

    return true;
}

bool MQTTService::publishRelayStatus(uint8_t relayIndex, bool state)
{
    if (!client || !client->connected() || relayIndex >= NUM_RELAYS) {
        return false;
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "relays/%u/state", relayIndex + 1);
    return publish(topic, state ? "ON" : "OFF");
}

bool MQTTService::publishAllRelayStatus(const RelayManager* relayMgr)
{
    if (!relayMgr) {
        return false;
    }

    bool ok = true;
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        ok = publishRelayStatus(i, relayMgr->getRelayState(i)) && ok;
    }
    return ok;
}

bool MQTTService::publishCropList()
{
    if (!client || !client->connected()) {
        return false;
    }

    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);

    String payload = "[";
    for (uint8_t i = 0; i < count; i++) {
        if (i > 0) {
            payload += ",";
        }
        payload += "\"";
        payload += escapeJson(crops[i].name);
        payload += "\"";
    }
    payload += "]";

    return publish("crop/available", payload.c_str());
}

bool MQTTService::publishCurrentCrop(const CropProfile* profile)
{
    if (!profile) {
        return false;
    }
    return publish("crop/current", profile->name);
}

bool MQTTService::publishStatus(const char* status)
{
    if (!status) {
        return false;
    }
    return publish("status", status);
}

bool MQTTService::publishUptime(unsigned long uptime)
{
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", uptime / 1000UL);
    return publish("uptime", payload, false);
}

void MQTTService::subscribeToTopics()
{
    if (!client) {
        return;
    }

    String baseTopic = String("smartgarden/") + deviceId + "/";
    client->subscribe((baseTopic + "relays/+/set").c_str());
    client->subscribe((baseTopic + "crop/select").c_str());

    Serial.printf("[MQTTService] Subscribed to %srelays/+/set and crop/select\n", baseTopic.c_str());
}

void MQTTService::handleRelayCommand(const char* relayName, const char* payload)
{
    if (!relayName || !payload) {
        return;
    }

    int relayNumber = atoi(relayName);
    if (relayNumber < 1 || relayNumber > NUM_RELAYS) {
        Serial.printf("[MQTTService] Invalid relay target: %s\n", relayName);
        return;
    }

    bool state = (strcmp(payload, "ON") == 0 || strcmp(payload, "1") == 0 || strcmp(payload, "true") == 0);

    if (relayCallback) {
        relayCallback((uint8_t)(relayNumber - 1), state);
    }
}

void MQTTService::handleCropSelect(const char* payload)
{
    if (cropCallback && payload && payload[0] != '\0') {
        cropCallback(payload);
    }
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length)
{
    if (!topic || !payload) {
        return;
    }

    char message[256];
    if (length >= sizeof(message)) {
        length = sizeof(message) - 1;
    }
    memcpy(message, payload, length);
    message[length] = '\0';

    String topicStr(topic);
    String baseTopic = String("smartgarden/") + deviceId + "/";

    if (!topicStr.startsWith(baseTopic)) {
        return;
    }

    String local = topicStr.substring(baseTopic.length());
    if (local == "crop/select") {
        handleCropSelect(message);
        return;
    }

    const String relayPrefix = "relays/";
    const String relaySuffix = "/set";
    if (local.startsWith(relayPrefix) && local.endsWith(relaySuffix)) {
        String relayName = local.substring(relayPrefix.length(), local.length() - relaySuffix.length());
        handleRelayCommand(relayName.c_str(), message);
    }
}

void MQTTService::publishDiscoveryMessages()
{
    if (!client || !client->connected()) {
        return;
    }

    String discoveryPrefix = "homeassistant";
    char deviceInfo[256];
    snprintf(deviceInfo, sizeof(deviceInfo),
             "\"identifiers\":[\"smartgarden_%s\"],\"manufacturer\":\"SmartGarden\",\"model\":\"ESP32\",\"name\":\"SmartGarden\",\"sw_version\":\"%s\"",
             deviceId, APP_VERSION);

    struct SensorEntity {
        const char* objectId;
        const char* name;
        const char* stateTopic;
        const char* unit;
        const char* deviceClass;
    };

    const SensorEntity sensors[] = {
        {"air_temperature", "Air Temperature", "sensors/air_temperature", "°C", "temperature"},
        {"air_humidity", "Air Humidity", "sensors/air_humidity", "%", "humidity"},
        {"soil_moisture", "Soil Moisture", "sensors/soil_moisture", "%", "moisture"},
        {"soil_temperature", "Soil Temperature", "sensors/soil_temperature", "°C", "temperature"},
        {"soil_ph", "Soil pH", "sensors/ph", "", ""},
        {"soil_ec", "Soil EC", "sensors/ec", "uS/cm", ""},
        {"soil_nitrogen", "Soil Nitrogen", "sensors/nitrogen", "mg/kg", ""},
        {"soil_phosphorus", "Soil Phosphorus", "sensors/phosphorus", "mg/kg", ""},
        {"soil_potassium", "Soil Potassium", "sensors/potassium", "mg/kg", ""}
    };

    for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++) {
        const SensorEntity& s = sensors[i];
        String configTopic = discoveryPrefix + "/sensor/smartgarden_" + deviceId + "_" + s.objectId + "/config";
        String stateTopic = String("smartgarden/") + deviceId + "/" + s.stateTopic;
        String uniqueId = String("smartgarden_") + deviceId + "_" + s.objectId;

        String payload = "{";
        payload += "\"name\":\"SmartGarden ";
        payload += s.name;
        payload += "\",";
        payload += "\"state_topic\":\"" + stateTopic + "\",";
        payload += "\"unique_id\":\"" + uniqueId + "\",";
        if (strlen(s.unit) > 0) {
            payload += "\"unit_of_measurement\":\"" + String(s.unit) + "\",";
        }
        if (strlen(s.deviceClass) > 0) {
            payload += "\"device_class\":\"" + String(s.deviceClass) + "\",";
        }
        payload += "\"state_class\":\"measurement\",";
        payload += "\"device\":{" + String(deviceInfo) + "}";
        payload += "}";

        client->publish(configTopic.c_str(), payload.c_str(), true);
    }

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        String relayId = String(i + 1);
        String configTopic = discoveryPrefix + "/switch/smartgarden_" + deviceId + "_relay_" + relayId + "/config";

        String payload = "{";
        payload += "\"name\":\"SmartGarden Relay ";
        payload += relayId;
        payload += "\",";
        payload += "\"state_topic\":\"smartgarden/" + String(deviceId) + "/relays/" + relayId + "/state\",";
        payload += "\"command_topic\":\"smartgarden/" + String(deviceId) + "/relays/" + relayId + "/set\",";
        payload += "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",";
        payload += "\"unique_id\":\"smartgarden_" + String(deviceId) + "_relay_" + relayId + "\",";
        payload += "\"device\":{" + String(deviceInfo) + "}";
        payload += "}";

        client->publish(configTopic.c_str(), payload.c_str(), true);
    }

    uint8_t cropCount = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(cropCount);

    String options = "[";
    for (uint8_t i = 0; i < cropCount; i++) {
        if (i > 0) {
            options += ",";
        }
        options += "\"" + escapeJson(crops[i].name) + "\"";
    }
    options += "]";

    String selectPayload = "{";
    selectPayload += "\"name\":\"SmartGarden Crop Profile\",";
    selectPayload += "\"command_topic\":\"smartgarden/" + String(deviceId) + "/crop/select\",";
    selectPayload += "\"state_topic\":\"smartgarden/" + String(deviceId) + "/crop/current\",";
    selectPayload += "\"options\":" + options + ",";
    selectPayload += "\"unique_id\":\"smartgarden_" + String(deviceId) + "_crop_profile\",";
    selectPayload += "\"device\":{" + String(deviceInfo) + "}";
    selectPayload += "}";

    String selectTopic = discoveryPrefix + "/select/smartgarden_" + deviceId + "_crop_profile/config";
    client->publish(selectTopic.c_str(), selectPayload.c_str(), true);

    lastDiscoveryTime = millis();
    Serial.println("[MQTT Discovery] ✓ Published all device configs");
}
