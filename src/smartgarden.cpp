#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>
#include <ArduinoJson.h>

#include "app_config.h"
#include "pins.h"
#include "garden_profile.h"

namespace {
constexpr uint8_t RELAY_COUNT = 8;
constexpr uint16_t MODBUS_START_REGISTER = 0x0000;
constexpr uint16_t MODBUS_REGISTER_COUNT = 40;

// Confirmed register map for the SmartGarden RS485/NPK soil sensor.
constexpr uint8_t SOIL_MOISTURE_REGISTER = 0;
constexpr uint8_t SOIL_TEMPERATURE_REGISTER = 1;
constexpr uint8_t SOIL_PH_REGISTER = 3;
constexpr uint8_t SOIL_NITROGEN_REGISTER = 4;
constexpr uint8_t SOIL_PHOSPHORUS_REGISTER = 5;
constexpr uint8_t SOIL_POTASSIUM_REGISTER = 6;
constexpr uint8_t SOIL_EC_REGISTER = 9;
constexpr uint8_t SOIL_SALINITY_REGISTER = 35;
constexpr uint8_t SOIL_TDS_REGISTER = 36;
constexpr uint16_t SENSOR_EXPIRE_AFTER_SECONDS = 20;
constexpr uint16_t SYSTEM_EXPIRE_AFTER_SECONDS = 90;

const char* const RELAY_NAMES[RELAY_COUNT] = {
    "Fan", "Heater", "Cooler", "Humidifier",
    "Dehumidifier", "Irrigation", "Relay7", "Relay8"
};

const char* const RELAY_DISCOVERY_IDS[RELAY_COUNT] = {
    "fan", "heater", "cooler", "humidifier",
    "dehumidifier", "irrigation", "relay7", "relay8"
};

const char* const RELAY_ICONS[RELAY_COUNT] = {
    "mdi:fan", "mdi:fire", "mdi:snowflake", "mdi:water-opacity",
    "mdi:water-off", "mdi:water-pump", "mdi:toggle-switch", "mdi:toggle-switch"
};

struct AirState {
    float temperature = 0.0f;
    float humidity = 0.0f;
    bool temperatureValid = false;
    bool humidityValid = false;

