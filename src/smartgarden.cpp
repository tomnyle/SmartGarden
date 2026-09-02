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

const unsigned long SENSOR_READ_INTERVAL = 5000;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;
const unsigned long MQTT_INITIAL_RECONNECT_DELAY = 1000;
const unsigned long MQTT_MAX_RECONNECT_DELAY = 30000;
const uint16_t MQTT_BUFFER_SIZE = 1536;

unsigned long lastSensorRead = 0;
unsigned long lastWiFiReconnectAttempt = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long mqttReconnectDelay = MQTT_INITIAL_RECONNECT_DELAY;
bool mqttWasConnected = false;
char mqttClientId[32] = {0};

struct SensorState {
    float airTemp;
    float airHumidity;
    float soilMoisture;
    float soilTemp;
    float ph;
    uint16_t ec;
    uint16_t nitrogen;
    uint16_t phosphorus;
    uint16_t potassium;
};

SensorState sensorState = {
    22.0f,
    65.0f,
    60.0f,
    20.0f,
    6.8f,
    1200,
    45,
    35,
    50
};

float clampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

uint16_t clampUint16(int value, uint16_t minValue, uint16_t maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return static_cast<uint16_t>(value);
}

bool publishTopic(const String& topic, const String& payload, bool retain, const char* label) {
    if (!client.connected()) {
        Serial.printf("[MQTT] Publish skipped (%s): client disconnected\n", label);
        return false;
    }

    bool ok = client.publish(topic.c_str(), payload.c_str(), retain);
    Serial.printf(ok ? "[MQTT] Published %s: %s = %s\n"
                     : "[MQTT] Publish FAILED %s: %s = %s\n",
                  label, topic.c_str(), payload.c_str());
    return ok;
}

void simulateClimateValues() {
    sensorState.airTemp = clampFloat(sensorState.airTemp + (random(-10, 11) / 100.0f), 15.0f, 35.0f);
    sensorState.airHumidity = clampFloat(sensorState.airHumidity + (random(-20, 21) / 100.0f), 40.0f, 90.0f);
}

void simulateSoilValues() {
    sensorState.soilMoisture = clampFloat(sensorState.soilMoisture + (random(-15, 16) / 100.0f), 35.0f, 85.0f);
    sensorState.soilTemp = clampFloat(sensorState.soilTemp + (random(-10, 11) / 100.0f), 15.0f, 30.0f);
    sensorState.ph = clampFloat(sensorState.ph + (random(-2, 3) / 100.0f), 5.5f, 7.5f);
    sensorState.ec = clampUint16(static_cast<int>(sensorState.ec) + random(-20, 21), 900, 1800);
    sensorState.nitrogen = clampUint16(static_cast<int>(sensorState.nitrogen) + random(-1, 2), 20, 80);
    sensorState.phosphorus = clampUint16(static_cast<int>(sensorState.phosphorus) + random(-1, 2), 15, 70);
    sensorState.potassium = clampUint16(static_cast<int>(sensorState.potassium) + random(-1, 2), 20, 90);
}

void updateSensorState() {
    float airTemp = dht.readTemperature();
    float airHum = dht.readHumidity();

    if (!isnan(airTemp) && !isnan(airHum) &&
        airTemp >= -40.0f && airTemp <= 80.0f &&
        airHum >= 0.0f && airHum <= 100.0f) {
        sensorState.airTemp = airTemp;
        sensorState.airHumidity = airHum;
        Serial.printf("[Sensor] DHT22 OK: Temp=%.1f°C, Humidity=%.1f%%\n", airTemp, airHum);
    } else {
        Serial.println("[Sensor] DHT22 unavailable, using simulated climate values");
        simulateClimateValues();
    }

    simulateSoilValues();
}

