#include "mqtt_service.h"
#include "garden_profile.h"
#include "app_config.h"
#include <WiFi.h>
#include <ArduinoJson.h>

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
    snprintf(deviceId, sizeof(deviceId), "SmartGarden_%llu", (unsigned long long)ESP.getEfuseMac());
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
    client->setBufferSize(MQTT_BUFFER_SIZE);

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

    if (client->connect(deviceId, mqttUsername, mqttPassword, MQTT_TOPIC_AVAILABILITY, 0, true, "offline")) {
        Serial.println("[MQTTService] Connected to MQTT broker");
        connected = true;
        client->publish(MQTT_TOPIC_AVAILABILITY, "online", true);
        publishStatus("online");
        subscribeToTopics();
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
    else if (millis() - lastDiscoveryTime > MQTT_RECONNECT_INTERVAL) { 
        lastDiscoveryTime = millis(); 
        connect(); 
    }
}

bool MQTTService::publish(const char* topic, const char* payload)
{
    if (!client || !client->connected()) return false;
    return client->publish(topic, payload, true);
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client || !client->connected()) return false;

    char payload[32];

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    client->publish(MQTT_TOPIC_AIR_TEMP, payload, true);
    Serial.printf("[MQTT] Air Temp: %s °C\n", payload);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    client->publish(MQTT_TOPIC_AIR_HUMIDITY, payload, true);
    Serial.printf("[MQTT] Air Humidity: %s %%\n", payload);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    client->publish(MQTT_TOPIC_SOIL_MOISTURE, payload, true);
    Serial.printf("[MQTT] Soil Moisture: %s\n", payload);

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    client->publish(MQTT_TOPIC_SOIL_TEMP, payload, true);
    Serial.printf("[MQTT] Soil Temp: %s °C\n", payload);

    snprintf(payload, sizeof(payload), "%.2f", snapshot.ph);
    client->publish(MQTT_TOPIC_PH, payload, true);
    Serial.printf("[MQTT] pH: %s\n", payload);

    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    client->publish(MQTT_TOPIC_EC, payload, true);
    Serial.printf("[MQTT] EC: %s µS/cm\n", payload);

    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    client->publish(MQTT_TOPIC_NITROGEN, payload, true);
    Serial.printf("[MQTT] Nitrogen: %s mg/kg\n", payload);

    snprintf(payload, sizeof(payload), "%u", snapshot.phosphorus);
    client->publish(MQTT_TOPIC_PHOSPHORUS, payload, true);
    Serial.printf("[MQTT] Phosphorus: %s mg/kg\n", payload);

    snprintf(payload, sizeof(payload), "%u", snapshot.potassium);
    client->publish(MQTT_TOPIC_POTASSIUM, payload, true);
    Serial.printf("[MQTT] Potassium: %s mg/kg\n", payload);

    Serial.println("[MQTT] Sensor data published successfully");
    return true;
}

bool MQTTService::publishRelayStatus(uint8_t relayIndex, bool state)
{
    if (!client || !client->connected()) return false;

    if (relayIndex >= NUM_RELAYS) return false;

    char relayTopic[64];
    snprintf(relayTopic, sizeof(relayTopic), MQTT_RUNTIME_PREFIX "/relay/%u/state", relayIndex + 1);

    bool success = client->publish(relayTopic, state ? "ON" : "OFF", true);
    if (success) {
        Serial.printf("[MQTT] Relay %u: %s\n", relayIndex + 1, state ? "ON" : "OFF");
    }
    return success;
}

bool MQTTService::publishAllRelayStatus(const RelayManager* relayMgr)
{
    if (!relayMgr) return false;
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        publishRelayStatus(i, relayMgr->getRelayState(i));
    }
    return true;
}

bool MQTTService::publishCropList()
{
    if (!client || !client->connected()) return false;

    CropProfileStore::initialize();
    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);

    StaticJsonDocument<1024> doc;
    JsonArray options = doc.createNestedArray("options");
    
    for (uint8_t i = 0; i < count; i++) {
        options.add(crops[i].name);
    }

    String payload;
    serializeJson(doc, payload);

    return client->publish(
        MQTT_TOPIC_CROP_LIST,
        payload.c_str(), 
        true
    );
}

bool MQTTService::publishCurrentCrop(const CropProfile* profile)
{
    if (!client || !client->connected() || !profile) return false;
    return client->publish(
        MQTT_TOPIC_CROP_STATE,
        profile->name, 
        true
    );
}