    bool valid() const {
        return temperatureValid && humidityValid;
    }
};

struct SoilState {
    float moisture = 0.0f;
    float temperature = 0.0f;
    float ph = 0.0f;
    uint16_t ec = 0;
    uint16_t nitrogen = 0;
    uint16_t phosphorus = 0;
    uint16_t potassium = 0;
    float salinity = 0.0f;
    uint16_t tds = 0;
    bool valid = false;
};

enum class OperationMode {
    MANUAL,
    AUTO,
    MONITOR,
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHT_TYPE);
HardwareSerial RS485Serial(2);
ModbusMaster node;

bool relayStates[RELAY_COUNT] = {false};
AirState lastAirState;
SoilState lastSoilState;
OperationMode currentMode = OperationMode::MANUAL;
const CropProfile* currentCrop = nullptr;
unsigned long lastSensorReadAt = 0;
unsigned long lastStatusPublishAt = 0;
unsigned long lastMqttReconnectAttemptAt = 0;
char mqttClientId[64] = {0};

void preTransmission() {
    digitalWrite(RS485_DE, HIGH);
}

void postTransmission() {
    digitalWrite(RS485_DE, LOW);
}

const char* modeToString(OperationMode mode) {
    switch (mode) {
        case OperationMode::MANUAL:
            return "manual";
        case OperationMode::AUTO:
            return "auto";
        case OperationMode::MONITOR:
            return "monitor";
    }
    return "manual";
}

bool parseMode(const char* payload, OperationMode& mode) {
    if (strcmp(payload, "manual") == 0) {
        mode = OperationMode::MANUAL;
        return true;
    }
    if (strcmp(payload, "auto") == 0) {
        mode = OperationMode::AUTO;
        return true;
    }
    if (strcmp(payload, "monitor") == 0) {
        mode = OperationMode::MONITOR;
        return true;
    }
    return false;
}

void buildRelayStateTopic(uint8_t relayIndex, char* topic, size_t topicSize) {
    snprintf(topic, topicSize, "smartgarden/relay/%u/state", relayIndex + 1);
}

void buildRelayCommandTopic(uint8_t relayIndex, char* topic, size_t topicSize) {
    snprintf(topic, topicSize, "smartgarden/relay/%u/set", relayIndex + 1);
}

void publishRetained(const char* topic, const char* payload) {
    mqttClient.publish(topic, payload, true);
}

void publishFloat(const char* topic, float value, uint8_t precision = 1) {
    char payload[32];
    snprintf(payload, sizeof(payload), precision == 2 ? "%.2f" : "%.1f", value);
    publishRetained(topic, payload);
}

void publishUInt(const char* topic, uint32_t value) {
    char payload[32];
    snprintf(payload, sizeof(payload), "%lu", static_cast<unsigned long>(value));
    publishRetained(topic, payload);
}

void publishInt(const char* topic, int32_t value) {
    char payload[32];
    snprintf(payload, sizeof(payload), "%ld", static_cast<long>(value));
    publishRetained(topic, payload);
}

void publishRelayState(uint8_t relayIndex) {
    char topic[48];
    buildRelayStateTopic(relayIndex, topic, sizeof(topic));
    publishRetained(topic, relayStates[relayIndex] ? "ON" : "OFF");
}

void setRelay(uint8_t relayIndex, bool state, bool publishState = true) {
    if (relayIndex >= RELAY_COUNT) {
        Serial.printf("[Relay] Invalid relay index: %u\n", relayIndex + 1);
        return;
    }

    relayStates[relayIndex] = state;
    digitalWrite(RELAY_PINS[relayIndex], state ? LOW : HIGH);

    Serial.printf("[Relay] %u - %s => %s\n", relayIndex + 1, RELAY_NAMES[relayIndex], state ? "ON" : "OFF");

    if (publishState && mqttClient.connected()) {
        publishRelayState(relayIndex);
    }
}

void setAllRelaysOff() {
    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        setRelay(i, false, false);
    }

    if (mqttClient.connected()) {
        for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
            publishRelayState(i);
        }
    }
}

void publishCurrentMode() {
    publishRetained(MQTT_TOPIC_MODE_STATE, modeToString(currentMode));
}

void publishCurrentCrop() {
    if (currentCrop != nullptr) {
        publishRetained(MQTT_TOPIC_CROP_CURRENT, currentCrop->name);
    }
}

void publishCropList() {
    uint8_t cropCount = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(cropCount);
    char payload[512] = {0};

    for (uint8_t i = 0; i < cropCount; ++i) {
        if (i > 0) {
            strncat(payload, ",", sizeof(payload) - strlen(payload) - 1);
        }
        strncat(payload, crops[i].name, sizeof(payload) - strlen(payload) - 1);
    }

    publishRetained(MQTT_TOPIC_CROP_LIST, payload);
}

void publishAirStateIfValid() {
    if (!lastAirState.valid()) {
        return;
    }

    publishFloat(MQTT_TOPIC_AIR_TEMP, lastAirState.temperature, 1);
    publishFloat(MQTT_TOPIC_AIR_HUMIDITY, lastAirState.humidity, 1);
}

void publishSoilStateIfValid() {
    if (!lastSoilState.valid) {
        return;
    }

    publishFloat(MQTT_TOPIC_SOIL_MOISTURE, lastSoilState.moisture, 1);
    publishFloat(MQTT_TOPIC_SOIL_TEMP, lastSoilState.temperature, 1);
    publishFloat(MQTT_TOPIC_PH, lastSoilState.ph, 1);
    publishUInt(MQTT_TOPIC_EC, lastSoilState.ec);
    publishUInt(MQTT_TOPIC_NITROGEN, lastSoilState.nitrogen);
    publishUInt(MQTT_TOPIC_PHOSPHORUS, lastSoilState.phosphorus);
    publishUInt(MQTT_TOPIC_POTASSIUM, lastSoilState.potassium);
    publishFloat(MQTT_TOPIC_SALINITY, lastSoilState.salinity, 1);
    publishUInt(MQTT_TOPIC_TDS, lastSoilState.tds);
}

