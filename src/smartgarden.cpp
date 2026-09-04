#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>
#include <cstring>

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
const char* relayStateTopicPattern = "smartgarden/relay/%d/state";

struct SensorDiscoveryConfig {
    const char* objectId;
    const char* name;
    const char* stateTopic;
    const char* unit;
    const char* deviceClass;
    const char* stateClass;
};

const SensorDiscoveryConfig sensorConfigs[] = {
    {"smartgarden_air_temp", "Air Temperature", "smartgarden/sensors/air_temp", "°C", "temperature", "measurement"},
    {"smartgarden_air_humidity", "Air Humidity", "smartgarden/sensors/air_humidity", "%", "humidity", "measurement"},
    {"smartgarden_soil_moisture", "Soil Moisture", "smartgarden/sensors/soil_moisture", "%", "moisture", "measurement"},
    {"smartgarden_soil_temp", "Soil Temperature", "smartgarden/sensors/soil_temp", "°C", "temperature", "measurement"},
    {"smartgarden_ph", "pH Value", "smartgarden/sensors/ph", "pH", nullptr, "measurement"},
    {"smartgarden_ec", "EC", "smartgarden/sensors/ec", "uS/cm", nullptr, "measurement"},
    {"smartgarden_nitrogen", "Nitrogen", "smartgarden/sensors/nitrogen", "mg/kg", nullptr, "measurement"},
    {"smartgarden_phosphorus", "Phosphorus", "smartgarden/sensors/phosphorus", "mg/kg", nullptr, "measurement"},
    {"smartgarden_potassium", "Potassium", "smartgarden/sensors/potassium", "mg/kg", nullptr, "measurement"},
};

// WiFi config
const char* ssid = "Le Danh";
const char* password = "123456789";

// MQTT config
const char* mqtt_server = "192.168.100.168";
const int mqtt_port = 1883;
const char* mqtt_user = "homer";
const char* mqtt_password = "Danh@@@1992";
const uint16_t mqttBufferSize = 1536;
const char* homeAssistantDeviceIdentifier = "smartgarden_esp32";

const char* deviceId = "smartgarden";
const char* discoveryPrefix = "homeassistant";

// ================= RS485 CONTROL =================
void preTransmission() {
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_DE, LOW);
}

// ================= RELAY CONTROL =================
void publishRelayStateOnly(int index) {
    if (index < 0 || index >= RELAY_COUNT) return;

    char topic[64];
    snprintf(topic, sizeof(topic), relayStateTopicPattern, index + 1);
    const char* payload = relayState[index] ? "ON" : "OFF";
    bool ok = client.publish(topic, payload, true);
    Serial.printf("[MQTT TX] Relay state %d (%s) -> %s (%s)\n",
                  index + 1, relayNames[index], topic, ok ? "OK" : "FAIL");
}

