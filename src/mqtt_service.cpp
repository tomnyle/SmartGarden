#include "mqtt_service.h"
#include "garden_profile.h"
#include "app_config.h"
#include <WiFi.h>

static MQTTService* g_mqttService = nullptr;

namespace {
constexpr char SMARTGARDEN_DEVICE_ID[] = "smartgarden";
constexpr char SMARTGARDEN_DEVICE_NAME[] = "SmartGarden";
constexpr char SMARTGARDEN_STATUS_TOPIC[] = "smartgarden/status";

const char* const RELAY_TOPIC_IDS[] = {
    "relay_1", "relay_2", "relay_3", "relay_4",
    "relay_5", "relay_6", "relay_7", "relay_8"
};

const char* const RELAY_NAMES[] = {
    "Fan", "Heater", "Cooler", "Humidifier",
    "Dehumidifier", "Irrigation", "Relay 7", "Relay 8"
};

const char* const CROP_OPTIONS_JSON =
    "[\"Sâm\",\"Cà chua\",\"Dâu tây\",\"Rau mầm\",\"Cải kale\",\"Bánh chua\",\"Thơm\",\"Xà lách\",\"Ớt\",\"Cúc hoa mi\",\"Chanh\",\"Bạc hà\",\"Tỏi\"]";
}

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
    strncpy(deviceId, SMARTGARDEN_DEVICE_ID, sizeof(deviceId) - 1);
    deviceId[sizeof(deviceId) - 1] = '\0';
    snprintf(mqttClientId, sizeof(mqttClientId), "smartgarden-%llx", (unsigned long long)ESP.getEfuseMac());
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
    mqttUsername[sizeof(mqttUsername) - 1] = '\0';
    mqttPassword[sizeof(mqttPassword) - 1] = '\0';

    Serial.println("[MQTTService] Initialized");
}

void MQTTService::setRelayCommandCallback(RelayCommandCallback callback) { relayCallback = callback; }
void MQTTService::setCropSelectCallback(CropSelectCallback callback) { cropCallback = callback; }

bool MQTTService::connect()
{
    if (!client) return false;
    if (client->connected()) return true;

    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);

    if (client->connect(mqttClientId, mqttUsername, mqttPassword, SMARTGARDEN_STATUS_TOPIC, 0, true, "offline")) {
        Serial.printf("[MQTTService] Connected to MQTT broker as %s\n", mqttClientId);
        connected = true;
        Serial.printf("[MQTTService] Publishing retained availability: %s=online\n", SMARTGARDEN_STATUS_TOPIC);
        publishStatus("online");
        subscribeToTopics();
        Serial.println("[MQTTService] Publishing Home Assistant discovery...");
        publishDiscoveryMessages();
        publishCropList();
        return true;
    }

    Serial.printf("[MQTTService] Connection failed, code=%d\n", client->state());
    connected = false;
    return false;
}

bool MQTTService::isConnected() const { return client && client->connected(); }

void MQTTService::loop()
{
    if (!client) return;
    if (client->connected()) client->loop();
    else if (millis() - lastDiscoveryTime > 5000) { lastDiscoveryTime = millis(); connect(); }
}