void publishSystemState() {
    publishInt(MQTT_TOPIC_RSSI, WiFi.RSSI());
    publishUInt(MQTT_TOPIC_UPTIME, millis() / 1000UL);
    publishUInt(MQTT_TOPIC_FREE_HEAP, ESP.getFreeHeap());
}

void publishDiscoveryMessage(const char* topic, JsonDocument& doc) {
    char payload[1024];
    size_t payloadLength = serializeJson(doc, payload, sizeof(payload));
    bool truncated = payloadLength >= sizeof(payload) - 1;
    bool published = !truncated && mqttClient.publish(topic, payload, true);

    if (published) {
        Serial.printf("[HA Discovery] %s => OK\n", topic);
    } else {
        Serial.printf("[HA Discovery] %s => FAIL%s\n", topic, truncated ? " (payload truncated)" : "");
    }

    delay(25);
}

void addAvailability(JsonDocument& doc) {
    doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
}

void addDeviceMetadata(JsonDocument& doc) {
    JsonObject device = doc["device"].to<JsonObject>();
    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(MQTT_DEVICE_ID);
    device["name"] = MQTT_DEVICE_NAME;
    device["manufacturer"] = MQTT_DEVICE_MANUFACTURER;
    device["model"] = MQTT_DEVICE_MODEL;
}

void publishSensorDiscovery(const char* objectId,
                            const char* name,
                            const char* stateTopic,
                            const char* unit,
                            const char* deviceClass,
                            const char* stateClass,
                            const char* icon,
                            uint16_t expireAfterSeconds) {
    char topic[128];
    StaticJsonDocument<512> doc;

    doc["name"] = name;
    doc["unique_id"] = objectId;
    doc["state_topic"] = stateTopic;
    if (unit != nullptr) {
        doc["unit_of_measurement"] = unit;
    }
    if (deviceClass != nullptr) {
        doc["device_class"] = deviceClass;
    }
    if (stateClass != nullptr) {
        doc["state_class"] = stateClass;
    }
    if (icon != nullptr) {
        doc["icon"] = icon;
    }
    doc["expire_after"] = expireAfterSeconds;
    addAvailability(doc);
    addDeviceMetadata(doc);

    snprintf(topic, sizeof(topic), "%s/sensor/%s/config", HA_DISCOVERY_PREFIX, objectId);
    publishDiscoveryMessage(topic, doc);
}

void publishRelayDiscovery(uint8_t relayIndex) {
    char topic[128];
    char stateTopic[48];
    char commandTopic[48];
    StaticJsonDocument<512> doc;

    buildRelayStateTopic(relayIndex, stateTopic, sizeof(stateTopic));
    buildRelayCommandTopic(relayIndex, commandTopic, sizeof(commandTopic));

    doc["name"] = RELAY_NAMES[relayIndex];
    doc["unique_id"] = String("smartgarden_") + RELAY_DISCOVERY_IDS[relayIndex];
    doc["state_topic"] = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = RELAY_ICONS[relayIndex];
    addAvailability(doc);
    addDeviceMetadata(doc);

    snprintf(topic, sizeof(topic), "%s/switch/smartgarden_%s/config", HA_DISCOVERY_PREFIX, RELAY_DISCOVERY_IDS[relayIndex]);
    publishDiscoveryMessage(topic, doc);
}