void setRelay(int index, bool state) {
    if (index < 0 || index >= RELAY_COUNT) return;
    
    relayState[index] = state;
    digitalWrite(relayPins[index], state ? LOW : HIGH);
    publishRelayStateOnly(index);
    
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
bool publishDiscoveryConfig(const String& topic, const String& payload, const char* label) {
    bool ok = client.publish(topic.c_str(), payload.c_str(), true);
    Serial.printf("  %s %s -> %s (%s)\n", ok ? "OK" : "FAIL", label, topic.c_str(), ok ? "retained" : "publish error");
    return ok;
}

String escapeJsonString(const char* value);

void appendJsonProperty(String& json, const char* key, const char* value) {
    if (value && value[0] != '\0') {
        if (!json.endsWith("{")) {
            json += ",";
        }
        json += "\"";
        json += key;
        json += "\":\"";
        json += escapeJsonString(value);
        json += "\"";
    }
}

String buildDeviceBlock() {
    return String("\"device\":{\"identifiers\":[\"") + homeAssistantDeviceIdentifier +
           "\"],\"name\":\"Smart Garden\",\"manufacturer\":\"DIY\",\"model\":\"ESP32\"}";
}

String escapeJsonString(const char* value) {
    String escaped = "";
    if (!value) return escaped;
    escaped.reserve(strlen(value) + 8);

    for (const char* p = value; *p != '\0'; ++p) {
        char c = *p;
        if (c == '"' || c == '\\') {
            escaped += '\\';
            escaped += c;
        } else if (c == '\n') {
            escaped += "\\n";
        } else if (c == '\r') {
            escaped += "\\r";
        } else if (c == '\t') {
            escaped += "\\t";
        } else {
            escaped += c;
        }
    }

    return escaped;
}

void publishSensorDiscovery(const SensorDiscoveryConfig& config) {
    String payload = "{";
    payload.reserve(512);
    appendJsonProperty(payload, "name", config.name);
    appendJsonProperty(payload, "state_topic", config.stateTopic);
    appendJsonProperty(payload, "unique_id", config.objectId);
    appendJsonProperty(payload, "unit_of_measurement", config.unit);
    appendJsonProperty(payload, "device_class", config.deviceClass);
    appendJsonProperty(payload, "state_class", config.stateClass);
    payload += ",";
    payload += buildDeviceBlock();
    payload += "}";

    String topic = String(discoveryPrefix) + "/sensor/" + config.objectId + "/config";
    publishDiscoveryConfig(topic, payload, config.name);
}

void publishRelayDiscovery(int index) {
    String objectId = "smartgarden_relay_" + String(index + 1);
    String stateTopic = "smartgarden/relay/" + String(index + 1) + "/state";
    String commandTopic = "smartgarden/relay/" + String(index + 1) + "/set";

    String payload = "{";
    payload.reserve(512);
    appendJsonProperty(payload, "name", relayNames[index]);
    appendJsonProperty(payload, "state_topic", stateTopic.c_str());
    appendJsonProperty(payload, "command_topic", commandTopic.c_str());
    appendJsonProperty(payload, "unique_id", objectId.c_str());
    appendJsonProperty(payload, "payload_on", "ON");
    appendJsonProperty(payload, "payload_off", "OFF");
    payload += ",";
    payload += buildDeviceBlock();
    payload += "}";

    String topic = String(discoveryPrefix) + "/switch/" + objectId + "/config";
    String label = "Relay " + String(index + 1) + ": " + relayNames[index];
    publishDiscoveryConfig(topic, payload, label.c_str());
}

void publishDiscoveryMessages() {
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");
        
    for (const auto& sensorConfig : sensorConfigs) {
        publishSensorDiscovery(sensorConfig);
    }

    for (int i = 0; i < RELAY_COUNT; i++) {
        publishRelayDiscovery(i);
    }
    
    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

// ================= MQTT RECONNECT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");
        
        if (client.connect(deviceId, mqtt_user, mqtt_password)) {
            Serial.println(" Connected!");
            
            // Publish discovery messages every time we reconnect
            publishDiscoveryMessages();
            
            // Subscribe to relay control topics
            for (int i = 0; i < RELAY_COUNT; i++) {
                String topic = "smartgarden/relay/" + String(i + 1) + "/set";
                client.subscribe(topic.c_str());
            }
            
            // Publish initial relay states
            for (int i = 0; i < RELAY_COUNT; i++) {
                publishRelayStateOnly(i);
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
    client.setBufferSize(mqttBufferSize);
    client.setCallback(callback);
    reconnect();
    
    Serial.println("========== Setup Complete ==========\n");
}

// ================= LOOP =================
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 10000; // 10 seconds

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
    
    // Read sensors every SENSOR_READ_INTERVAL
    unsigned long now = millis();
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;
        
        // ===== READ REAL SENSOR DATA =====
        
        // Read DHT22 (real data)
        float airTemp = dht.readTemperature();
        float airHum = dht.readHumidity();
        
        // Handle DHT read errors
        if (isnan(airTemp)) {
            airTemp = 0;
            Serial.println("[Sensor] DHT22 Temperature read failed!");
        }
        if (isnan(airHum)) {
            airHum = 0;
            Serial.println("[Sensor] DHT22 Humidity read failed!");
        }
        
        // Other sensors = 0 (waiting for hardware to be connected)
        float moisture = 0.0;
        float soilTemp = 0.0;
        float ph = 0.0;
        uint16_t ec = 0;
        uint16_t n = 0;
        uint16_t p = 0;
        uint16_t k = 0;
        
        // Publish sensor data to MQTT
        if (client.connected()) {
            client.publish("smartgarden/sensors/air_temp", String(airTemp, 1).c_str(), true);
            client.publish("smartgarden/sensors/air_humidity", String(airHum, 1).c_str(), true);
            client.publish("smartgarden/sensors/soil_moisture", String(moisture, 1).c_str(), true);
            client.publish("smartgarden/sensors/soil_temp", String(soilTemp, 1).c_str(), true);
            client.publish("smartgarden/sensors/ph", String(ph, 1).c_str(), true);
            client.publish("smartgarden/sensors/ec", String(ec).c_str(), true);
            client.publish("smartgarden/sensors/nitrogen", String(n).c_str(), true);
            client.publish("smartgarden/sensors/phosphorus", String(p).c_str(), true);
            client.publish("smartgarden/sensors/potassium", String(k).c_str(), true);
        }
        
        // Print to serial
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
        Serial.println("=================================\n");
    }
    
    delay(10);
}
