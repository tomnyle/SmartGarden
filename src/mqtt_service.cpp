#include "mqtt_service.h"
#include "garden_profile.h"
#include <WiFi.h>

static MQTTService* g_mqttService = nullptr;

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

void MQTTService::setRelayCommandCallback(RelayCommandCallback callback) { relayCallback = callback; }
void MQTTService::setCropSelectCallback(CropSelectCallback callback) { cropCallback = callback; }

bool MQTTService::connect()
{
    if (!client) return false;
    if (client->connected()) return true;

    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);

    if (client->connect(deviceId, mqttUsername, mqttPassword)) {
        Serial.println("[MQTTService] Connected to MQTT broker");
        connected = true;
        publishStatus("online");
        subscribeToTopics();
        publishCropList();
        publishDiscoveryMessages();
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
    if (!client->connected()) {
        if (millis() % 5000 == 0) connect();
    } else {
        client->loop();
    }
}

bool MQTTService::publish(const char* topic, const char* payload)
{
    if (!client || !client->connected()) return false;
    String fullTopic = String("smartgarden/") + String(deviceId) + "/" + String(topic);
    return client->publish(fullTopic.c_str(), payload, true);
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client || !client->connected()) return false;

    char payload[64];
    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    client->publish((String("smartgarden/") + deviceId + "/sensors/air_temp").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    client->publish((String("smartgarden/") + deviceId + "/sensors/air_humidity").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    client->publish((String("smartgarden/") + deviceId + "/sensors/soil_moisture").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    client->publish((String("smartgarden/") + deviceId + "/sensors/soil_temp").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    client->publish((String("smartgarden/") + deviceId + "/sensors/ph").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    client->publish((String("smartgarden/") + deviceId + "/sensors/ec").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    client->publish((String("smartgarden/") + deviceId + "/sensors/nitrogen").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.phosphorus);
    client->publish((String("smartgarden/") + deviceId + "/sensors/phosphorus").c_str(), payload, true);

    snprintf(payload, sizeof(payload), "%u", snapshot.potassium);
    client->publish((String("smartgarden/") + deviceId + "/sensors/potassium").c_str(), payload, true);

    return true;
}

bool MQTTService::publishRelayStatus(uint8_t relayIndex, bool state)
{
    if (!client || !client->connected()) return false;

    const char* relayNames[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};
    if (relayIndex >= 8) return false;

    String topic = String("smartgarden/") + deviceId + "/relays/" + relayNames[relayIndex];
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

    String topic = String("smartgarden/") + deviceId + "/crop/available";
    return client->publish(topic.c_str(), payload, true);
}

bool MQTTService::publishCurrentCrop(const CropProfile* profile)
{
    if (!client || !client->connected() || !profile) return false;
    String topic = String("smartgarden/") + deviceId + "/crop/current";
    return client->publish(topic.c_str(), profile->name, true);
}

bool MQTTService::publishStatus(const char* status)
{
    if (!client || !client->connected()) return false;
    String topic = String("smartgarden/") + deviceId + "/status";
    return client->publish(topic.c_str(), status, true);
}

bool MQTTService::publishUptime(unsigned long uptime)
{
    if (!client || !client->connected()) return false;
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", uptime / 1000);
    String topic = String("smartgarden/") + deviceId + "/uptime";
    return client->publish(topic.c_str(), payload, true);
}

void MQTTService::subscribeToTopics()
{
    if (!client) return;
    String baseTopic = String("smartgarden/") + deviceId + "/";
    client->subscribe((baseTopic + "relays/fan/set").c_str());
    client->subscribe((baseTopic + "relays/heater/set").c_str());
    client->subscribe((baseTopic + "relays/cooler/set").c_str());
    client->subscribe((baseTopic + "relays/humidifier/set").c_str());
    client->subscribe((baseTopic + "relays/dehumidifier/set").c_str());
    client->subscribe((baseTopic + "relays/irrigation/set").c_str());
    client->subscribe((baseTopic + "relays/relay7/set").c_str());
    client->subscribe((baseTopic + "relays/relay8/set").c_str());
    client->subscribe((baseTopic + "crop/select").c_str());
}

void MQTTService::handleRelayCommand(const char* relayName, const char* payload)
{
    const char* relayNames[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};
    uint8_t relayIndex = 0xFF;
    for (uint8_t i = 0; i < 8; i++) if (strcmp(relayName, relayNames[i]) == 0) { relayIndex = i; break; }
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
    char message[256];
    if (length >= sizeof(message)) length = sizeof(message) - 1;
    strncpy(message, (char*)payload, length);
    message[length] = '\0';

    String topicStr(topic);
    if (topicStr.indexOf("/relays/") > 0) {
        if (topicStr.indexOf("/fan/set") > 0) handleRelayCommand("fan", message);
        else if (topicStr.indexOf("/heater/set") > 0) handleRelayCommand("heater", message);
        else if (topicStr.indexOf("/cooler/set") > 0) handleRelayCommand("cooler", message);
        else if (topicStr.indexOf("/humidifier/set") > 0) handleRelayCommand("humidifier", message);
        else if (topicStr.indexOf("/dehumidifier/set") > 0) handleRelayCommand("dehumidifier", message);
        else if (topicStr.indexOf("/irrigation/set") > 0) handleRelayCommand("irrigation", message);
        else if (topicStr.indexOf("/relay7/set") > 0) handleRelayCommand("relay7", message);
        else if (topicStr.indexOf("/relay8/set") > 0) handleRelayCommand("relay8", message);
    } else if (topicStr.indexOf("/crop/select") > 0) {
        handleCropSelect(message);
    }
}

void MQTTService::publishDiscoveryMessages()
{
    if (!client || !client->connected()) return;

    const char* deviceInfo = R"({"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32 SmartGarden Controller","name":"Smart Garden"})";
    char buffer[1024];
    auto pub = [&](const char* topic, const char* json){ client->publish(topic, json, true); };

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Temperature\",\"object_id\":\"smartgarden_air_temp\",\"unique_id\":\"smartgarden_air_temp\",\"state_topic\":\"smartgarden/sensors/air_temp\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_air_temp/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Humidity\",\"object_id\":\"smartgarden_air_humidity\",\"unique_id\":\"smartgarden_air_humidity\",\"state_topic\":\"smartgarden/sensors/air_humidity\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_air_humidity/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Moisture\",\"object_id\":\"smartgarden_soil_moisture\",\"unique_id\":\"smartgarden_soil_moisture\",\"state_topic\":\"smartgarden/sensors/soil_moisture\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device_class\":\"moisture\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_soil_moisture/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Temperature\",\"object_id\":\"smartgarden_soil_temp\",\"unique_id\":\"smartgarden_soil_temp\",\"state_topic\":\"smartgarden/sensors/soil_temp\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_soil_temp/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"pH Value\",\"object_id\":\"smartgarden_ph\",\"unique_id\":\"smartgarden_ph\",\"state_topic\":\"smartgarden/sensors/ph\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"state_class\":\"measurement\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_ph/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"EC\",\"object_id\":\"smartgarden_ec\",\"unique_id\":\"smartgarden_ec\",\"state_topic\":\"smartgarden/sensors/ec\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"uS/cm\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_ec/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Nitrogen\",\"object_id\":\"smartgarden_nitrogen\",\"unique_id\":\"smartgarden_nitrogen\",\"state_topic\":\"smartgarden/sensors/nitrogen\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_nitrogen/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Phosphorus\",\"object_id\":\"smartgarden_phosphorus\",\"unique_id\":\"smartgarden_phosphorus\",\"state_topic\":\"smartgarden/sensors/phosphorus\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_phosphorus/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Potassium\",\"object_id\":\"smartgarden_potassium\",\"unique_id\":\"smartgarden_potassium\",\"state_topic\":\"smartgarden/sensors/potassium\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"device\":%s}",
        deviceInfo);
    pub("homeassistant/sensor/smartgarden_potassium/config", buffer);

    const char* relayNames[] = {"Circulation Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation", "Relay 7", "Relay 8"};
    const char* relayIds[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};
    for (uint8_t i = 0; i < 8; i++) {
        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"object_id\":\"smartgarden_%s\",\"unique_id\":\"smartgarden_%s\",\"state_topic\":\"smartgarden/relay/%u/state\",\"command_topic\":\"smartgarden/relay/%u/set\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device\":%s}",
            relayNames[i], relayIds[i], relayIds[i], i + 1, i + 1, deviceInfo);
        char topic[128];
        snprintf(topic, sizeof(topic), "homeassistant/switch/smartgarden_%s/config", relayIds[i]);
        pub(topic, buffer);
    }

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Crop Profile\",\"object_id\":\"smartgarden_crop\",\"unique_id\":\"smartgarden_crop\",\"command_topic\":\"smartgarden/%s/crop/select\",\"state_topic\":\"smartgarden/%s/crop/current\",\"availability_topic\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"options\":[\"Sâm\",\"Cà chua\",\"Dâu tây\",\"Rau mầm\",\"Cải kale\",\"Bánh chua\",\"Thơm\",\"Xà lách\",\"Ớt\",\"Cúc hoa mi\",\"Chanh\",\"Bạc hà\",\"Tỏi\"],\"device\":%s}",
        deviceId, deviceId, deviceInfo);
    pub("homeassistant/select/smartgarden_crop/config", buffer);

    client->publish("homeassistant/binary_sensor/smartgarden_status/config",
        "{\"name\":\"System Online\",\"object_id\":\"smartgarden_status\",\"unique_id\":\"smartgarden_status\",\"state_topic\":\"smartgarden/status\",\"payload_on\":\"online\",\"payload_off\":\"offline\",\"device_class\":\"connectivity\",\"device\":{\"identifiers\":[\"smartgarden_esp32\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32 SmartGarden Controller\",\"name\":\"Smart Garden\"}}",
        true);
}