bool MQTTService::publishStatus(const char* status)
{
    if (!client || !client->connected()) return false;
    bool success = client->publish(MQTT_TOPIC_STATUS, status, true);
    Serial.printf("[MQTT] Status: %s\n", status);
    return success;
}

bool MQTTService::publishUptime(unsigned long uptime)
{
    if (!client || !client->connected()) return false;
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", uptime / 1000);
    return client->publish(MQTT_TOPIC_UPTIME, payload, true);
}

void MQTTService::subscribeToTopics()
{
    if (!client) return;

    for (uint8_t i = 1; i <= NUM_RELAYS; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), MQTT_RUNTIME_PREFIX "/relay/%u/set", i);
        client->subscribe(topic);
    }

    // Subscribe to crop select topic
    client->subscribe(MQTT_TOPIC_CROP_SELECT);
    client->subscribe(MQTT_TOPIC_MODE_SET);
    
    Serial.println("[MQTT] Subscribed to all control topics");
}

void MQTTService::handleRelayCommand(uint8_t relayIndex, const char* payload)
{
    bool state = (strcmp(payload, "ON") == 0 || strcmp(payload, "1") == 0);

    Serial.printf("[MQTT] Command received: relay %u = %s\n", relayIndex + 1, state ? "ON" : "OFF");
    
    if (relayCallback) relayCallback(relayIndex, state);
}

void MQTTService::handleCropSelect(const char* payload)
{
    Serial.printf("[MQTT] Crop select: %s\n", payload);
    if (cropCallback) cropCallback(payload);
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length)
{
    char message[128];
    if (length >= sizeof(message)) length = sizeof(message) - 1;
    strncpy(message, (char*)payload, length);
    message[length] = '\0';

    String topicStr(topic);
    
    Serial.printf("[MQTT] Message received on: %s = %s\n", topic, message);

    if (topicStr.startsWith(MQTT_RUNTIME_PREFIX "/relay/") && topicStr.endsWith("/set")) {
        int relayIndex = topicStr.substring(String(MQTT_RUNTIME_PREFIX "/relay/").length(), topicStr.lastIndexOf('/')).toInt() - 1;
        if (relayIndex >= 0 && relayIndex < NUM_RELAYS) {
            handleRelayCommand(static_cast<uint8_t>(relayIndex), message);
        }
    } else if (topicStr == MQTT_TOPIC_CROP_SELECT) {
        handleCropSelect(message);
    }
}

