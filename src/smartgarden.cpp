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
const char* relayNames[RELAY_COUNT] = {"Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation", "Relay7", "Relay8"};
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

// Flags
bool discoveryPublished = false;

const uint16_t SOIL_SENSOR_START_REGISTER = 0x0000;
const uint16_t SOIL_SENSOR_REGISTER_COUNT = 8;

struct AirSensorReadings {
    float temperature;
    float humidity;
};

struct SoilSensorReadings {
    float moisture;
    float temperature;
    float ph;
    uint16_t ec;
    uint16_t nitrogen;
    uint16_t phosphorus;
    uint16_t potassium;
};

// ================= RS485 CONTROL =================
void preTransmission() {
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_DE, LOW);
}

float decodeUnsignedTenths(uint16_t rawValue) {
    return rawValue / 10.0f;
}

float decodeSignedTenths(uint16_t rawValue) {
    return static_cast<int16_t>(rawValue) / 10.0f;
}

bool readAirSensor(AirSensorReadings& readings) {
    readings.humidity = dht.readHumidity();
    readings.temperature = dht.readTemperature();

    if (isnan(readings.humidity) || isnan(readings.temperature)) {
        Serial.println("[Sensor] DHT22 read failed");
        return false;
    }

    return true;
}

bool readSoilSensor(SoilSensorReadings& readings) {
    while (RS485Serial.available()) {
        RS485Serial.read();
    }

    uint8_t result = node.readHoldingRegisters(SOIL_SENSOR_START_REGISTER, SOIL_SENSOR_REGISTER_COUNT);
    if (result != ModbusMaster::ku8MBSuccess) {
        Serial.printf("[Sensor] RS485 Modbus read failed (code=%u)\n", result);
        return false;
    }

    readings.moisture = decodeUnsignedTenths(node.getResponseBuffer(0));
    readings.temperature = decodeSignedTenths(node.getResponseBuffer(1));
    readings.ph = decodeUnsignedTenths(node.getResponseBuffer(2));
    readings.ec = node.getResponseBuffer(3);
    readings.nitrogen = node.getResponseBuffer(4);
    readings.phosphorus = node.getResponseBuffer(5);
    readings.potassium = node.getResponseBuffer(6);
    return true;
}

void publishUnavailable(const char* topic) {
    client.publish(topic, "unavailable", true);
}

// ================= RELAY CONTROL =================
void setRelay(int index, bool state) {
    if (index < 0 || index >= RELAY_COUNT) return;
    
    relayState[index] = state;
    digitalWrite(relayPins[index], state ? LOW : HIGH);
    
    String stateTopic = "smartgarden/relay/" + String(index + 1) + "/state";
    client.publish(stateTopic.c_str(), state ? "ON" : "OFF", true);
    
    Serial.printf("[Relay] Relay %d -> %s\n", index + 1, state ? "ON" : "OFF");
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    
    Serial.printf("[MQTT RX] %s = %s\n", topic, msg.c_str());
    
    // Handle relay control
    for (int i = 0; i < RELAY_COUNT; i++) {
        String cmdTopic = "smartgarden/relay/" + String(i + 1) + "/set";
        if (String(topic) == cmdTopic) {
            if (msg == "ON") setRelay(i, true);
            else if (msg == "OFF") setRelay(i, false);
        }
    }
}

// ================= PUBLISH DISCOVERY MESSAGES =================
void publishDiscoveryMessages() {
    if (discoveryPublished) return;
    
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");
    
    char buffer[1024];
    
    // Device info (used by all entities)
    const char* deviceInfo = R"(,"device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"})";
    
    // ===== SENSORS =====
    
    // Air Temperature
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Temperature\",\"unique_id\":\"smartgarden_air_temp\",\"state_topic\":\"smartgarden/sensors/air_temp\",\"unit_of_measurement\":\"C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_air_temp/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Air Temperature");
    
    // Air Humidity
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Humidity\",\"unique_id\":\"smartgarden_air_humidity\",\"state_topic\":\"smartgarden/sensors/air_humidity\",\"unit_of_measurement\":\"%%\",\"device_class\":\"humidity\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_air_humidity/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Air Humidity");
    
    // Soil Moisture
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Moisture\",\"unique_id\":\"smartgarden_soil_moisture\",\"state_topic\":\"smartgarden/sensors/soil_moisture\",\"unit_of_measurement\":\"%%\",\"device_class\":\"moisture\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_soil_moisture/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Soil Moisture");
    
    // Soil Temperature
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Temperature\",\"unique_id\":\"smartgarden_soil_temp\",\"state_topic\":\"smartgarden/sensors/soil_temp\",\"unit_of_measurement\":\"C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_soil_temp/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Soil Temperature");
    
    // pH
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"pH Value\",\"unique_id\":\"smartgarden_ph\",\"state_topic\":\"smartgarden/sensors/ph\",\"unit_of_measurement\":\"pH\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_ph/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK pH Value");
    
    // EC
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"EC\",\"unique_id\":\"smartgarden_ec\",\"state_topic\":\"smartgarden/sensors/ec\",\"unit_of_measurement\":\"uS/cm\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_ec/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK EC");
    
    // Nitrogen
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Nitrogen\",\"unique_id\":\"smartgarden_nitrogen\",\"state_topic\":\"smartgarden/sensors/nitrogen\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_nitrogen/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Nitrogen");
    
    // Phosphorus
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Phosphorus\",\"unique_id\":\"smartgarden_phosphorus\",\"state_topic\":\"smartgarden/sensors/phosphorus\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_phosphorus/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Phosphorus");
    
    // Potassium
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Potassium\",\"unique_id\":\"smartgarden_potassium\",\"state_topic\":\"smartgarden/sensors/potassium\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    client.publish((String(discoveryPrefix) + "/sensor/smartgarden_potassium/config").c_str(), buffer, true);
    delay(100);
    Serial.println("  OK Potassium");
    
    // ===== SWITCHES (RELAYS) =====
    
    for (int i = 0; i < RELAY_COUNT; i++) {
        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"unique_id\":\"smartgarden_relay_%d\",\"state_topic\":\"smartgarden/relay/%d/state\",\"command_topic\":\"smartgarden/relay/%d/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"%s}",
            relayNames[i], i + 1, i + 1, i + 1, deviceInfo);
        
        String switchTopic = String(discoveryPrefix) + "/switch/smartgarden_relay_" + String(i + 1) + "/config";
        client.publish(switchTopic.c_str(), buffer, true);
        delay(100);
        Serial.printf("  OK Relay %d: %s\n", i + 1, relayNames[i]);
    }
    
    Serial.println("[MQTT Discovery] All discovery messages published!\n");
    discoveryPublished = true;
}

