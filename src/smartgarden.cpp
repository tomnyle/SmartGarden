#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "app_config.h"
#include "pins.h"
#include "garden_profile.h"

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHT_TYPE);

enum OperationMode {
    MODE_MANUAL,
    MODE_AUTO,
    MODE_MONITOR
};

static OperationMode currentMode = MODE_MANUAL;
static bool relayState[NUM_RELAYS] = {false};
static float airTemp = 0.0f;
static float airHum = 0.0f;
static float soilMoisture = 0.0f;
static float soilTemp = 0.0f;
static float ph = 0.0f;
static uint16_t ec = 0;
static uint16_t nitrogen = 0;
static uint16_t phosphorus = 0;
static uint16_t potassium = 0;
static bool rs485DataValid = false;

static const char* deviceId = MQTT_DEVICE_ID;
static const char* relayNames[NUM_RELAYS] = {
    "Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation", "Relay7", "Relay8"
};
static const char* relayIds[NUM_RELAYS] = {
    "fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"
};
static const CropProfile* currentCrop = nullptr;

static unsigned long lastSensorRead = 0;
static unsigned long lastMqttConnectAttempt = 0;

static uint16_t modbusCRC16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static const char* modeToString(OperationMode mode)
{
    switch (mode) {
        case MODE_AUTO: return "auto";
        case MODE_MONITOR: return "monitor";
        case MODE_MANUAL:
        default:
            return "manual";
    }
}

static OperationMode parseMode(const char* value)
{
    if (strcmp(value, "auto") == 0) return MODE_AUTO;
    if (strcmp(value, "monitor") == 0) return MODE_MONITOR;
    return MODE_MANUAL;
}

static void publishRelayState(uint8_t relayIndex)
{
    if (!client.connected() || relayIndex >= NUM_RELAYS) {
        return;
    }

    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic), MQTT_RUNTIME_PREFIX "/relay/%u/state", relayIndex + 1);
    client.publish(stateTopic, relayState[relayIndex] ? "ON" : "OFF", true);
}

static void setRelay(uint8_t relayIndex, bool state)
{
    if (relayIndex >= NUM_RELAYS) {
        return;
    }

    relayState[relayIndex] = state;
    digitalWrite(RELAY_PINS[relayIndex], state ? LOW : HIGH); // active-low
    publishRelayState(relayIndex);
}

static void setAllRelays(bool state)
{
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        setRelay(i, state);
    }
}

static void publishMode()
{
    if (client.connected()) {
        client.publish(MQTT_TOPIC_MODE_STATE, modeToString(currentMode), true);
    }
}

static void publishCropState()
{
    if (client.connected() && currentCrop) {
        client.publish(MQTT_TOPIC_CROP_STATE, currentCrop->name, true);
    }
}

static void publishCropList()
{
    if (!client.connected()) {
        return;
    }

    StaticJsonDocument<1024> doc;
    JsonArray options = doc.createNestedArray("options");
    uint8_t cropCount = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(cropCount);
    for (uint8_t i = 0; i < cropCount; i++) {
        options.add(crops[i].name);
    }

    char payload[1024];
    size_t size = serializeJson(doc, payload, sizeof(payload));
    if (size > 0) {
        client.publish(MQTT_TOPIC_CROP_LIST, payload, true);
    }
}

static void publishSensorData()
{
    if (!client.connected()) {
        return;
    }

    char payload[32];

    snprintf(payload, sizeof(payload), "%.1f", airTemp);
    client.publish(MQTT_TOPIC_AIR_TEMP, payload, true);

    snprintf(payload, sizeof(payload), "%.1f", airHum);
    client.publish(MQTT_TOPIC_AIR_HUMIDITY, payload, true);

    snprintf(payload, sizeof(payload), "%.1f", soilMoisture);
    client.publish(MQTT_TOPIC_SOIL_MOISTURE, payload, true);

    snprintf(payload, sizeof(payload), "%.1f", soilTemp);
    client.publish(MQTT_TOPIC_SOIL_TEMP, payload, true);

    snprintf(payload, sizeof(payload), "%.2f", ph);
    client.publish(MQTT_TOPIC_PH, payload, true);

    snprintf(payload, sizeof(payload), "%u", ec);
    client.publish(MQTT_TOPIC_EC, payload, true);

    snprintf(payload, sizeof(payload), "%u", nitrogen);
    client.publish(MQTT_TOPIC_NITROGEN, payload, true);

    snprintf(payload, sizeof(payload), "%u", phosphorus);
    client.publish(MQTT_TOPIC_PHOSPHORUS, payload, true);

    snprintf(payload, sizeof(payload), "%u", potassium);
    client.publish(MQTT_TOPIC_POTASSIUM, payload, true);
}

