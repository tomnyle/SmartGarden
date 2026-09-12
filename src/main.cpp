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
const char* statusTopic = "smartgarden/status";
const char* defaultCrop = "Sâm";

// ================= RS485 CONTROL =================
void preTransmission() {
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_DE, LOW);
}

// ================= HELPERS =================
static void publishDiscoveryMessage(const char* topic, const char* payload) {
    client.publish(topic, payload, true);
    delay(50);
}

static void publishDiscoveryConfig(const char* domain, const char* objectId, const char* payload) {
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/config", discoveryPrefix, domain, deviceId, objectId);
    publishDiscoveryMessage(topic, payload);
}

static void publishAvailability(const char* payload) {
    client.publish(statusTopic, payload, true);
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
        }
    }
}

// ================= PUBLISH DISCOVERY MESSAGES =================
void publishDiscoveryMessages() {
    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");

    char buffer[1024];
    const char* deviceInfo = R"({"ids":["smartgarden_esp32"],"identifiers":["smartgarden_esp32"],"mf":"DIY","manufacturer":"DIY","mdl":"ESP32","model":"ESP32","name":"Smart Garden"})";

    // Sensors
    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Temperature\",\"obj_id\":\"air_temp\",\"uniq_id\":\"smartgarden_air_temp\",\"stat_t\":\"smartgarden/sensors/air_temp\",\"dev_cla\":\"temperature\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "air_temp", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Humidity\",\"obj_id\":\"air_humidity\",\"uniq_id\":\"smartgarden_air_humidity\",\"stat_t\":\"smartgarden/sensors/air_humidity\",\"dev_cla\":\"humidity\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "air_humidity", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Moisture\",\"obj_id\":\"soil_moisture\",\"uniq_id\":\"smartgarden_soil_moisture\",\"stat_t\":\"smartgarden/sensors/soil_moisture\",\"dev_cla\":\"moisture\",\"device_class\":\"moisture\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "soil_moisture", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Temperature\",\"obj_id\":\"soil_temp\",\"uniq_id\":\"smartgarden_soil_temp\",\"stat_t\":\"smartgarden/sensors/soil_temp\",\"dev_cla\":\"temperature\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "soil_temp", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"pH Value\",\"obj_id\":\"ph\",\"uniq_id\":\"smartgarden_ph\",\"stat_t\":\"smartgarden/sensors/ph\",\"state_class\":\"measurement\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "ph", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"EC\",\"obj_id\":\"ec\",\"uniq_id\":\"smartgarden_ec\",\"stat_t\":\"smartgarden/sensors/ec\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"uS/cm\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "ec", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Nitrogen\",\"obj_id\":\"nitrogen\",\"uniq_id\":\"smartgarden_nitrogen\",\"stat_t\":\"smartgarden/sensors/nitrogen\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "nitrogen", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Phosphorus\",\"obj_id\":\"phosphorus\",\"uniq_id\":\"smartgarden_phosphorus\",\"stat_t\":\"smartgarden/sensors/phosphorus\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "phosphorus", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Potassium\",\"obj_id\":\"potassium\",\"uniq_id\":\"smartgarden_potassium\",\"stat_t\":\"smartgarden/sensors/potassium\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "potassium", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Crop Profile\",\"obj_id\":\"crop_current\",\"uniq_id\":\"smartgarden_crop_current\",\"stat_t\":\"smartgarden/crop/current\",\"icon\":\"mdi:sprout\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
        deviceInfo);
    publishDiscoveryConfig("sensor", "crop_current", buffer);

    // Switches
    const char* relayIds[] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};
    for (int i = 0; i < RELAY_COUNT; i++) {
        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"obj_id\":\"%s\",\"uniq_id\":\"smartgarden_%s\",\"stat_t\":\"smartgarden/relay/%d/state\",\"cmd_t\":\"smartgarden/relay/%d/set\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"avty_t\":\"smartgarden/status\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",\"dev\":%s}",
            relayNames[i], relayIds[i], relayIds[i], i + 1, i + 1, deviceInfo);
        publishDiscoveryConfig("switch", relayIds[i], buffer);
    }

    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

// ================= MQTT RECONNECT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting...");

        if (client.connect(deviceId, mqtt_user, mqtt_password, statusTopic, 0, true, "offline")) {
            Serial.println(" Connected!");

            publishAvailability("online");

            publishDiscoveryMessages();
            client.publish("smartgarden/crop/current", defaultCrop, true);

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

    Serial.println("========== Setup Complete ==========");
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
        Serial.println("=================================");
    }

    delay(10);
}
