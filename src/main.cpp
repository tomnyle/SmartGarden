#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>
#include "app_config.h"

// ================= GLOBAL INSTANCES =================
WiFiClient espClient;
PubSubClient client(espClient);

// DHT22
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// RS485
#define RXD2 16
#define TXD2 17
#define MAX485_RE_DE 4
HardwareSerial RS485Serial(2);
ModbusMaster node;

// RELAY
#define RELAY_COUNT 8
int relayPins[RELAY_COUNT] = {5, 18, 19, 27, 32, 33, 25, 26};
const char* relayNames[RELAY_COUNT] = {"Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation", "Relay 7", "Relay 8"};
const char* relayObjectIds[RELAY_COUNT] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};
bool relayState[RELAY_COUNT] = {false};

// WiFi config
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// MQTT config
const char* mqtt_server = MQTT_BROKER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_user = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

const char* deviceId = "smartgarden";
const char* discoveryPrefix = "homeassistant";
const char* availabilityTopic = "smartgarden/status";
const char* currentCropTopic = "smartgarden/crop/current";

struct SensorState {
    float airTemp = 0.0f;
    float airHumidity = 0.0f;
    float soilMoisture = 0.0f;
    float soilTemp = 0.0f;
    float ph = 0.0f;
    uint16_t ec = 0;
    uint16_t nitrogen = 0;
    uint16_t phosphorus = 0;
    uint16_t potassium = 0;
};

SensorState sensorState;
char currentCrop[32] = "Demo Crop";
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 10000;

struct DiscoverySensorConfig {
    const char* objectId;
    const char* name;
    const char* stateTopic;
    const char* deviceClass;
    const char* stateClass;
    const char* unit;
};

// ================= RS485 CONTROL =================
void preTransmission() {
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_DE, LOW);
}

// ================= HELPERS =================
static bool publishRetained(const char* topic, const char* payload) {
    if (!client.connected()) {
        return false;
    }

    const bool published = client.publish(topic, payload, true);
    if (!published) {
        Serial.printf("[MQTT] Publish failed: %s\n", topic);
    }

    delay(20);
    return published;
}

static void buildDeviceBlock(char* buffer, size_t size) {
    snprintf(
        buffer,
        size,
        "{\"ids\":[\"%s\"],\"name\":\"Smart Garden\",\"mf\":\"DIY\",\"mdl\":\"ESP32 SmartGarden Controller\",\"sw\":\"%s\"}",
        deviceId,
        APP_VERSION
    );
}

static void publishDiscoveryMessage(const String& topic, const String& payload) {
    publishRetained(topic.c_str(), payload.c_str());
}

static void publishRelayState(uint8_t index) {
    char topic[48];
    snprintf(topic, sizeof(topic), "smartgarden/relay/%u/state", index + 1);
    publishRetained(topic, relayState[index] ? "ON" : "OFF");
}

static void publishCropState() {
    publishRetained(currentCropTopic, currentCrop);
}

static void publishSensorState() {
    char payload[32];

    snprintf(payload, sizeof(payload), "%.1f", sensorState.airTemp);
    publishRetained("smartgarden/sensors/air_temp", payload);

    snprintf(payload, sizeof(payload), "%.1f", sensorState.airHumidity);
    publishRetained("smartgarden/sensors/air_humidity", payload);

    snprintf(payload, sizeof(payload), "%.1f", sensorState.soilMoisture);
    publishRetained("smartgarden/sensors/soil_moisture", payload);

    snprintf(payload, sizeof(payload), "%.1f", sensorState.soilTemp);
    publishRetained("smartgarden/sensors/soil_temp", payload);

    snprintf(payload, sizeof(payload), "%.1f", sensorState.ph);
    publishRetained("smartgarden/sensors/ph", payload);

    snprintf(payload, sizeof(payload), "%u", sensorState.ec);
    publishRetained("smartgarden/sensors/ec", payload);

    snprintf(payload, sizeof(payload), "%u", sensorState.nitrogen);
    publishRetained("smartgarden/sensors/nitrogen", payload);

    snprintf(payload, sizeof(payload), "%u", sensorState.phosphorus);
    publishRetained("smartgarden/sensors/phosphorus", payload);

    snprintf(payload, sizeof(payload), "%u", sensorState.potassium);
    publishRetained("smartgarden/sensors/potassium", payload);
}

