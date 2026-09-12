#include "mqtt_service.h"
#include "garden_profile.h"
#include <WiFi.h>
#include "app_config.h"

static MQTTService* g_mqttService = nullptr;

namespace {

constexpr uint8_t RELAY_COUNT = 8;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = MQTT_RECONNECT_INTERVAL;

const char* const RELAY_OBJECT_IDS[RELAY_COUNT] = {
    "fan",
    "heater",
    "cooler",
    "humidifier",
    "dehumidifier",
    "irrigation",
    "relay7",
    "relay8"
};

} // namespace

void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
    if (g_mqttService) {
        g_mqttService->onMessageReceived(topic, payload, length);
    }
}

MQTTService::MQTTService(const char* broker, int port)
    : client(wifiClient), mqttBroker(broker), mqttPort(port), connected(false),
      connectEventPending(false), lastReconnectAttempt(0),
      relayCallback(nullptr), cropCallback(nullptr),
      sensorManager(nullptr), relayManager(nullptr), currentCrop(nullptr)
{
    snprintf(deviceId, sizeof(deviceId), "smartgarden_%012llx", (unsigned long long)ESP.getEfuseMac());
    memset(mqttUsername, 0, sizeof(mqttUsername));
    memset(mqttPassword, 0, sizeof(mqttPassword));
    g_mqttService = this;
}

void MQTTService::attachStateProviders(const SensorManager* sensorMgr,
                                       const RelayManager* relayMgr,
                                       const CropProfile* const* crop)
{
    sensorManager = sensorMgr;
    relayManager = relayMgr;
    currentCrop = crop;
}

void MQTTService::begin(const char* username, const char* password)
{
    client.setServer(mqttBroker, mqttPort);
    client.setKeepAlive(60);
    client.setSocketTimeout(15);
    client.setBufferSize(1024);
    client.setCallback(mqttMessageCallback);

    strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
    strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);

    Serial.println("[MQTTService] Initialized");
}

void MQTTService::setRelayCommandCallback(RelayCommandCallback callback) { relayCallback = callback; }
void MQTTService::setCropSelectCallback(CropSelectCallback callback) { cropCallback = callback; }

bool MQTTService::connect()
{
    if (client.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) {
        connected = false;
        return false;
    }

    const unsigned long now = millis();
    if (lastReconnectAttempt != 0 && now - lastReconnectAttempt < MQTT_RETRY_INTERVAL_MS) {
        return false;
    }
    lastReconnectAttempt = now;

    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);

    if (client.connect(deviceId,
                       mqttUsername,
                       mqttPassword,
                       getStatusTopic().c_str(),
                       0,
                       true,
                       "offline")) {
        Serial.println("[MQTTService] Connected to MQTT broker");
        connected = true;
        connectEventPending = true;
        subscribeToTopics();
        publishRetainedState();
        return true;
    }

    Serial.printf("[MQTTService] Connection failed, code=%d\n", client.state());
    connected = false;
    return false;
}

bool MQTTService::isConnected() const { return client.connected(); }

void MQTTService::loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        connected = false;
        return;
    }

    if (client.connected()) {
        client.loop();
        return;
    }

    connect();
}

bool MQTTService::consumeConnectEvent()
{
    const bool result = connectEventPending;
    connectEventPending = false;
    return result;
}

bool MQTTService::publish(const char* topic, const char* payload, bool retained)
{
    return publishRaw((getBaseTopic() + "/" + topic).c_str(), payload, retained);
}

bool MQTTService::publishRaw(const char* topic, const char* payload, bool retained)
{
    if (!client.connected()) return false;
    return client.publish(topic, payload, retained);
}

String MQTTService::getBaseTopic() const
{
    return "smartgarden/" + String(deviceId);
}

String MQTTService::getStatusTopic() const
{
    return getBaseTopic() + "/status";
}

String MQTTService::getSensorTopic(const char* sensorName) const
{
    return getBaseTopic() + "/" + sensorName;
}

String MQTTService::getRelayStateTopic(uint8_t relayIndex) const
{
    return getBaseTopic() + "/" + getRelayObjectId(relayIndex) + "/state";
}

String MQTTService::getRelayCommandTopic(uint8_t relayIndex) const
{
    return getBaseTopic() + "/" + getRelayObjectId(relayIndex) + "/set";
}

String MQTTService::getCropCurrentTopic() const
{
    return getBaseTopic() + "/crop/current";
}

String MQTTService::getCropSelectTopic() const
{
    return getBaseTopic() + "/crop/select";
}