static void readRS485Sensors()
{
    rs485DataValid = false;

    uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x08, 0x44, 0x09};

    while (Serial2.available() > 0) {
        Serial2.read();
    }

    digitalWrite(RS485_DE, HIGH);
    delay(10);
    Serial2.write(request, sizeof(request));
    Serial2.flush();
    digitalWrite(RS485_DE, LOW);

    unsigned long waitStart = millis();
    while (Serial2.available() < 3 && (millis() - waitStart) < RS485_READ_TIMEOUT) {
        delay(1);
    }
    if (Serial2.available() < 3) {
        return;
    }

    uint8_t response[32];
    size_t headerRead = Serial2.readBytes(response, 3);
    if (headerRead != 3) {
        return;
    }

    if (response[0] != 0x01 || response[1] != 0x03) {
        return;
    }

    uint8_t payloadBytes = response[2];
    uint8_t expectedLength = payloadBytes + 5;
    if (payloadBytes < 14 || expectedLength > sizeof(response)) {
        return;
    }

    size_t remainingLength = expectedLength - 3;
    size_t bodyRead = Serial2.readBytes(response + 3, remainingLength);
    if (bodyRead != remainingLength) {
        return;
    }

    uint16_t receivedCrc = response[expectedLength - 2] | (response[expectedLength - 1] << 8);
    uint16_t calculatedCrc = modbusCRC16(response, expectedLength - 2);
    if (receivedCrc != calculatedCrc) {
        return;
    }

    float parsedSoilMoisture = (response[3] << 8 | response[4]) / 10.0f;
    int16_t rawSoilTemp = static_cast<int16_t>((response[5] << 8) | response[6]);
    float parsedSoilTemp = rawSoilTemp / 10.0f;
    float parsedPh = (response[7] << 8 | response[8]) / 10.0f;
    uint16_t parsedEc = (response[9] << 8 | response[10]);
    uint16_t parsedNitrogen = (response[11] << 8 | response[12]);
    uint16_t parsedPhosphorus = (response[13] << 8 | response[14]);
    uint16_t parsedPotassium = (response[15] << 8 | response[16]);

    if (parsedSoilMoisture < 0.0f || parsedSoilMoisture > 100.0f ||
        parsedSoilTemp < -40.0f || parsedSoilTemp > 85.0f ||
        parsedPh < 0.0f || parsedPh > 14.0f ||
        parsedEc > 20000 || parsedNitrogen > 5000 ||
        parsedPhosphorus > 5000 || parsedPotassium > 5000) {
        return;
    }

    soilMoisture = parsedSoilMoisture;
    soilTemp = parsedSoilTemp;
    ph = parsedPh;
    ec = parsedEc;
    nitrogen = parsedNitrogen;
    phosphorus = parsedPhosphorus;
    potassium = parsedPotassium;

    rs485DataValid = true;
}

static void applyAutoControl()
{
    if (!currentCrop) {
        return;
    }

    setRelay(0, airTemp > currentCrop->temperature.max || airHum > currentCrop->airHumidity.max); // fan
    setRelay(1, airTemp < currentCrop->temperature.min);      // heater
    setRelay(2, airTemp > currentCrop->temperature.max);      // cooler
    setRelay(3, airHum < currentCrop->airHumidity.min);       // humidifier
    setRelay(4, airHum > currentCrop->airHumidity.max);       // dehumidifier

    if (!rs485DataValid) {
        setRelay(IRRIGATION_RELAY_INDEX, false);
        return;
    }

    if (soilMoisture < currentCrop->soilHumidity.min) {
        setRelay(IRRIGATION_RELAY_INDEX, true);
    } else {
        setRelay(IRRIGATION_RELAY_INDEX, false);
    }
}