void publishSensorData() {
    publishTopic("smartgarden/sensors/air_temp", String(sensorState.airTemp, 1), true, "Air Temperature");
    publishTopic("smartgarden/sensors/air_humidity", String(sensorState.airHumidity, 1), true, "Air Humidity");
    publishTopic("smartgarden/sensors/soil_moisture", String(sensorState.soilMoisture, 1), true, "Soil Moisture");
    publishTopic("smartgarden/sensors/soil_temp", String(sensorState.soilTemp, 1), true, "Soil Temperature");
    publishTopic("smartgarden/sensors/ph", String(sensorState.ph, 2), true, "pH");
    publishTopic("smartgarden/sensors/ec", String(sensorState.ec), true, "EC");
    publishTopic("smartgarden/sensors/nitrogen", String(sensorState.nitrogen), true, "Nitrogen");
    publishTopic("smartgarden/sensors/phosphorus", String(sensorState.phosphorus), true, "Phosphorus");
    publishTopic("smartgarden/sensors/potassium", String(sensorState.potassium), true, "Potassium");
}

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
    publishTopic(stateTopic, state ? "ON" : "OFF", true, relayNames[index]);
    
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
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");
    
    char buffer[1024];
    
    // Device info (used by all entities)
    const char* deviceInfo = R"(,"availability_topic":"smartgarden/status","payload_available":"online","payload_not_available":"offline","device":{"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"})";
    
    // ===== SENSORS =====
    
    // Air Temperature
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Temperature\",\"unique_id\":\"smartgarden_air_temp\",\"state_topic\":\"smartgarden/sensors/air_temp\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_air_temp/config", buffer, true, "Discovery Air Temperature");
    delay(25);
    
    // Air Humidity
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Humidity\",\"unique_id\":\"smartgarden_air_humidity\",\"state_topic\":\"smartgarden/sensors/air_humidity\",\"unit_of_measurement\":\"%%\",\"device_class\":\"humidity\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_air_humidity/config", buffer, true, "Discovery Air Humidity");
    delay(25);
    
    // Soil Moisture
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Moisture\",\"unique_id\":\"smartgarden_soil_moisture\",\"state_topic\":\"smartgarden/sensors/soil_moisture\",\"unit_of_measurement\":\"%%\",\"device_class\":\"moisture\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_soil_moisture/config", buffer, true, "Discovery Soil Moisture");
    delay(25);
    
    // Soil Temperature
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Temperature\",\"unique_id\":\"smartgarden_soil_temp\",\"state_topic\":\"smartgarden/sensors/soil_temp\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_soil_temp/config", buffer, true, "Discovery Soil Temperature");
    delay(25);
    
    // pH
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"pH Value\",\"unique_id\":\"smartgarden_ph\",\"state_topic\":\"smartgarden/sensors/ph\",\"unit_of_measurement\":\"pH\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_ph/config", buffer, true, "Discovery pH");
    delay(25);
    
    // EC
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"EC\",\"unique_id\":\"smartgarden_ec\",\"state_topic\":\"smartgarden/sensors/ec\",\"unit_of_measurement\":\"µS/cm\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_ec/config", buffer, true, "Discovery EC");
    delay(25);
    
    // Nitrogen
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Nitrogen\",\"unique_id\":\"smartgarden_nitrogen\",\"state_topic\":\"smartgarden/sensors/nitrogen\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_nitrogen/config", buffer, true, "Discovery Nitrogen");
    delay(25);
    
    // Phosphorus
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Phosphorus\",\"unique_id\":\"smartgarden_phosphorus\",\"state_topic\":\"smartgarden/sensors/phosphorus\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_phosphorus/config", buffer, true, "Discovery Phosphorus");
    delay(25);
    
    // Potassium
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Potassium\",\"unique_id\":\"smartgarden_potassium\",\"state_topic\":\"smartgarden/sensors/potassium\",\"unit_of_measurement\":\"mg/kg\",\"state_class\":\"measurement\"%s}",
        deviceInfo);
    publishTopic(String(discoveryPrefix) + "/sensor/smartgarden_potassium/config", buffer, true, "Discovery Potassium");
    delay(25);
    
    // ===== SWITCHES (RELAYS) =====
    
    for (int i = 0; i < RELAY_COUNT; i++) {
        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"unique_id\":\"smartgarden_relay_%d\",\"state_topic\":\"smartgarden/relay/%d/state\",\"command_topic\":\"smartgarden/relay/%d/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"%s}",
            relayNames[i], i + 1, i + 1, i + 1, deviceInfo);
        
        String switchTopic = String(discoveryPrefix) + "/switch/smartgarden_relay_" + String(i + 1) + "/config";
        String label = String("Discovery Relay ") + String(i + 1);
        publishTopic(switchTopic, buffer, true, label.c_str());
        delay(25);
    }
    
    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