void publishModeSelectDiscovery() {
    char topic[128];
    StaticJsonDocument<512> doc;

    doc["name"] = "Operation Mode";
    doc["unique_id"] = "smartgarden_operation_mode";
    doc["state_topic"] = MQTT_TOPIC_MODE_STATE;
    doc["command_topic"] = MQTT_TOPIC_MODE_SET;
    doc["icon"] = "mdi:cog";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("manual");
    options.add("auto");
    options.add("monitor");
    addAvailability(doc);
    addDeviceMetadata(doc);

    snprintf(topic, sizeof(topic), "%s/select/smartgarden_operation_mode/config", HA_DISCOVERY_PREFIX);
    publishDiscoveryMessage(topic, doc);
}

void publishCropSelectDiscovery() {
    char topic[128];
    StaticJsonDocument<1024> doc;
    uint8_t cropCount = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(cropCount);

    doc["name"] = "Crop Profile";
    doc["unique_id"] = "smartgarden_crop";
    doc["state_topic"] = MQTT_TOPIC_CROP_CURRENT;
    doc["command_topic"] = MQTT_TOPIC_CROP_SELECT;
    doc["icon"] = "mdi:leaf";
    JsonArray options = doc["options"].to<JsonArray>();
    for (uint8_t i = 0; i < cropCount; ++i) {
        options.add(crops[i].name);
    }
    addAvailability(doc);
    addDeviceMetadata(doc);

    snprintf(topic, sizeof(topic), "%s/select/smartgarden_crop/config", HA_DISCOVERY_PREFIX);
    publishDiscoveryMessage(topic, doc);
}

void publishDiscoveryMessages() {
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");

    publishSensorDiscovery("smartgarden_air_temp", "Air Temperature", MQTT_TOPIC_AIR_TEMP, "°C", "temperature", "measurement", "mdi:thermometer", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_air_humidity", "Air Humidity", MQTT_TOPIC_AIR_HUMIDITY, "%", "humidity", "measurement", "mdi:water-percent", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_soil_moisture", "Soil Moisture", MQTT_TOPIC_SOIL_MOISTURE, "%", nullptr, "measurement", "mdi:water", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_soil_temp", "Soil Temperature", MQTT_TOPIC_SOIL_TEMP, "°C", "temperature", "measurement", "mdi:thermometer", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_ph", "Soil pH", MQTT_TOPIC_PH, "pH", nullptr, "measurement", "mdi:test-tube", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_ec", "Soil EC", MQTT_TOPIC_EC, "µS/cm", nullptr, "measurement", "mdi:flash", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_nitrogen", "Nitrogen", MQTT_TOPIC_NITROGEN, "mg/kg", nullptr, "measurement", "mdi:leaf", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_phosphorus", "Phosphorus", MQTT_TOPIC_PHOSPHORUS, "mg/kg", nullptr, "measurement", "mdi:leaf", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_potassium", "Potassium", MQTT_TOPIC_POTASSIUM, "mg/kg", nullptr, "measurement", "mdi:leaf", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_salinity", "Soil Salinity", MQTT_TOPIC_SALINITY, "ppt", nullptr, "measurement", "mdi:shaker", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_tds", "Soil TDS", MQTT_TOPIC_TDS, "ppm", nullptr, "measurement", "mdi:waves", SENSOR_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_rssi", "WiFi RSSI", MQTT_TOPIC_RSSI, "dBm", "signal_strength", "measurement", "mdi:wifi", SYSTEM_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_uptime", "Uptime", MQTT_TOPIC_UPTIME, "s", "duration", "measurement", "mdi:timer-outline", SYSTEM_EXPIRE_AFTER_SECONDS);
    publishSensorDiscovery("smartgarden_free_heap", "Free Heap", MQTT_TOPIC_FREE_HEAP, "B", nullptr, "measurement", "mdi:memory", SYSTEM_EXPIRE_AFTER_SECONDS);

    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        publishRelayDiscovery(i);
    }

    publishCropSelectDiscovery();
    publishModeSelectDiscovery();

    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