// ================= MQTT RECONNECT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");
        
        if (client.connect(deviceId, mqtt_user, mqtt_password)) {
            Serial.println(" Connected!");
            
            // Publish discovery messages
            publishDiscoveryMessages();
            
            // Subscribe to relay control topics
            for (int i = 0; i < RELAY_COUNT; i++) {
                String topic = "smartgarden/relay/" + String(i + 1) + "/set";
                client.subscribe(topic.c_str());
            }
            
            // Publish initial relay states
            for (int i = 0; i < RELAY_COUNT; i++) {
                setRelay(i, relayState[i]);
            }
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
    
    // Setup MQTT
    Serial.printf("[Setup] Initializing MQTT: %s:%d\n", mqtt_server, mqtt_port);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    reconnect();
    
    Serial.println("========== Setup Complete ==========\n");
}

// ================= LOOP =================
unsigned long lastSensorRead = 0;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 10000; // 10 seconds

void loop() {
    // WiFi reconnect check
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
    }
    
    // MQTT reconnect check
    if (!client.connected()) {
        reconnect();
    }
    
    client.loop();
    
    // Read sensors every MQTT_PUBLISH_INTERVAL_MS
    unsigned long now = millis();
    if (now - lastSensorRead >= MQTT_PUBLISH_INTERVAL_MS) {
        lastSensorRead = now;

        AirSensorReadings airReadings = {};
        SoilSensorReadings soilReadings = {};
        bool airSensorOk = readAirSensor(airReadings);
        bool soilSensorOk = readSoilSensor(soilReadings);

        // Publish sensor data to MQTT
        if (client.connected()) {
            if (airSensorOk) {
                client.publish("smartgarden/sensors/air_temp", String(airReadings.temperature, 1).c_str(), true);
                client.publish("smartgarden/sensors/air_humidity", String(airReadings.humidity, 1).c_str(), true);
            } else {
                publishUnavailable("smartgarden/sensors/air_temp");
                publishUnavailable("smartgarden/sensors/air_humidity");
            }

            if (soilSensorOk) {
                client.publish("smartgarden/sensors/soil_moisture", String(soilReadings.moisture, 1).c_str(), true);
                client.publish("smartgarden/sensors/soil_temp", String(soilReadings.temperature, 1).c_str(), true);
                client.publish("smartgarden/sensors/ph", String(soilReadings.ph, 1).c_str(), true);
                client.publish("smartgarden/sensors/ec", String(soilReadings.ec).c_str(), true);
                client.publish("smartgarden/sensors/nitrogen", String(soilReadings.nitrogen).c_str(), true);
                client.publish("smartgarden/sensors/phosphorus", String(soilReadings.phosphorus).c_str(), true);
                client.publish("smartgarden/sensors/potassium", String(soilReadings.potassium).c_str(), true);
            } else {
                publishUnavailable("smartgarden/sensors/soil_moisture");
                publishUnavailable("smartgarden/sensors/soil_temp");
                publishUnavailable("smartgarden/sensors/ph");
                publishUnavailable("smartgarden/sensors/ec");
                publishUnavailable("smartgarden/sensors/nitrogen");
                publishUnavailable("smartgarden/sensors/phosphorus");
                publishUnavailable("smartgarden/sensors/potassium");
            }
        }

        // Print to serial
        Serial.println("\n========== SENSOR DATA (REAL) ==========");
        if (airSensorOk) {
            Serial.printf("Air Temp     : %.1f C\n", airReadings.temperature);
            Serial.printf("Air Humidity : %.1f %%\n", airReadings.humidity);
        } else {
            Serial.println("Air Temp     : READ FAILED");
            Serial.println("Air Humidity : READ FAILED");
        }

        if (soilSensorOk) {
            Serial.printf("Soil Moisture: %.1f %%\n", soilReadings.moisture);
            Serial.printf("Soil Temp    : %.1f C\n", soilReadings.temperature);
            Serial.printf("pH           : %.1f\n", soilReadings.ph);
            Serial.printf("EC           : %u uS/cm\n", soilReadings.ec);
            Serial.printf("Nitrogen     : %u mg/kg\n", soilReadings.nitrogen);
            Serial.printf("Phosphorus   : %u mg/kg\n", soilReadings.phosphorus);
            Serial.printf("Potassium    : %u mg/kg\n", soilReadings.potassium);
        } else {
            Serial.println("Soil Moisture: READ FAILED");
            Serial.println("Soil Temp    : READ FAILED");
            Serial.println("pH           : READ FAILED");
            Serial.println("EC           : READ FAILED");
            Serial.println("Nitrogen     : READ FAILED");
            Serial.println("Phosphorus   : READ FAILED");
            Serial.println("Potassium    : READ FAILED");
        }
        Serial.println("=================================\n");
    }
    
    delay(10);
}
