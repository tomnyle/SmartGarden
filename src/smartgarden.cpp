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

// ================= RS485 CONTROL =================
void preTransmission() {
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_DE, LOW);
}

// ================= MQTT HELPERS =================
static void publishDiscovery(const char* topic, const char* payload) {
    client.publish(topic, payload, true);
    delay(50);
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

    const char* deviceInfo = R"(\"device\":{\"identifiers\":[\"smartgarden_esp32\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32\",\"name\":\"Smart Garden\")";
    char buffer[1024];

    auto publishSensor = [&](const char* topic, const char* name, const char* uniqueId, const char* stateTopic, const char* unit, const char* deviceClass, const char* extra = "") {
        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"unique_id\":\"%s\",\"state_topic\":\"%s\"%s%s%s,\"state_class\":\"measurement\",\"device\":{\"identifiers\":[\"smartgarden_esp32\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32\",\"name\":\"Smart Garden\"}}",
            name,
            uniqueId,
            stateTopic,
            (unit && strlen(unit)) ? ",\"unit_of_measurement\":\"" : "",
            (unit && strlen(unit)) ? unit : "",
            (unit && strlen(unit)) ? "\"" : "");

        String payload = buffer;
        if (deviceClass && strlen(deviceClass)) {
            size_t pos = payload.find("\"state_topic\"");
            if (pos != String::npos) {
                payload = String("{\"name\":\"") + name +
                    "\",\"unique_id\":\"" + uniqueId +
                    "\",\"state_topic\":\"" + stateTopic +
                    "\",\"device_class\":\"" + deviceClass +
                    "\",\"state_class\":\"measurement\"" +
                    ((unit && strlen(unit)) ? String(",\"unit_of_measurement\":\"") + unit + "\"" : "") +
                    ",\"device\":{\"identifiers\":[\"smartgarden_esp32\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32\",\"name\":\"Smart Garden\"}}";
            }
        }
        publishDiscovery(topic, payload.c_str());
    };

    publishDiscovery("homeassistant/sensor/smartgarden_air_temp/config", R"({"name":"Air Temperature","unique_id":"smartgarden_air_temp","state_topic":"smartgarden/sensors/air_temp","device_class":"temperature","state_class":"measurement","unit_of_measurement":"°C","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_air_humidity/config", R"({"name":"Air Humidity","unique_id":"smartgarden_air_humidity","state_topic":"smartgarden/sensors/air_humidity","device_class":"humidity","state_class":"measurement","unit_of_measurement":"%","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_soil_moisture/config", R"({"name":"Soil Moisture","unique_id":"smartgarden_soil_moisture","state_topic":"smartgarden/sensors/soil_moisture","device_class":"moisture","state_class":"measurement","unit_of_measurement":"%","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_soil_temp/config", R"({"name":"Soil Temperature","unique_id":"smartgarden_soil_temp","state_topic":"smartgarden/sensors/soil_temp","device_class":"temperature","state_class":"measurement","unit_of_measurement":"°C","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_ph/config", R"({"name":"pH Value","unique_id":"smartgarden_ph","state_topic":"smartgarden/sensors/ph","state_class":"measurement","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_ec/config", R"({"name":"EC","unique_id":"smartgarden_ec","state_topic":"smartgarden/sensors/ec","state_class":"measurement","unit_of_measurement":"uS/cm","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_nitrogen/config", R"({"name":"Nitrogen","unique_id":"smartgarden_nitrogen","state_topic":"smartgarden/sensors/nitrogen","state_class":"measurement","unit_of_measurement":"mg/kg","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_phosphorus/config", R"({"name":"Phosphorus","unique_id":"smartgarden_phosphorus","state_topic":"smartgarden/sensors/phosphorus","state_class":"measurement","unit_of_measurement":"mg/kg","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");
    publishDiscovery("homeassistant/sensor/smartgarden_potassium/config", R"({"name":"Potassium","unique_id":"smartgarden_potassium","state_topic":"smartgarden/sensors/potassium","state_class":"measurement","unit_of_measurement":"mg/kg","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"}})");

    for (int i = 0; i < RELAY_COUNT; i++) {
        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"unique_id\":\"smartgarden_relay_%d\",\"state_topic\":\"smartgarden/relay/%d/state\",\"command_topic\":\"smartgarden/relay/%d/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device\":{\"identifiers\":[\"smartgarden_esp32\"],\"manufacturer\":\"DIY\",\"model\":\"ESP32\",\"name\":\"Smart Garden\"}}",
            relayNames[i], i + 1, i + 1, i + 1);
        String switchTopic = String(discoveryPrefix) + "/switch/smartgarden_relay_" + String(i + 1) + "/config";
        publishDiscovery(switchTopic.c_str(), buffer);
        Serial.printf("  OK Relay %d: %s\n", i + 1, relayNames[i]);
    }

    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

// ================= MQTT RECONNECT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");
        
        if (client.connect(deviceId, mqtt_user, mqtt_password)) {
            Serial.println(" Connected!");
            publishDiscoveryMessages();
            for (int i = 0; i < RELAY_COUNT; i++) {
                String topic = "smartgarden/relay/" + String(i + 1) + "/set";
                client.subscribe(topic.c_str());
            }
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
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    reconnect();
    
    Serial.println("========== Setup Complete ==========
");
}

// ================= LOOP =================
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 10000;

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