static void publishDiscoveryMessages()
{
    if (!client.connected()) {
        return;
    }

    StaticJsonDocument<768> doc;
    char payload[1024];
    char topic[160];

    auto setDevice = [&](JsonDocument& json) {
        json["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
        json["payload_available"] = "online";
        json["payload_not_available"] = "offline";
        json["device"]["identifiers"][0] = MQTT_DEVICE_ID;
        json["device"]["name"] = MQTT_DEVICE_NAME;
        json["device"]["manufacturer"] = "DIY";
        json["device"]["model"] = "ESP32 SmartGarden Controller";
    };

    const struct {
        const char* name;
        const char* id;
        const char* stateTopic;
        const char* unit;
        const char* deviceClass;
        const char* icon;
    } sensors[] = {
        {"Air Temperature", "smartgarden_air_temp", MQTT_TOPIC_AIR_TEMP, "°C", "temperature", "mdi:thermometer"},
        {"Air Humidity", "smartgarden_air_humidity", MQTT_TOPIC_AIR_HUMIDITY, "%", "humidity", "mdi:water-percent"},
        {"Soil Moisture", "smartgarden_soil_moisture", MQTT_TOPIC_SOIL_MOISTURE, "%", nullptr, "mdi:water"},
        {"Soil Temperature", "smartgarden_soil_temp", MQTT_TOPIC_SOIL_TEMP, "°C", "temperature", "mdi:thermometer"},
        {"pH Value", "smartgarden_ph", MQTT_TOPIC_PH, "pH", nullptr, "mdi:test-tube"},
        {"Electrical Conductivity", "smartgarden_ec", MQTT_TOPIC_EC, "µS/cm", nullptr, "mdi:flash"},
        {"Nitrogen", "smartgarden_nitrogen", MQTT_TOPIC_NITROGEN, "mg/kg", nullptr, "mdi:leaf"},
        {"Phosphorus", "smartgarden_phosphorus", MQTT_TOPIC_PHOSPHORUS, "mg/kg", nullptr, "mdi:leaf"},
        {"Potassium", "smartgarden_potassium", MQTT_TOPIC_POTASSIUM, "mg/kg", nullptr, "mdi:leaf"}
    };

    for (const auto& sensor : sensors) {
        doc.clear();
        doc["name"] = sensor.name;
        doc["unique_id"] = sensor.id;
        doc["state_topic"] = sensor.stateTopic;
        doc["unit_of_measurement"] = sensor.unit;
        doc["icon"] = sensor.icon;
        doc["state_class"] = "measurement";
        if (sensor.deviceClass) {
            doc["device_class"] = sensor.deviceClass;
        }
        setDevice(doc);
        serializeJson(doc, payload);
        snprintf(topic, sizeof(topic), HA_DISCOVERY_PREFIX "/sensor/%s/config", sensor.id);
        client.publish(topic, payload, true);
        delay(30);
    }

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        doc.clear();
        char stateTopic[64];
        char cmdTopic[64];
        snprintf(stateTopic, sizeof(stateTopic), MQTT_RUNTIME_PREFIX "/relay/%u/state", i + 1);
        snprintf(cmdTopic, sizeof(cmdTopic), MQTT_RUNTIME_PREFIX "/relay/%u/set", i + 1);

        doc["name"] = relayNames[i];
        doc["unique_id"] = String("smartgarden_") + relayIds[i];
        doc["state_topic"] = stateTopic;
        doc["command_topic"] = cmdTopic;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["state_on"] = "ON";
        doc["state_off"] = "OFF";
        setDevice(doc);

        serializeJson(doc, payload);
        snprintf(topic, sizeof(topic), HA_DISCOVERY_PREFIX "/switch/smartgarden_%s/config", relayIds[i]);
        client.publish(topic, payload, true);
        delay(30);
    }

    doc.clear();
    doc["name"] = "Crop Profile";
    doc["unique_id"] = "smartgarden_crop";
    doc["state_topic"] = MQTT_TOPIC_CROP_STATE;
    doc["command_topic"] = MQTT_TOPIC_CROP_SELECT;
    JsonArray cropOptions = doc.createNestedArray("options");
    uint8_t cropCount = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(cropCount);
    for (uint8_t i = 0; i < cropCount; i++) {
        cropOptions.add(crops[i].name);
    }
    setDevice(doc);
    serializeJson(doc, payload);
    client.publish(HA_DISCOVERY_PREFIX "/select/smartgarden_crop/config", payload, true);

    doc.clear();
    doc["name"] = "Operation Mode";
    doc["unique_id"] = "smartgarden_operation_mode";
    doc["state_topic"] = MQTT_TOPIC_MODE_STATE;
    doc["command_topic"] = MQTT_TOPIC_MODE_SET;
    JsonArray modeOptions = doc.createNestedArray("options");
    modeOptions.add("manual");
    modeOptions.add("auto");
    modeOptions.add("monitor");
    setDevice(doc);
    serializeJson(doc, payload);
    client.publish(HA_DISCOVERY_PREFIX "/select/smartgarden_operation_mode/config", payload, true);
}

