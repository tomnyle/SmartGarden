#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>

#include "app_config.h"
#include "pins.h"
#include "garden_profile.h"

namespace {
constexpr uint8_t RELAY_COUNT = 8;
constexpr uint16_t MODBUS_START_REGISTER = 0x0000;
constexpr uint16_t MODBUS_REGISTER_COUNT = 40;

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
    bool valid = false;
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

const char* jsonStringOrNull(const char* value, char* buffer, size_t bufferSize) {
    if (value == nullptr) {
        strlcpy(buffer, "null", bufferSize);
    } else {
        snprintf(buffer, bufferSize, "\"%s\"", value);
    }
    return buffer;
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
    if (!lastAirState.valid) {
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

void publishDiscoveryMessage(const char* topic, const char* payload) {
    mqttClient.publish(topic, payload, true);
    delay(25);
    Serial.printf("[HA Discovery] %s => OK\n", topic);
}

void publishSensorDiscovery(const char* objectId,
                            const char* name,
                            const char* stateTopic,
                            const char* unit,
                            const char* deviceClass,
                            const char* stateClass,
                            const char* icon) {
    char topic[128];
    char payload[1024];
    char deviceClassJson[64];
    char stateClassJson[64];
    char iconJson[64];
    snprintf(topic, sizeof(topic), "%s/sensor/%s/config", HA_DISCOVERY_PREFIX, objectId);
    snprintf(
        payload,
        sizeof(payload),
        "{\"name\":\"%s\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"unit_of_measurement\":\"%s\",\"device_class\":%s,\"state_class\":%s,\"icon\":%s,\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"%s\",\"model\":\"%s\"}}",
        name,
        objectId,
        stateTopic,
        MQTT_TOPIC_AVAILABILITY,
        unit != nullptr ? unit : "",
        jsonStringOrNull(deviceClass, deviceClassJson, sizeof(deviceClassJson)),
        jsonStringOrNull(stateClass, stateClassJson, sizeof(stateClassJson)),
        jsonStringOrNull(icon, iconJson, sizeof(iconJson)),
        MQTT_DEVICE_ID,
        MQTT_DEVICE_NAME,
        MQTT_DEVICE_MANUFACTURER,
        MQTT_DEVICE_MODEL);
    publishDiscoveryMessage(topic, payload);
}

void publishRelayDiscovery(uint8_t relayIndex) {
    char topic[128];
    char payload[1024];
    char stateTopic[48];
    char commandTopic[48];

    buildRelayStateTopic(relayIndex, stateTopic, sizeof(stateTopic));
    buildRelayCommandTopic(relayIndex, commandTopic, sizeof(commandTopic));

    snprintf(topic, sizeof(topic), "%s/switch/smartgarden_%s/config", HA_DISCOVERY_PREFIX, RELAY_DISCOVERY_IDS[relayIndex]);
    snprintf(
        payload,
        sizeof(payload),
        "{\"name\":\"%s\",\"unique_id\":\"smartgarden_%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"%s\",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"%s\",\"model\":\"%s\"}}",
        RELAY_NAMES[relayIndex],
        RELAY_DISCOVERY_IDS[relayIndex],
        stateTopic,
        commandTopic,
        MQTT_TOPIC_AVAILABILITY,
        RELAY_ICONS[relayIndex],
        MQTT_DEVICE_ID,
        MQTT_DEVICE_NAME,
        MQTT_DEVICE_MANUFACTURER,
        MQTT_DEVICE_MODEL);
    publishDiscoveryMessage(topic, payload);
}

void publishSelectDiscovery(const char* componentId,
                            const char* name,
                            const char* stateTopic,
                            const char* commandTopic,
                            const char* optionsCsv,
                            const char* icon) {
    char topic[128];
    char payload[1024];
    char optionsJson[512] = {0};
    const char* cursor = optionsCsv;
    bool first = true;

    strncat(optionsJson, "[", sizeof(optionsJson) - 1);
    while (*cursor != '\0') {
        const char* comma = strchr(cursor, ',');
        size_t len = comma != nullptr ? static_cast<size_t>(comma - cursor) : strlen(cursor);
        if (!first) {
            strncat(optionsJson, ",", sizeof(optionsJson) - strlen(optionsJson) - 1);
        }
        char optionValue[64] = {0};
        strncpy(optionValue, cursor, len);
        optionValue[len] = '\0';
        strncat(optionsJson, "\"", sizeof(optionsJson) - strlen(optionsJson) - 1);
        strncat(optionsJson, optionValue, sizeof(optionsJson) - strlen(optionsJson) - 1);
        strncat(optionsJson, "\"", sizeof(optionsJson) - strlen(optionsJson) - 1);
        if (comma == nullptr) {
            break;
        }
        cursor = comma + 1;
        first = false;
    }
    strncat(optionsJson, "]", sizeof(optionsJson) - strlen(optionsJson) - 1);

    snprintf(topic, sizeof(topic), "%s/select/%s/config", HA_DISCOVERY_PREFIX, componentId);
    snprintf(
        payload,
        sizeof(payload),
        "{\"name\":\"%s\",\"unique_id\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"icon\":\"%s\",\"options\":%s,\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"%s\",\"model\":\"%s\"}}",
        name,
        componentId,
        stateTopic,
        commandTopic,
        MQTT_TOPIC_AVAILABILITY,
        icon,
        optionsJson,
        MQTT_DEVICE_ID,
        MQTT_DEVICE_NAME,
        MQTT_DEVICE_MANUFACTURER,
        MQTT_DEVICE_MODEL);
    publishDiscoveryMessage(topic, payload);
}

void publishDiscoveryMessages() {
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");

    publishSensorDiscovery("smartgarden_air_temp", "Air Temperature", MQTT_TOPIC_AIR_TEMP, "°C", "temperature", "measurement", "mdi:thermometer");
    publishSensorDiscovery("smartgarden_air_humidity", "Air Humidity", MQTT_TOPIC_AIR_HUMIDITY, "%", "humidity", "measurement", "mdi:water-percent");
    publishSensorDiscovery("smartgarden_soil_moisture", "Soil Moisture", MQTT_TOPIC_SOIL_MOISTURE, "%", nullptr, "measurement", "mdi:water");
    publishSensorDiscovery("smartgarden_soil_temp", "Soil Temperature", MQTT_TOPIC_SOIL_TEMP, "°C", "temperature", "measurement", "mdi:thermometer");
    publishSensorDiscovery("smartgarden_ph", "Soil pH", MQTT_TOPIC_PH, "pH", nullptr, "measurement", "mdi:test-tube");
    publishSensorDiscovery("smartgarden_ec", "Soil EC", MQTT_TOPIC_EC, "µS/cm", nullptr, "measurement", "mdi:flash");
    publishSensorDiscovery("smartgarden_nitrogen", "Nitrogen", MQTT_TOPIC_NITROGEN, "mg/kg", nullptr, "measurement", "mdi:leaf");
    publishSensorDiscovery("smartgarden_phosphorus", "Phosphorus", MQTT_TOPIC_PHOSPHORUS, "mg/kg", nullptr, "measurement", "mdi:leaf");
    publishSensorDiscovery("smartgarden_potassium", "Potassium", MQTT_TOPIC_POTASSIUM, "mg/kg", nullptr, "measurement", "mdi:leaf");
    publishSensorDiscovery("smartgarden_salinity", "Soil Salinity", MQTT_TOPIC_SALINITY, "ppt", nullptr, "measurement", "mdi:shaker");
    publishSensorDiscovery("smartgarden_tds", "Soil TDS", MQTT_TOPIC_TDS, "ppm", nullptr, "measurement", "mdi:waves");
    publishSensorDiscovery("smartgarden_rssi", "WiFi RSSI", MQTT_TOPIC_RSSI, "dBm", "signal_strength", "measurement", "mdi:wifi");
    publishSensorDiscovery("smartgarden_uptime", "Uptime", MQTT_TOPIC_UPTIME, "s", "duration", "measurement", "mdi:timer-outline");
    publishSensorDiscovery("smartgarden_free_heap", "Free Heap", MQTT_TOPIC_FREE_HEAP, "B", nullptr, "measurement", "mdi:memory");

    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        publishRelayDiscovery(i);
    }

    char cropOptions[512] = {0};
    uint8_t cropCount = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(cropCount);
    for (uint8_t i = 0; i < cropCount; ++i) {
        if (i > 0) {
            strncat(cropOptions, ",", sizeof(cropOptions) - strlen(cropOptions) - 1);
        }
        strncat(cropOptions, crops[i].name, sizeof(cropOptions) - strlen(cropOptions) - 1);
    }

    publishSelectDiscovery("smartgarden_crop", "Crop Profile", MQTT_TOPIC_CROP_CURRENT, MQTT_TOPIC_CROP_SELECT, cropOptions, "mdi:leaf");
    publishSelectDiscovery("smartgarden_operation_mode", "Operation Mode", MQTT_TOPIC_MODE_STATE, MQTT_TOPIC_MODE_SET, "manual,auto,monitor", "mdi:cog");

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
    while (!mqttClient.connected()) {
        Serial.printf("[MQTT] Connecting to %s:%d ... ", MQTT_BROKER, MQTT_PORT);
        bool connected = mqttClient.connect(
            mqttClientId,
            MQTT_USERNAME,
            MQTT_PASSWORD,
            MQTT_TOPIC_AVAILABILITY,
            1,
            true,
            "offline");

        if (!connected) {
            Serial.printf("FAILED (code=%d)\n", mqttClient.state());
            delay(MQTT_RECONNECT_INTERVAL);
            continue;
        }

        Serial.println("CONNECTED");
        subscribeToTopics();
        publishDiscoveryMessages();
        publishAllState();
    }
}

bool readAirSensor() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    bool updated = false;

    if (!isnan(temperature)) {
        lastAirState.temperature = temperature;
        updated = true;
    } else {
        Serial.println("[DHT] Temperature read failed, keeping last valid value");
    }

    if (!isnan(humidity)) {
        lastAirState.humidity = humidity;
        updated = true;
    } else {
        Serial.println("[DHT] Humidity read failed, keeping last valid value");
    }

    if (!isnan(temperature) && !isnan(humidity)) {
        lastAirState.valid = true;
    }

    return updated && lastAirState.valid;
}

bool readSoilSensor() {
    uint8_t result = node.readHoldingRegisters(MODBUS_START_REGISTER, MODBUS_REGISTER_COUNT);
    if (result != node.ku8MBSuccess) {
        Serial.printf("[RS485] Modbus read failed, error=0x%02X\n", result);
        return false;
    }

    lastSoilState.moisture = node.getResponseBuffer(0) / 10.0f;
    lastSoilState.temperature = node.getResponseBuffer(1) / 10.0f;
    lastSoilState.ph = node.getResponseBuffer(3) / 10.0f;
    lastSoilState.nitrogen = node.getResponseBuffer(4);
    lastSoilState.phosphorus = node.getResponseBuffer(5);
    lastSoilState.potassium = node.getResponseBuffer(6);
    lastSoilState.ec = node.getResponseBuffer(9);
    lastSoilState.salinity = node.getResponseBuffer(35) / 10.0f;
    lastSoilState.tds = node.getResponseBuffer(36);
    lastSoilState.valid = true;

    Serial.printf(
        "[RS485] Soil sensor read OK | moisture=%.1f%% temp=%.1fC pH=%.1f EC=%u N=%u P=%u K=%u salinity=%.1fppt TDS=%u\n",
        lastSoilState.moisture,
        lastSoilState.temperature,
        lastSoilState.ph,
        lastSoilState.ec,
        lastSoilState.nitrogen,
        lastSoilState.phosphorus,
        lastSoilState.potassium,
        lastSoilState.salinity,
        lastSoilState.tds);
    return true;
}

void printSensorSummary() {
    Serial.println("\n========== SENSOR DATA ==========");
    if (lastAirState.valid) {
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
        Serial.printf("EC           : %u uS/cm\n", lastSoilState.ec);
        Serial.printf("Nitrogen     : %u mg/kg\n", lastSoilState.nitrogen);
        Serial.printf("Phosphorus   : %u mg/kg\n", lastSoilState.phosphorus);
        Serial.printf("Potassium    : %u mg/kg\n", lastSoilState.potassium);
        Serial.printf("Salinity     : %.1f ppt\n", lastSoilState.salinity);
        Serial.printf("TDS          : %u ppm\n", lastSoilState.tds);
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

    if (lastAirState.valid) {
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
    node.begin(1, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    Serial.printf("[Setup] RS485 initialized at %u baud\n", RS485_BAUD_RATE);

    ensureWiFiConnected();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

    Serial.println("========== Setup Complete ==========");
}

void loop() {
    ensureWiFiConnected();

    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
        reconnectMQTT();
    }

    mqttClient.loop();

    const unsigned long now = millis();
    if (now - lastSensorReadAt >= SENSOR_READ_INTERVAL) {
        lastSensorReadAt = now;

        readAirSensor();
        const bool soilReadOk = readSoilSensor();

        if (mqttClient.connected()) {
            publishAirStateIfValid();
            if (soilReadOk) {
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
