#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>

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
const char* ssid = "Le Danh";
const char* password = "123456789";

// MQTT config
const char* mqtt_server = "192.168.100.168";
const int mqtt_port = 1883;
const char* mqtt_user = "homer";
const char* mqtt_password = "Danh@@@1992";

const char* deviceId = "smartgarden";
const char* discoveryPrefix = "homeassistant";
const char* availabilityTopic = "smartgarden/status";

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
    
    String stateTopic = "smartgarden/relay/" + String(index + 1) + "/state";
    client.publish(stateTopic.c_str(), state ? "ON" : "OFF", true);
    
    Serial.printf("[Relay] Relay %d -> %s\n", index + 1, state ? "ON" : "OFF");
}

bool publishRetained(const String& topic, const String& payload) {
    bool ok = client.publish(topic.c_str(), payload.c_str(), true);
    Serial.printf("[MQTT] %s => %s\n", topic.c_str(), ok ? "OK" : "FAILED");
    return ok;
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
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");

    publishRetained(availabilityTopic, "online");

    // Compact payloads keep MQTT packets below PubSubClient limits.
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_air_temp/config",
        "{\"name\":\"Air Temperature\",\"unique_id\":\"smartgarden_air_temp\",\"state_topic\":\"smartgarden/sensors/air_temp\",\"unit_of_measurement\":\"C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_air_humidity/config",
        "{\"name\":\"Air Humidity\",\"unique_id\":\"smartgarden_air_humidity\",\"state_topic\":\"smartgarden/sensors/air_humidity\",\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_soil_moisture/config",
        "{\"name\":\"Soil Moisture\",\"unique_id\":\"smartgarden_soil_moisture\",\"state_topic\":\"smartgarden/sensors/soil_moisture\",\"unit_of_measurement\":\"%\",\"device_class\":\"moisture\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_soil_temp/config",
        "{\"name\":\"Soil Temperature\",\"unique_id\":\"smartgarden_soil_temp\",\"state_topic\":\"smartgarden/sensors/soil_temp\",\"unit_of_measurement\":\"C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_ph/config",
        "{\"name\":\"pH Value\",\"unique_id\":\"smartgarden_ph\",\"state_topic\":\"smartgarden/sensors/ph\",\"unit_of_measurement\":\"pH\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_ec/config",
        "{\"name\":\"EC\",\"unique_id\":\"smartgarden_ec\",\"state_topic\":\"smartgarden/sensors/ec\",\"unit_of_measurement\":\"uS/cm\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_nitrogen/config",
        "{\"name\":\"Nitrogen\",\"unique_id\":\"smartgarden_nitrogen\",\"state_topic\":\"smartgarden/sensors/nitrogen\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_phosphorus/config",
        "{\"name\":\"Phosphorus\",\"unique_id\":\"smartgarden_phosphorus\",\"state_topic\":\"smartgarden/sensors/phosphorus\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );
    publishRetained(
        String(discoveryPrefix) + "/sensor/smartgarden_potassium/config",
        "{\"name\":\"Potassium\",\"unique_id\":\"smartgarden_potassium\",\"state_topic\":\"smartgarden/sensors/potassium\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\",\"availability_topic\":\"smartgarden/status\"}"
    );

    for (int i = 0; i < RELAY_COUNT; i++) {
        String switchTopic = String(discoveryPrefix) + "/switch/smartgarden_relay_" + String(i + 1) + "/config";
        String switchPayload =
            "{\"name\":\"" + String(relayNames[i]) +
            "\",\"unique_id\":\"smartgarden_relay_" + String(i + 1) +
            "\",\"state_topic\":\"smartgarden/relay/" + String(i + 1) +
            "/state\",\"command_topic\":\"smartgarden/relay/" + String(i + 1) +
            "/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"availability_topic\":\"smartgarden/status\"}";
        publishRetained(switchTopic, switchPayload);
        delay(50);
    }
    
    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

// ================= MQTT RECONNECT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");
        
        if (client.connect(deviceId, mqtt_user, mqtt_password, availabilityTopic, 1, true, "offline")) {
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
                String stateTopic = "smartgarden/relay/" + String(i + 1) + "/state";
                client.publish(stateTopic.c_str(), relayState[i] ? "ON" : "OFF", true);
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
    client.setBufferSize(512);
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