static void subscribeTopics()
{
    for (uint8_t i = 1; i <= NUM_RELAYS; i++) {
        char cmdTopic[64];
        snprintf(cmdTopic, sizeof(cmdTopic), MQTT_RUNTIME_PREFIX "/relay/%u/set", i);
        client.subscribe(cmdTopic);
    }

    client.subscribe(MQTT_TOPIC_CROP_SELECT);
    client.subscribe(MQTT_TOPIC_MODE_SET);
}

static void handleRelayCommand(uint8_t relayIndex, const char* payload)
{
    if (relayIndex >= NUM_RELAYS) {
        return;
    }

    if (currentMode == MODE_AUTO || currentMode == MODE_MONITOR) {
        Serial.printf("[Mode] Ignored manual relay command in mode '%s'\n", modeToString(currentMode));
        return;
    }

    if (strcmp(payload, "ON") == 0) {
        setRelay(relayIndex, true);
    } else if (strcmp(payload, "OFF") == 0) {
        setRelay(relayIndex, false);
    }
}

static void handleCropSelect(const char* payload)
{
    const CropProfile* selected = CropProfileStore::getCropByName(payload);
    if (!selected) {
        return;
    }

    currentCrop = selected;
    publishCropState();
}

static void setMode(OperationMode mode)
{
    OperationMode previousMode = currentMode;
    currentMode = mode;

    if (currentMode == MODE_MONITOR) {
        setAllRelays(false);
    } else if (currentMode == MODE_MANUAL && previousMode != MODE_MANUAL) {
        setAllRelays(false);
    }

    publishMode();
}

static void callback(char* topic, byte* payload, unsigned int length)
{
    char message[MQTT_BUFFER_SIZE];
    if (length >= sizeof(message)) {
        Serial.println("[MQTT] Command payload too long, ignored");
        return;
    }
    memcpy(message, payload, length);
    message[length] = '\0';

    String topicStr(topic);

    if (topicStr.startsWith(MQTT_RUNTIME_PREFIX "/relay/") && topicStr.endsWith("/set")) {
        int relayIndex = topicStr.substring(String(MQTT_RUNTIME_PREFIX "/relay/").length(), topicStr.lastIndexOf('/')).toInt() - 1;
        if (relayIndex >= 0 && relayIndex < NUM_RELAYS) {
            handleRelayCommand(static_cast<uint8_t>(relayIndex), message);
        }
        return;
    }

    if (topicStr == MQTT_TOPIC_CROP_SELECT) {
        handleCropSelect(message);
        return;
    }

    if (topicStr == MQTT_TOPIC_MODE_SET) {
        setMode(parseMode(message));
    }
}

static bool reconnect()
{
    if (client.connected()) {
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    unsigned long now = millis();
    if (now - lastMqttConnectAttempt < MQTT_RECONNECT_INTERVAL) {
        return false;
    }
    lastMqttConnectAttempt = now;

    if (client.connect(deviceId, MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC_AVAILABILITY, 0, true, "offline")) {
        client.publish(MQTT_TOPIC_AVAILABILITY, "online", true);
        subscribeTopics();
        publishDiscoveryMessages();
        publishMode();
        publishCropState();
        publishCropList();
        for (uint8_t i = 0; i < NUM_RELAYS; i++) {
            publishRelayState(i);
        }
        return true;
    }

    return false;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    CropProfileStore::initialize();
    currentCrop = CropProfileStore::getCropById(DEFAULT_CROP_ID);
    if (!currentCrop) {
        currentCrop = CropProfileStore::getCropById(1);
    }

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH);
    }

    dht.begin();

    Serial2.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX, RS485_TX);
    Serial2.setTimeout(RS485_READ_TIMEOUT);
    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT) {
        delay(500);
    }

    client.setServer(MQTT_BROKER, MQTT_PORT);
    client.setBufferSize(MQTT_BUFFER_SIZE);
    client.setCallback(callback);

    reconnect();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        delay(500);
    }

    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long now = millis();
    if (now - lastSensorRead < SENSOR_READ_INTERVAL) {
        delay(10);
        return;
    }
    lastSensorRead = now;

    float dhtTemp = dht.readTemperature();
    float dhtHum = dht.readHumidity();

    if (!isnan(dhtTemp)) {
        airTemp = dhtTemp;
    }
    if (!isnan(dhtHum)) {
        airHum = dhtHum;
    }

    readRS485Sensors();

    if (currentMode == MODE_AUTO) {
        applyAutoControl();
    } else if (currentMode == MODE_MONITOR) {
        setAllRelays(false);
    }

    publishSensorData();
}