// ================= MQTT RECONNECT =================
bool reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    if (client.connected()) {
        return true;
    }

    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt < mqttReconnectDelay) {
        return false;
    }
    lastMqttReconnectAttempt = now;

    Serial.printf("[MQTT] Connecting as %s...", mqttClientId);

    if (client.connect(mqttClientId, mqtt_user, mqtt_password, availabilityTopic, 1, true, "offline")) {
        Serial.println(" Connected!");
        mqttReconnectDelay = MQTT_INITIAL_RECONNECT_DELAY;
        mqttWasConnected = true;

        publishTopic(availabilityTopic, "online", true, "Availability");
        publishDiscoveryMessages();

        for (int i = 0; i < RELAY_COUNT; i++) {
            String topic = "smartgarden/relay/" + String(i + 1) + "/set";
            client.subscribe(topic.c_str());
            Serial.printf("[MQTT] Subscribed: %s\n", topic.c_str());
        }

        for (int i = 0; i < RELAY_COUNT; i++) {
            setRelay(i, relayState[i]);
        }

        return true;
    }

    Serial.printf(" Failed (code=%d), next retry in %lus\n", client.state(), mqttReconnectDelay / 1000);
    mqttReconnectDelay = min(mqttReconnectDelay * 2, MQTT_MAX_RECONNECT_DELAY);
    mqttWasConnected = false;
    return false;
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
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
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
    client.setBufferSize(MQTT_BUFFER_SIZE);
    client.setKeepAlive(30);
    client.setSocketTimeout(10);
    snprintf(mqttClientId, sizeof(mqttClientId), "smartgarden-%llX", ESP.getEfuseMac());
    Serial.printf("[Setup] MQTT client ID: %s\n", mqttClientId);
    reconnect();
    
    Serial.println("========== Setup Complete ==========\n");
}

// ================= LOOP =================
void loop() {
    // WiFi reconnect check
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
            lastWiFiReconnectAttempt = now;
            Serial.println("[WiFi] Disconnected, attempting reconnect...");
            WiFi.reconnect();
        }
    } else if (lastWiFiReconnectAttempt != 0) {
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());
        lastWiFiReconnectAttempt = 0;
    }
    
    // MQTT reconnect check
    if (!client.connected()) {
        if (mqttWasConnected) {
            Serial.printf("[MQTT] Connection lost (state=%d)\n", client.state());
            mqttWasConnected = false;
        }
        reconnect();
    } else {
        mqttWasConnected = true;
    }
    
    client.loop();
    
    // Read sensors every SENSOR_READ_INTERVAL
    unsigned long now = millis();
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;
        updateSensorState();
        publishSensorData();
        
        // Print to serial
        Serial.println("\n========== SENSOR DATA ==========");
        Serial.printf("Air Temp     : %.1f C\n", sensorState.airTemp);
        Serial.printf("Air Humidity : %.1f %%\n", sensorState.airHumidity);
        Serial.printf("Soil Moisture: %.1f %%\n", sensorState.soilMoisture);
        Serial.printf("Soil Temp    : %.1f C\n", sensorState.soilTemp);
        Serial.printf("pH           : %.2f\n", sensorState.ph);
        Serial.printf("EC           : %u uS/cm\n", sensorState.ec);
        Serial.printf("Nitrogen     : %u mg/kg\n", sensorState.nitrogen);
        Serial.printf("Phosphorus   : %u mg/kg\n", sensorState.phosphorus);
        Serial.printf("Potassium    : %u mg/kg\n", sensorState.potassium);
        Serial.println("=================================\n");
    }
    
    delay(10);
}