String MQTTService::getFirmwareTopic() const
{
    return getBaseTopic() + "/firmware";
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client.connected()) return false;

    char payload[32];

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    publishRaw(getSensorTopic("air_temperature").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    publishRaw(getSensorTopic("air_humidity").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    publishRaw(getSensorTopic("soil_moisture").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    publishRaw(getSensorTopic("soil_temperature").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    publishRaw(getSensorTopic("ph").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    publishRaw(getSensorTopic("ec").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    publishRaw(getSensorTopic("nitrogen").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.phosphorus);
    publishRaw(getSensorTopic("phosphorus").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.potassium);
    publishRaw(getSensorTopic("potassium").c_str(), payload, true);

    return true;
}

bool MQTTService::publishRelayStatus(uint8_t relayIndex, bool state)
{
    if (!client.connected()) return false;
    if (relayIndex >= RELAY_COUNT) return false;
    return publishRaw(getRelayStateTopic(relayIndex).c_str(), state ? "ON" : "OFF", true);
}

bool MQTTService::publishAllRelayStatus(const RelayManager* relayMgr)
{
    if (!relayMgr) return false;
    for (uint8_t i = 0; i < 8; i++) publishRelayStatus(i, relayMgr->getRelayState(i));
    return true;
}

bool MQTTService::publishCropList()
{
    if (!client.connected()) return false;

    CropProfileStore::initialize();
    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);

    char payload[1024];
    strcpy(payload, "[");
    for (uint8_t i = 0; i < count; i++) {
        if (i > 0) strcat(payload, ",");
        char crop[128];
        snprintf(crop, sizeof(crop), "\"%s\"", crops[i].name);
        strcat(payload, crop);
    }
    strcat(payload, "]");

    return publish("crop/available", payload, true);
}

bool MQTTService::publishCurrentCrop(const CropProfile* profile)
{
    if (!client.connected() || !profile) return false;
    return publishRaw(getCropCurrentTopic().c_str(), profile->name, true);
}

bool MQTTService::publishStatus(const char* status)
{
    if (!client.connected()) return false;
    return publishRaw(getStatusTopic().c_str(), status, true);
}

bool MQTTService::publishUptime(unsigned long uptime)
{
    if (!client.connected()) return false;
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", uptime / 1000);
    return publish("uptime", payload, true);
}

bool MQTTService::publishRetainedState()
{
    if (!client.connected()) return false;

    publishStatus("online");
    publishRaw(getFirmwareTopic().c_str(), APP_VERSION, true);
    publishUptime(millis());
    publishCropList();

    if (sensorManager != nullptr) {
        publishSensorData(sensorManager->getSnapshot());
    }
    if (relayManager != nullptr) {
        publishAllRelayStatus(relayManager);
    }
    if (currentCrop != nullptr && *currentCrop != nullptr) {
        publishCurrentCrop(*currentCrop);
    }

    return true;
}

void MQTTService::subscribeToTopics()
{
    client.subscribe(getRelayCommandTopic(0).c_str());
    client.subscribe(getRelayCommandTopic(1).c_str());
    client.subscribe(getRelayCommandTopic(2).c_str());
    client.subscribe(getRelayCommandTopic(3).c_str());
    client.subscribe(getRelayCommandTopic(4).c_str());
    client.subscribe(getRelayCommandTopic(5).c_str());
    client.subscribe(getRelayCommandTopic(6).c_str());
    client.subscribe(getRelayCommandTopic(7).c_str());
    client.subscribe(getCropSelectTopic().c_str());
}

void MQTTService::handleRelayCommand(const char* relayName, const char* payload)
{
    uint8_t relayIndex = 0xFF;
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        if (strcmp(relayName, RELAY_OBJECT_IDS[i]) == 0) {
            relayIndex = i;
            break;
        }
    }
    if (relayIndex == 0xFF) return;
    bool state = (strcmp(payload, "ON") == 0 || strcmp(payload, "1") == 0);
    if (relayCallback) relayCallback(relayIndex, state);
}

void MQTTService::handleCropSelect(const char* payload)
{
    if (cropCallback) cropCallback(payload);
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length)
{
    char message[128];
    if (length >= sizeof(message)) length = sizeof(message) - 1;
    memcpy(message, payload, length);
    message[length] = '\0';

    String topicStr(topic);
    message[sizeof(message) - 1] = '\0';

    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        if (topicStr == getRelayCommandTopic(i)) {
            handleRelayCommand(getRelayObjectId(i), message);
            return;
        }
    }

    if (topicStr == getCropSelectTopic()) {
        handleCropSelect(message);
    }
}

const char* MQTTService::getRelayObjectId(uint8_t relayIndex) const
{
    if (relayIndex >= RELAY_COUNT) {
        return "relay";
    }
    return RELAY_OBJECT_IDS[relayIndex];
}