void MQTTService::publishDiscoveryMessages()
{
    if (!client || !client->connected()) return;

    Serial.println("[HA Discovery] Publishing entity discoveries...");
    delay(100);

    char topic[256];
    char payload[2048];

    // ==================== SENSORS ====================
    
    // Air Temperature
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Air Temperature";
        doc["unique_id"] = "smartgarden_air_temp";
        doc["state_topic"] = MQTT_TOPIC_AIR_TEMP;
        doc["unit_of_measurement"] = "°C";
        doc["device_class"] = "temperature";
        doc["icon"] = "mdi:thermometer";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_air_temp/config").c_str(), payload, true);
    }
    delay(50);

    // Air Humidity
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Air Humidity";
        doc["unique_id"] = "smartgarden_air_humidity";
        doc["state_topic"] = MQTT_TOPIC_AIR_HUMIDITY;
        doc["unit_of_measurement"] = "%";
        doc["device_class"] = "humidity";
        doc["icon"] = "mdi:water-percent";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_air_humidity/config").c_str(), payload, true);
    }
    delay(50);

    // Soil Moisture
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Soil Moisture";
        doc["unique_id"] = "smartgarden_soil_moisture";
        doc["state_topic"] = MQTT_TOPIC_SOIL_MOISTURE;
        doc["unit_of_measurement"] = "%";
        doc["icon"] = "mdi:water";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_soil_moisture/config").c_str(), payload, true);
    }
    delay(50);

    // Soil Temperature
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Soil Temperature";
        doc["unique_id"] = "smartgarden_soil_temp";
        doc["state_topic"] = MQTT_TOPIC_SOIL_TEMP;
        doc["unit_of_measurement"] = "°C";
        doc["device_class"] = "temperature";
        doc["icon"] = "mdi:thermometer";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_soil_temp/config").c_str(), payload, true);
    }
    delay(50);

    // pH
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "pH Value";
        doc["unique_id"] = "smartgarden_ph";
        doc["state_topic"] = MQTT_TOPIC_PH;
        doc["unit_of_measurement"] = "pH";
        doc["icon"] = "mdi:test-tube";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_ph/config").c_str(), payload, true);
    }
    delay(50);

    // EC
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Electrical Conductivity";
        doc["unique_id"] = "smartgarden_ec";
        doc["state_topic"] = MQTT_TOPIC_EC;
        doc["unit_of_measurement"] = "µS/cm";
        doc["icon"] = "mdi:flash";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_ec/config").c_str(), payload, true);
    }
    delay(50);

    // Nitrogen
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Nitrogen";
        doc["unique_id"] = "smartgarden_nitrogen";
        doc["state_topic"] = MQTT_TOPIC_NITROGEN;
        doc["unit_of_measurement"] = "mg/kg";
        doc["icon"] = "mdi:leaf";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_nitrogen/config").c_str(), payload, true);
    }
    delay(50);

    // Phosphorus
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Phosphorus";
        doc["unique_id"] = "smartgarden_phosphorus";
        doc["state_topic"] = MQTT_TOPIC_PHOSPHORUS;
        doc["unit_of_measurement"] = "mg/kg";
        doc["icon"] = "mdi:leaf";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_phosphorus/config").c_str(), payload, true);
    }
    delay(50);

    // Potassium
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Potassium";
        doc["unique_id"] = "smartgarden_potassium";
        doc["state_topic"] = MQTT_TOPIC_POTASSIUM;
        doc["unit_of_measurement"] = "mg/kg";
        doc["icon"] = "mdi:leaf";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/sensor/smartgarden_potassium/config").c_str(), payload, true);
    }
    delay(50);

    // ==================== SWITCHES (RELAYS) ====================
    
    const char* relayNames[] = {"Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation", "Relay 7", "Relay 8"};
    const char* relayIds[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};
    const char* relayIcons[] = {"mdi:fan", "mdi:radiator", "mdi:snowflake", "mdi:water-percent", "mdi:water-off", "mdi:water-pump", "mdi:toggle-switch", "mdi:toggle-switch"};

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        char stateTopic[64];
        char commandTopic[64];
        snprintf(stateTopic, sizeof(stateTopic), MQTT_RUNTIME_PREFIX "/relay/%u/state", i + 1);
        snprintf(commandTopic, sizeof(commandTopic), MQTT_RUNTIME_PREFIX "/relay/%u/set", i + 1);

        StaticJsonDocument<512> doc;
        doc["name"] = relayNames[i];
        doc["unique_id"] = String("smartgarden_") + relayIds[i];
        doc["state_topic"] = stateTopic;
        doc["command_topic"] = commandTopic;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["state_on"] = "ON";
        doc["state_off"] = "OFF";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        doc["icon"] = relayIcons[i];
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        snprintf(topic, sizeof(topic), "%s/switch/smartgarden_%s/config", HA_DISCOVERY_PREFIX, relayIds[i]);
        client->publish(topic, payload, true);
        delay(50);
    }

    // ==================== CROP SELECT ====================
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Crop Profile";
        doc["unique_id"] = "smartgarden_crop";
        doc["command_topic"] = MQTT_TOPIC_CROP_SELECT;
        doc["state_topic"] = MQTT_TOPIC_CROP_STATE;
        doc["icon"] = "mdi:leaf";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        JsonArray options = doc.createNestedArray("options");
        options.add("Tomato");
        options.add("Lettuce");
        options.add("Pepper");
        options.add("Cucumber");
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;
        
        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/select/smartgarden_crop/config").c_str(), payload, true);
    }
    delay(50);

    // ==================== OPERATION MODE ====================
    {
        StaticJsonDocument<512> doc;
        doc["name"] = "Operation Mode";
        doc["unique_id"] = "smartgarden_operation_mode";
        doc["command_topic"] = MQTT_TOPIC_MODE_SET;
        doc["state_topic"] = MQTT_TOPIC_MODE_STATE;
        doc["icon"] = "mdi:cog-transfer";
        doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        JsonArray options = doc.createNestedArray("options");
        options.add("manual");
        options.add("auto");
        options.add("monitor");
        doc["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        doc["device"]["name"] = MQTT_DEVICE_NAME;

        serializeJson(doc, payload);
        client->publish((String(HA_DISCOVERY_PREFIX) + "/select/smartgarden_operation_mode/config").c_str(), payload, true);
    }
    delay(50);

    Serial.println("[OK] All discoveries published!");
}