bool MQTTService::publish(const char* topic, const char* payload)
{
    if (!client || !client->connected()) return false;
    String fullTopic = String("smartgarden/") + String(topic);
    return client->publish(fullTopic.c_str(), payload, true);
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client || !client->connected()) return false;

    char payload[32];

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    client->publish("smartgarden/sensors/air_temp", payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    client->publish("smartgarden/sensors/air_humidity", payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    client->publish("smartgarden/sensors/soil_moisture", payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    client->publish("smartgarden/sensors/soil_temp", payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    client->publish("smartgarden/sensors/ph", payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    client->publish("smartgarden/sensors/ec", payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    client->publish("smartgarden/sensors/nitrogen", payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.phosphorus);
    client->publish("smartgarden/sensors/phosphorus", payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.potassium);
    client->publish("smartgarden/sensors/potassium", payload, true);

    return true;
}

bool MQTTService::publishRelayStatus(uint8_t relayIndex, bool state)
{
    if (!client || !client->connected()) return false;
    if (relayIndex >= 8) return false;
    String topic = String("smartgarden/relay/") + String(relayIndex + 1) + "/state";
    return client->publish(topic.c_str(), state ? "ON" : "OFF", true);
}

bool MQTTService::publishAllRelayStatus(const RelayManager* relayMgr)
{
    if (!relayMgr) return false;
    for (uint8_t i = 0; i < 8; i++) publishRelayStatus(i, relayMgr->getRelayState(i));
    return true;
}

bool MQTTService::publishCropList()
{
    if (!client || !client->connected()) return false;

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

    return client->publish("smartgarden/crop/available", payload, true);
}

bool MQTTService::publishCurrentCrop(const CropProfile* profile)
{
    if (!client || !client->connected() || !profile) return false;
    return client->publish("smartgarden/crop/current", profile->name, true);
}

bool MQTTService::publishStatus(const char* status)
{
    if (!client || !client->connected()) return false;
    return client->publish(SMARTGARDEN_STATUS_TOPIC, status, true);
}

bool MQTTService::publishUptime(unsigned long uptime)
{
    if (!client || !client->connected()) return false;
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", uptime / 1000);
    return client->publish("smartgarden/uptime", payload, true);
}

void MQTTService::subscribeToTopics()
{
    if (!client) return;
    for (uint8_t i = 0; i < 8; i++) {
        String relayTopic = String("smartgarden/relay/") + String(i + 1) + "/set";
        client->subscribe(relayTopic.c_str());
    }
    client->subscribe("smartgarden/crop/select");
}

void MQTTService::handleRelayCommand(uint8_t relayIndex, const char* payload)
{
    if (relayIndex >= 8) return;
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
    strncpy(message, (char*)payload, length);
    message[length] = '\0';

    String topicStr(topic);
    Serial.printf("[MQTTService] RX %s = %s\n", topic, message);

    if (topicStr == "smartgarden/crop/select") {
        handleCropSelect(message);
        return;
    }

    for (uint8_t i = 0; i < 8; i++) {
        String relayTopic = String("smartgarden/relay/") + String(i + 1) + "/set";
        if (topicStr == relayTopic) {
            handleRelayCommand(i, message);
            return;
        }
    }
}

void MQTTService::publishDiscoveryMessages()
{
    if (!client || !client->connected()) return;

    char topic[160];
    char payload[1024];
    char deviceInfo[256];
    auto pub = [&](const char* t, const char* p){ client->publish(t, p, true); };

    snprintf(deviceInfo, sizeof(deviceInfo),
        "{\"identifiers\":[\"%s\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32 SmartGarden Controller\",\"name\":\"%s\",\"sw_version\":\"%s\"}",
        deviceId, SMARTGARDEN_DEVICE_NAME, APP_VERSION);

    snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/smartgarden/status/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Status\",\"object_id\":\"smartgarden_status\",\"unique_id\":\"smartgarden_status\",\"state_topic\":\"smartgarden/status\",\"payload_on\":\"online\",\"payload_off\":\"offline\",\"device_class\":\"connectivity\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/air_temp/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Air Temperature\",\"object_id\":\"smartgarden_air_temp\",\"unique_id\":\"smartgarden_air_temp\",\"state_topic\":\"smartgarden/sensors/air_temp\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/air_humidity/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Air Humidity\",\"object_id\":\"smartgarden_air_humidity\",\"unique_id\":\"smartgarden_air_humidity\",\"state_topic\":\"smartgarden/sensors/air_humidity\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/soil_moisture/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Soil Moisture\",\"object_id\":\"smartgarden_soil_moisture\",\"unique_id\":\"smartgarden_soil_moisture\",\"state_topic\":\"smartgarden/sensors/soil_moisture\",\"device_class\":\"moisture\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/soil_temp/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Soil Temperature\",\"object_id\":\"smartgarden_soil_temp\",\"unique_id\":\"smartgarden_soil_temp\",\"state_topic\":\"smartgarden/sensors/soil_temp\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/ph/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"pH Value\",\"object_id\":\"smartgarden_ph\",\"unique_id\":\"smartgarden_ph\",\"state_topic\":\"smartgarden/sensors/ph\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/ec/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"EC\",\"object_id\":\"smartgarden_ec\",\"unique_id\":\"smartgarden_ec\",\"state_topic\":\"smartgarden/sensors/ec\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"uS/cm\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/nitrogen/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Nitrogen\",\"object_id\":\"smartgarden_nitrogen\",\"unique_id\":\"smartgarden_nitrogen\",\"state_topic\":\"smartgarden/sensors/nitrogen\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/phosphorus/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Phosphorus\",\"object_id\":\"smartgarden_phosphorus\",\"unique_id\":\"smartgarden_phosphorus\",\"state_topic\":\"smartgarden/sensors/phosphorus\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/smartgarden/potassium/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Potassium\",\"object_id\":\"smartgarden_potassium\",\"unique_id\":\"smartgarden_potassium\",\"state_topic\":\"smartgarden/sensors/potassium\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        deviceInfo);
    pub(topic, payload);

    for (uint8_t i = 0; i < 8; i++) {
        snprintf(topic, sizeof(topic), "homeassistant/switch/smartgarden/%s/config", RELAY_TOPIC_IDS[i]);
        snprintf(payload, sizeof(payload),
            "{\"name\":\"%s\",\"object_id\":\"smartgarden_%s\",\"unique_id\":\"smartgarden_%s\",\"state_topic\":\"smartgarden/relay/%u/state\",\"command_topic\":\"smartgarden/relay/%u/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
            RELAY_NAMES[i], RELAY_TOPIC_IDS[i], RELAY_TOPIC_IDS[i], i + 1, i + 1, deviceInfo);
        pub(topic, payload);
    }

    snprintf(topic, sizeof(topic), "homeassistant/select/smartgarden/crop_profile/config");
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Crop Profile\",\"object_id\":\"smartgarden_crop_profile\",\"unique_id\":\"smartgarden_crop_profile\",\"command_topic\":\"smartgarden/crop/select\",\"state_topic\":\"smartgarden/crop/current\",\"options\":%s,\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device\":%s}",
        CROP_OPTIONS_JSON, deviceInfo);
    pub(topic, payload);

    Serial.println("[MQTTService] Discovery messages published");
}