static void publishAllState() {
    publishSensorState();
    publishCropState();

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        publishRelayState(i);
    }
}

// ================= RELAY CONTROL =================
void setRelay(int index, bool state) {
    if (index < 0 || index >= RELAY_COUNT) return;

    relayState[index] = state;
    digitalWrite(relayPins[index], state ? LOW : HIGH);
    publishRelayState(index);

    Serial.printf("[Relay] Relay %d -> %s\n", index + 1, state ? "ON" : "OFF");
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.printf("[MQTT RX] %s = %s\n", topic, msg.c_str());

    for (int i = 0; i < RELAY_COUNT; i++) {
        String cmdTopic = "smartgarden/relay/" + String(i + 1) + "/set";
        if (String(topic) == cmdTopic) {
            if (msg == "ON") setRelay(i, true);
            else if (msg == "OFF") setRelay(i, false);
            return;
        }
    }

    if (String(topic) == "smartgarden/crop/set" && msg.length() > 0) {
        msg.toCharArray(currentCrop, sizeof(currentCrop));
        publishCropState();
        Serial.printf("[Crop] Current crop -> %s\n", currentCrop);
    }
}

// ================= PUBLISH DISCOVERY MESSAGES =================
void publishDiscoveryMessages() {
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");

    char deviceInfo[160];
    buildDeviceBlock(deviceInfo, sizeof(deviceInfo));

    const DiscoverySensorConfig sensors[] = {
        {"air_temp", "Air Temperature", "smartgarden/sensors/air_temp", "temperature", "measurement", "°C"},
        {"air_humidity", "Air Humidity", "smartgarden/sensors/air_humidity", "humidity", "measurement", "%"},
        {"soil_moisture", "Soil Moisture", "smartgarden/sensors/soil_moisture", "moisture", "measurement", "%"},
        {"soil_temp", "Soil Temperature", "smartgarden/sensors/soil_temp", "temperature", "measurement", "°C"},
        {"ph", "pH Value", "smartgarden/sensors/ph", nullptr, "measurement", nullptr},
        {"ec", "EC", "smartgarden/sensors/ec", nullptr, "measurement", "uS/cm"},
        {"nitrogen", "Nitrogen", "smartgarden/sensors/nitrogen", nullptr, "measurement", "mg/kg"},
        {"phosphorus", "Phosphorus", "smartgarden/sensors/phosphorus", nullptr, "measurement", "mg/kg"},
        {"potassium", "Potassium", "smartgarden/sensors/potassium", nullptr, "measurement", "mg/kg"},
        {"current_crop", "Current Crop", currentCropTopic, nullptr, nullptr, nullptr}
    };

    for (const DiscoverySensorConfig& sensor : sensors) {
        String topic = String(discoveryPrefix) + "/sensor/" + deviceId + "/" + sensor.objectId + "/config";
        String payload =
            String("{\"name\":\"") + sensor.name +
            "\",\"uniq_id\":\"" + deviceId + "_" + sensor.objectId +
            "\",\"object_id\":\"" + sensor.objectId +
            "\",\"stat_t\":\"" + sensor.stateTopic +
            "\",\"avty_t\":\"" + availabilityTopic +
            "\",\"pl_avail\":\"online\"" +
            ",\"pl_not_avail\":\"offline\"";

        if (sensor.deviceClass) {
            payload += String(",\"dev_cla\":\"") + sensor.deviceClass + "\"";
        }
        if (sensor.stateClass) {
            payload += String(",\"stat_cla\":\"") + sensor.stateClass + "\"";
        }
        if (sensor.unit) {
            payload += String(",\"unit_of_meas\":\"") + sensor.unit + "\"";
        }

        payload += String(",\"dev\":") + deviceInfo + "}";
        publishDiscoveryMessage(topic, payload);
    }

    for (int i = 0; i < RELAY_COUNT; i++) {
        String topic = String(discoveryPrefix) + "/switch/" + deviceId + "/" + relayObjectIds[i] + "/config";
        String payload =
            String("{\"name\":\"") + relayNames[i] +
            "\",\"uniq_id\":\"" + deviceId + "_" + relayObjectIds[i] +
            "\",\"object_id\":\"" + relayObjectIds[i] + "\"";

        payload += String(",\"stat_t\":\"smartgarden/relay/") + String(i + 1) + "/state\"";
        payload += String(",\"cmd_t\":\"smartgarden/relay/") + String(i + 1) + "/set\"";
        payload += ",\"pl_on\":\"ON\",\"pl_off\":\"OFF\"";
        payload += String(",\"avty_t\":\"") + availabilityTopic + "\"";
        payload += ",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"";
        payload += String(",\"dev\":") + deviceInfo + "}";
        publishDiscoveryMessage(topic, payload);
    }

    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

// ================= MQTT RECONNECT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");

        if (client.connect(deviceId, mqtt_user, mqtt_password, availabilityTopic, 1, true, "offline")) {
            Serial.println(" Connected!");

            publishRetained(availabilityTopic, "online");
            publishDiscoveryMessages();

            for (int i = 0; i < RELAY_COUNT; i++) {
                String topic = "smartgarden/relay/" + String(i + 1) + "/set";
                client.subscribe(topic.c_str());
            }
            client.subscribe("smartgarden/crop/set");

            publishAllState();
        } else {
            Serial.printf(" Failed (code=%d), retry in 3s\n", client.state());
            delay(3000);
        }
    }
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========== SmartGarden Startup ==========");

    Serial.println("[Setup] Initializing relays...");
    for (int i = 0; i < RELAY_COUNT; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH);
        relayState[i] = false;
    }
    Serial.println("[Setup] Relays initialized");

    Serial.println("[Setup] Initializing DHT22...");
    dht.begin();
    Serial.println("[Setup] DHT22 initialized");

    Serial.println("[Setup] Initializing RS485...");
    pinMode(MAX485_RE_DE, OUTPUT);
    digitalWrite(MAX485_RE_DE, LOW);
    RS485Serial.begin(4800, SERIAL_8N1, RXD2, TXD2);
    node.begin(1, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    Serial.println("[Setup] RS485 initialized");

    Serial.printf("[Setup] Connecting to WiFi: %s\n", ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WiFi] Failed to connect!");
    }

    Serial.printf("[Setup] Initializing MQTT: %s:%d\n", mqtt_server, mqtt_port);
    client.setBufferSize(1024);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    reconnect();
    lastSensorRead = millis() - SENSOR_READ_INTERVAL;

    Serial.println("========== Setup Complete ==========");
}