void subscribeToTopics() {
    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        char topic[48];
        buildRelayCommandTopic(i, topic, sizeof(topic));
        bool ok = mqttClient.subscribe(topic);
        Serial.printf("[MQTT] SUB %s => %s\n", topic, ok ? "OK" : "FAIL");
    }

    Serial.printf("[MQTT] SUB %s => %s\n", MQTT_TOPIC_CROP_SELECT, mqttClient.subscribe(MQTT_TOPIC_CROP_SELECT) ? "OK" : "FAIL");
    Serial.printf("[MQTT] SUB %s => %s\n", MQTT_TOPIC_MODE_SET, mqttClient.subscribe(MQTT_TOPIC_MODE_SET) ? "OK" : "FAIL");
}

void publishAllState() {
    publishRetained(MQTT_TOPIC_AVAILABILITY, "online");
    publishCurrentMode();
    publishCurrentCrop();
    publishCropList();
    publishAirStateIfValid();
    publishSoilStateIfValid();
    publishSystemState();

    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        publishRelayState(i);
    }
}

void ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("[WiFi] Connection timeout");
    }
}

void reconnectMQTT() {
    if (mqttClient.connected()) {
        return;
    }

    Serial.printf("[MQTT] Connecting to %s:%d ... ", MQTT_BROKER, MQTT_PORT);
    bool connected = mqttClient.connect(
        mqttClientId,
        MQTT_USERNAME,
        MQTT_PASSWORD,
        MQTT_TOPIC_AVAILABILITY,
        1,
        true,
        "offline");

    lastMqttReconnectAttemptAt = millis();
    if (!connected) {
        Serial.printf("FAILED (code=%d)\n", mqttClient.state());
        return;
    }

    Serial.println("CONNECTED");
    subscribeToTopics();
    publishDiscoveryMessages();
    publishAllState();
}

bool readAirSensor() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    bool updated = false;

    if (!isnan(temperature)) {
        lastAirState.temperature = temperature;
        lastAirState.temperatureValid = true;
        updated = true;
    } else {
        Serial.println("[DHT] Temperature read failed, keeping last valid value");
    }

    if (!isnan(humidity)) {
        lastAirState.humidity = humidity;
        lastAirState.humidityValid = true;
        updated = true;
    } else {
        Serial.println("[DHT] Humidity read failed, keeping last valid value");
    }

    return updated && lastAirState.valid();
}

bool readSoilSensor() {
    uint8_t result = node.readHoldingRegisters(MODBUS_START_REGISTER, MODBUS_REGISTER_COUNT);
    if (result != node.ku8MBSuccess) {
        Serial.printf("[RS485] Modbus read failed, error=0x%02X\n", result);
        return false;
    }

    lastSoilState.moisture = node.getResponseBuffer(SOIL_MOISTURE_REGISTER) / 10.0f;
    lastSoilState.temperature = node.getResponseBuffer(SOIL_TEMPERATURE_REGISTER) / 10.0f;
    lastSoilState.ph = node.getResponseBuffer(SOIL_PH_REGISTER) / 10.0f;
    lastSoilState.nitrogen = node.getResponseBuffer(SOIL_NITROGEN_REGISTER);
    lastSoilState.phosphorus = node.getResponseBuffer(SOIL_PHOSPHORUS_REGISTER);
    lastSoilState.potassium = node.getResponseBuffer(SOIL_POTASSIUM_REGISTER);
    lastSoilState.ec = node.getResponseBuffer(SOIL_EC_REGISTER);
    lastSoilState.salinity = node.getResponseBuffer(SOIL_SALINITY_REGISTER) / 10.0f;
    lastSoilState.tds = node.getResponseBuffer(SOIL_TDS_REGISTER);
    lastSoilState.valid = true;

    Serial.printf(
        "[RS485] Soil sensor read OK | moisture=%.1f%% temp=%.1fC pH=%.1f EC=%u N=%u P=%u K=%u salinity=%.1fppt TDS=%u\n",
        lastSoilState.moisture,
        lastSoilState.temperature,
        lastSoilState.ph,
        static_cast<unsigned int>(lastSoilState.ec),
        static_cast<unsigned int>(lastSoilState.nitrogen),
        static_cast<unsigned int>(lastSoilState.phosphorus),
        static_cast<unsigned int>(lastSoilState.potassium),
        lastSoilState.salinity,
        static_cast<unsigned int>(lastSoilState.tds));
    return true;
}

