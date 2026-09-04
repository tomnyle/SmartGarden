#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>
#include "app_config.h"
#include "mqtt_service.h"
#include "sensor_manager.h"

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
bool relayState[RELAY_COUNT] = {false};

const char* deviceId = "smartgarden";
SensorSnapshot sensorSnapshot = {};

MQTTService mqttService(&client, deviceId);

// ================= RS485 CONTROL =================
void preTransmission() {
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_DE, LOW);
}

// ================= RELAY CONTROL =================
void setRelay(int index, bool state) {
    if (index < 0 || index >= RELAY_COUNT) return;

    relayState[index] = state;
    digitalWrite(relayPins[index], state ? LOW : HIGH);

    Serial.printf("[Relay] Relay %d -> %s\n", index + 1, state ? "ON" : "OFF");

    if (mqttService.isConnected()) {
        mqttService.publishRelayStatus(static_cast<uint8_t>(index), state);
    }
}

void onRelayCommand(uint8_t relayIndex, bool state) {
    setRelay(relayIndex, state);
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========== SmartGarden Startup ==========");

    // Initialize Relays
    Serial.println("[Setup] Initializing relays...");
    for (int i = 0; i < RELAY_COUNT; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH);
        relayState[i] = false;
    }
    Serial.println("[Setup] Relays initialized");

    // Initialize DHT22
    Serial.println("[Setup] Initializing DHT22...");
    dht.begin();
    Serial.println("[Setup] DHT22 initialized");

    // Initialize RS485
    Serial.println("[Setup] Initializing RS485...");
    pinMode(MAX485_RE_DE, OUTPUT);
    digitalWrite(MAX485_RE_DE, LOW);
    RS485Serial.begin(4800, SERIAL_8N1, RXD2, TXD2);
    node.begin(1, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    Serial.println("[Setup] RS485 initialized");

    // Connect WiFi
    Serial.printf("[Setup] Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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

    // Setup MQTT
    Serial.printf("[Setup] Initializing MQTT: %s:%d\n", MQTT_BROKER, MQTT_PORT);
    client.setServer(MQTT_BROKER, MQTT_PORT);
    mqttService.begin(MQTT_USERNAME, MQTT_PASSWORD);
    mqttService.setRelayCommandCallback(onRelayCommand);

    Serial.println("========== Setup Complete ==========");
}

// ================= LOOP =================
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL_MS = 10000; // 10 seconds

void loop() {
    // WiFi reconnect check
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
    }

    // Read sensors every SENSOR_READ_INTERVAL_MS
    unsigned long now = millis();
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
        lastSensorRead = now;

        float airTemp = dht.readTemperature();
        float airHum = dht.readHumidity();

        if (isnan(airTemp)) {
            airTemp = 0;
            Serial.println("[Sensor] DHT22 Temperature read failed!");
        }
        if (isnan(airHum)) {
            airHum = 0;
            Serial.println("[Sensor] DHT22 Humidity read failed!");
        }

        float moisture = 0.0;
        float soilTemp = 0.0;
        float ph = 0.0;
        uint16_t ec = 0;
        uint16_t n = 0;
        uint16_t p = 0;
        uint16_t k = 0;

        sensorSnapshot.airTemp = airTemp;
        sensorSnapshot.airHumidity = airHum;
        sensorSnapshot.soilMoisture = moisture;
        sensorSnapshot.soilTemp = soilTemp;
        sensorSnapshot.ph = ph;
        sensorSnapshot.ec = ec;
        sensorSnapshot.nitrogen = n;
        sensorSnapshot.phosphorus = p;
        sensorSnapshot.potassium = k;
        sensorSnapshot.timestamp = (now == 0) ? 1 : now;

        Serial.println("\n========== SENSOR DATA ==========");
        Serial.printf("Air Temp     : %.1f C\n", airTemp);
        Serial.printf("Air Humidity : %.1f %%\n", airHum);
        Serial.printf("Soil Moisture: %.1f %% (waiting for sensor)\n", moisture);
        Serial.printf("Soil Temp    : %.1f C (waiting for sensor)\n", soilTemp);
        Serial.printf("pH           : %.1f (waiting for sensor)\n", ph);
        Serial.printf("EC           : %u uS/cm (waiting for sensor)\n", ec);
        Serial.printf("Nitrogen     : %u mg/kg (waiting for sensor)\n", n);
        Serial.printf("Phosphorus   : %u mg/kg (waiting for sensor)\n", p);
        Serial.printf("Potassium    : %u mg/kg (waiting for sensor)\n", k);
        Serial.println("=================================");
    }

    mqttService.loop();

    delay(10);
}