// ================= LOOP =================
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
    }

    if (!client.connected()) {
        reconnect();
    }

    client.loop();

    unsigned long now = millis();
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;

        float airTemp = dht.readTemperature();
        float airHum = dht.readHumidity();

        if (isnan(airTemp)) {
            Serial.println("[Sensor] DHT22 Temperature read failed!");
        } else {
            sensorState.airTemp = airTemp;
        }
        if (isnan(airHum)) {
            Serial.println("[Sensor] DHT22 Humidity read failed!");
        } else {
            sensorState.airHumidity = airHum;
        }

        sensorState.soilMoisture = 0.0f;
        sensorState.soilTemp = 0.0f;
        sensorState.ph = 0.0f;
        sensorState.ec = 0;
        sensorState.nitrogen = 0;
        sensorState.phosphorus = 0;
        sensorState.potassium = 0;

        if (client.connected()) {
            publishSensorState();
        }

        Serial.println("\n========== SENSOR DATA ==========");
        Serial.printf("Air Temp     : %.1f C\n", sensorState.airTemp);
        Serial.printf("Air Humidity : %.1f %%\n", sensorState.airHumidity);
        Serial.printf("Soil Moisture: %.1f %% (waiting for sensor)\n", sensorState.soilMoisture);
        Serial.printf("Soil Temp    : %.1f C (waiting for sensor)\n", sensorState.soilTemp);
        Serial.printf("pH           : %.1f (waiting for sensor)\n", sensorState.ph);
        Serial.printf("EC           : %u uS/cm (waiting for sensor)\n", sensorState.ec);
        Serial.printf("Nitrogen     : %u mg/kg (waiting for sensor)\n", sensorState.nitrogen);
        Serial.printf("Phosphorus   : %u mg/kg (waiting for sensor)\n", sensorState.phosphorus);
        Serial.printf("Potassium    : %u mg/kg (waiting for sensor)\n", sensorState.potassium);
        Serial.println("=================================");
    }

    delay(10);
}