void printSensorSummary() {
    Serial.println("\n========== SENSOR DATA ==========");
    if (lastAirState.valid()) {
        Serial.printf("Air Temp     : %.1f C\n", lastAirState.temperature);
        Serial.printf("Air Humidity : %.1f %%\n", lastAirState.humidity);
    } else {
        Serial.println("Air Temp     : unavailable");
        Serial.println("Air Humidity : unavailable");
    }

    if (lastSoilState.valid) {
        Serial.printf("Soil Moisture: %.1f %%\n", lastSoilState.moisture);
        Serial.printf("Soil Temp    : %.1f C\n", lastSoilState.temperature);
        Serial.printf("pH           : %.1f\n", lastSoilState.ph);
        Serial.printf("EC           : %u uS/cm\n", static_cast<unsigned int>(lastSoilState.ec));
        Serial.printf("Nitrogen     : %u mg/kg\n", static_cast<unsigned int>(lastSoilState.nitrogen));
        Serial.printf("Phosphorus   : %u mg/kg\n", static_cast<unsigned int>(lastSoilState.phosphorus));
        Serial.printf("Potassium    : %u mg/kg\n", static_cast<unsigned int>(lastSoilState.potassium));
        Serial.printf("Salinity     : %.1f ppt\n", lastSoilState.salinity);
        Serial.printf("TDS          : %u ppm\n", static_cast<unsigned int>(lastSoilState.tds));
    } else {
        Serial.println("Soil Moisture: unavailable");
        Serial.println("Soil Temp    : unavailable");
        Serial.println("pH           : unavailable");
        Serial.println("EC           : unavailable");
        Serial.println("Nitrogen     : unavailable");
        Serial.println("Phosphorus   : unavailable");
        Serial.println("Potassium    : unavailable");
        Serial.println("Salinity     : unavailable");
        Serial.println("TDS          : unavailable");
    }
    Serial.println("=================================");
}

void applyAutoControl() {
    if (currentMode != OperationMode::AUTO || currentCrop == nullptr) {
        return;
    }

    Serial.printf("[Auto] Evaluating crop '%s'\n", currentCrop->name);

    if (lastAirState.valid()) {
        bool heaterOn = lastAirState.temperature < currentCrop->temperature.min;
        bool coolerOn = lastAirState.temperature > currentCrop->temperature.max;
        bool fanOn = coolerOn;
        setRelay(CLIMATE_HEATER_RELAY_INDEX, heaterOn);
        setRelay(CLIMATE_COOLER_RELAY_INDEX, coolerOn);
        setRelay(CLIMATE_FAN_RELAY_INDEX, fanOn);

        bool humidifierOn = lastAirState.humidity < currentCrop->airHumidity.min;
        bool dehumidifierOn = lastAirState.humidity > currentCrop->airHumidity.max;
        setRelay(CLIMATE_HUMIDIFIER_RELAY_INDEX, humidifierOn);
        setRelay(CLIMATE_DEHUMIDIFIER_RELAY_INDEX, dehumidifierOn);
    }

    if (lastSoilState.valid) {
        bool irrigationOn = lastSoilState.moisture < currentCrop->soilHumidity.min;
        setRelay(IRRIGATION_RELAY_INDEX, irrigationOn);
    }
}

void handleCropSelection(const char* payload) {
    const CropProfile* crop = CropProfileStore::getCropByName(payload);
    if (crop == nullptr) {
        Serial.printf("[Crop] Unknown crop: %s\n", payload);
        return;
    }

    currentCrop = crop;
    Serial.printf("[Crop] Selected crop: %s\n", currentCrop->name);
    if (mqttClient.connected()) {
        publishCurrentCrop();
    }
}

void handleModeSelection(const char* payload) {
    OperationMode nextMode;
    if (!parseMode(payload, nextMode)) {
        Serial.printf("[Mode] Invalid mode: %s\n", payload);
        return;
    }

    currentMode = nextMode;
    Serial.printf("[Mode] Changed to: %s\n", modeToString(currentMode));

    if (currentMode == OperationMode::MONITOR) {
        setAllRelaysOff();
    } else if (currentMode == OperationMode::AUTO) {
        applyAutoControl();
    }

    if (mqttClient.connected()) {
        publishCurrentMode();
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char message[128];
    if (length >= sizeof(message)) {
        length = sizeof(message) - 1;
    }
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("[MQTT RX] %s = %s\n", topic, message);

    if (strcmp(topic, MQTT_TOPIC_CROP_SELECT) == 0) {
        handleCropSelection(message);
        return;
    }

    if (strcmp(topic, MQTT_TOPIC_MODE_SET) == 0) {
        handleModeSelection(message);
        return;
    }

    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        char relayTopic[48];
        buildRelayCommandTopic(i, relayTopic, sizeof(relayTopic));
        if (strcmp(topic, relayTopic) != 0) {
            continue;
        }

        if (currentMode != OperationMode::MANUAL) {
            Serial.printf("[Mode] %s mode: relay command ignored\n", modeToString(currentMode));
            return;
        }

        if (strcmp(message, "ON") == 0) {
            setRelay(i, true);
        } else if (strcmp(message, "OFF") == 0) {
            setRelay(i, false);
        } else {
            Serial.printf("[Relay] Unsupported payload for relay %u: %s\n", i + 1, message);
        }
        return;
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);

    snprintf(mqttClientId, sizeof(mqttClientId), "smartgarden_%llX", static_cast<unsigned long long>(ESP.getEfuseMac()));

    Serial.println("\n========== SmartGarden Startup ==========");
    CropProfileStore::initialize();
    currentCrop = CropProfileStore::getCropById(2);

    Serial.println("[Setup] Initializing relays...");
    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH);
        relayStates[i] = false;
    }
    Serial.println("[Setup] Relays initialized");

    Serial.printf("[Setup] Initializing DHT22 on GPIO %u...\n", DHT_PIN);
    dht.begin();
    Serial.println("[Setup] DHT22 initialized");

    Serial.printf("[Setup] Initializing RS485 on RX=%u TX=%u DE=%u...\n", RS485_RX, RS485_TX, RS485_DE);
    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW);
    RS485Serial.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX, RS485_TX);
    node.begin(RS485_SLAVE_ID, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    Serial.printf("[Setup] RS485 initialized at %u baud\n", RS485_BAUD_RATE);

    ensureWiFiConnected();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

    if (WiFi.status() == WL_CONNECTED) {
        reconnectMQTT();
    }

    Serial.println("========== Setup Complete ==========");
}

void loop() {
    ensureWiFiConnected();

    const unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected() &&
        now - lastMqttReconnectAttemptAt >= MQTT_RECONNECT_INTERVAL) {
        reconnectMQTT();
    }

    mqttClient.loop();
    if (now - lastSensorReadAt >= SENSOR_READ_INTERVAL) {
        lastSensorReadAt = now;

        readAirSensor();
        const bool soilReadOk = readSoilSensor();

        if (mqttClient.connected()) {
            publishAirStateIfValid();
            if (soilReadOk || lastSoilState.valid) {
                publishSoilStateIfValid();
            }
        }

        applyAutoControl();
        printSensorSummary();
    }

    if (mqttClient.connected() && now - lastStatusPublishAt >= PUBLISH_STATUS_INTERVAL) {
        lastStatusPublishAt = now;
        publishSystemState();
        publishRetained(MQTT_TOPIC_AVAILABILITY, "online");
    }

    delay(10);
}
