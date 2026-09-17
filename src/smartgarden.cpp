#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>
#include "app_config.h"
#include "pins.h"

WiFiClient espClient;
PubSubClient client(espClient);

DHT dht(DHT_PIN, DHT_TYPE);
HardwareSerial RS485Serial(2);
ModbusMaster node;

static constexpr int RELAY_COUNT = 8;
static bool relayState[RELAY_COUNT] = {false};
static const char* relayNames[RELAY_COUNT] = {"Fan", "Heater", "Cooler", "Humidifier", "Dehumidifier", "Irrigation", "Relay7", "Relay8"};
static const char* relayEntityIds[RELAY_COUNT] = {"fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation", "relay7", "relay8"};

static unsigned long lastWifiReconnectAttempt = 0;
static unsigned long lastMqttReconnectAttempt = 0;
static unsigned long mqttBackoffMs = 2000;
static const unsigned long MQTT_BACKOFF_MAX_MS = 60000;
static String currentCrop = "lettuce";

static unsigned long lastSensorRead = 0;

void preTransmission() { digitalWrite(RS485_DE, HIGH); }
void postTransmission() { digitalWrite(RS485_DE, LOW); }

static const char* relayStateTopicByIndex(int relayIndex) {
    switch (relayIndex) {
        case 0: return MQTT_TOPIC_RELAY_1_STATE;
        case 1: return MQTT_TOPIC_RELAY_2_STATE;
        case 2: return MQTT_TOPIC_RELAY_3_STATE;
        case 3: return MQTT_TOPIC_RELAY_4_STATE;
        case 4: return MQTT_TOPIC_RELAY_5_STATE;
        case 5: return MQTT_TOPIC_RELAY_6_STATE;
        case 6: return MQTT_TOPIC_RELAY_7_STATE;
        case 7: return MQTT_TOPIC_RELAY_8_STATE;
        default: return nullptr;
    }
}

static const char* relayCommandTopicByIndex(int relayIndex) {
    switch (relayIndex) {
        case 0: return MQTT_TOPIC_RELAY_1_SET;
        case 1: return MQTT_TOPIC_RELAY_2_SET;
        case 2: return MQTT_TOPIC_RELAY_3_SET;
        case 3: return MQTT_TOPIC_RELAY_4_SET;
        case 4: return MQTT_TOPIC_RELAY_5_SET;
        case 5: return MQTT_TOPIC_RELAY_6_SET;
        case 6: return MQTT_TOPIC_RELAY_7_SET;
        case 7: return MQTT_TOPIC_RELAY_8_SET;
        default: return nullptr;
    }
}

static void publishDiscoveryMessage(const char* topic, const char* payload) {
    client.publish(topic, payload, true);
    delay(20);
}

void setRelay(int index, bool state) {
    if (index < 0 || index >= RELAY_COUNT) return;

    relayState[index] = state;
    digitalWrite(RELAY_PINS[index], state ? LOW : HIGH);

    const char* stateTopic = relayStateTopicByIndex(index);
    if (stateTopic) {
        client.publish(stateTopic, state ? "ON" : "OFF", true);
    }

    Serial.printf("[Relay] Relay %d (%s) -> %s\n", index + 1, relayNames[index], state ? "ON" : "OFF");
}

static void publishDiscoveryMessages() {
    if (!client.connected()) return;

    Serial.println("\n[MQTT Discovery] Publishing Home Assistant discovery...");

    char buffer[1024];
    const char* deviceInfo = R"({"identifiers":["smartgarden_esp32"],"manufacturer":"DIY","model":"ESP32","name":"Smart Garden"})";
    char av[192];
    snprintf(av, sizeof(av),
        "\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"",
        MQTT_TOPIC_AVAILABILITY);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Temperature\",\"unique_id\":\"smartgarden_air_temp\",\"state_topic\":\"%s\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",%s,\"device\":%s}",
        MQTT_TOPIC_AIR_TEMP, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_air_temp/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Air Humidity\",\"unique_id\":\"smartgarden_air_humidity\",\"state_topic\":\"%s\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",%s,\"device\":%s}",
        MQTT_TOPIC_AIR_HUMIDITY, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_air_humidity/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Moisture\",\"unique_id\":\"smartgarden_soil_moisture\",\"state_topic\":\"%s\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"%%\",%s,\"device\":%s}",
        MQTT_TOPIC_SOIL_MOISTURE, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_soil_moisture/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Soil Temperature\",\"unique_id\":\"smartgarden_soil_temp\",\"state_topic\":\"%s\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"°C\",%s,\"device\":%s}",
        MQTT_TOPIC_SOIL_TEMP, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_soil_temp/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"pH Value\",\"unique_id\":\"smartgarden_ph\",\"state_topic\":\"%s\",\"state_class\":\"measurement\",%s,\"device\":%s}",
        MQTT_TOPIC_PH, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_ph/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"EC\",\"unique_id\":\"smartgarden_ec\",\"state_topic\":\"%s\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"uS/cm\",%s,\"device\":%s}",
        MQTT_TOPIC_EC, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_ec/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Nitrogen\",\"unique_id\":\"smartgarden_nitrogen\",\"state_topic\":\"%s\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",%s,\"device\":%s}",
        MQTT_TOPIC_NITROGEN, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_nitrogen/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Phosphorus\",\"unique_id\":\"smartgarden_phosphorus\",\"state_topic\":\"%s\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",%s,\"device\":%s}",
        MQTT_TOPIC_PHOSPHORUS, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_phosphorus/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Potassium\",\"unique_id\":\"smartgarden_potassium\",\"state_topic\":\"%s\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"mg/kg\",%s,\"device\":%s}",
        MQTT_TOPIC_POTASSIUM, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_potassium/config", buffer);

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"WiFi RSSI\",\"unique_id\":\"smartgarden_rssi\",\"state_topic\":\"%s\",\"device_class\":\"signal_strength\",\"state_class\":\"measurement\",\"unit_of_measurement\":\"dBm\",\"entity_category\":\"diagnostic\",%s,\"device\":%s}",
        MQTT_TOPIC_DIAG_RSSI, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/sensor/smartgarden_rssi/config", buffer);

    for (int i = 0; i < RELAY_COUNT; i++) {
        const char* cmdTopic = relayCommandTopicByIndex(i);
        const char* stateTopic = relayStateTopicByIndex(i);
        if (!cmdTopic || !stateTopic) continue;

        snprintf(buffer, sizeof(buffer),
            "{\"name\":\"%s\",\"unique_id\":\"smartgarden_%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",%s,\"device\":%s}",
            relayNames[i], relayEntityIds[i], stateTopic, cmdTopic, av, deviceInfo);

        char switchTopic[128];
        snprintf(switchTopic, sizeof(switchTopic), "homeassistant/switch/smartgarden_%s/config", relayEntityIds[i]);
        publishDiscoveryMessage(switchTopic, buffer);
    }

    snprintf(buffer, sizeof(buffer),
        "{\"name\":\"Crop Select\",\"unique_id\":\"smartgarden_crop\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"options\":[\"ginseng\",\"salvia\",\"morinda\",\"lettuce\",\"microgreens\",\"tomato\",\"strawberry\",\"cucumber\",\"chili\",\"eggplant\",\"carrot\",\"onion\",\"broccoli\"],%s,\"device\":%s}",
        MQTT_TOPIC_CROP_SELECT_STATE, MQTT_TOPIC_CROP_SELECT_COMMAND, av, deviceInfo);
    publishDiscoveryMessage("homeassistant/select/smartgarden_crop/config", buffer);

    Serial.println("[MQTT Discovery] All discovery messages published!\n");
}

static void publishRelayStates() {
    for (int i = 0; i < RELAY_COUNT; i++) {
        setRelay(i, relayState[i]);
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    msg.reserve(length);
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    Serial.printf("[MQTT RX] %s = %s\n", topic, msg.c_str());

    for (int i = 0; i < RELAY_COUNT; i++) {
        const char* cmdTopic = relayCommandTopicByIndex(i);
        if (cmdTopic && String(topic) == cmdTopic) {
            if (msg == "ON") setRelay(i, true);
            else if (msg == "OFF") setRelay(i, false);
            return;
        }
    }

    if (String(topic) == MQTT_TOPIC_CROP_SELECT_COMMAND) {
        if (msg.length() > 0) {
            currentCrop = msg;
        }
        client.publish(MQTT_TOPIC_CROP_SELECT_STATE, msg.c_str(), true);
    }
}

static void ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) return;

    const unsigned long now = millis();
    if (lastWifiReconnectAttempt != 0 && now - lastWifiReconnectAttempt < MQTT_RECONNECT_INTERVAL) return;
    lastWifiReconnectAttempt = now;

    if (WiFi.SSID().length() == 0) {
        Serial.printf("[WiFi] Begin SSID=%s\n", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    } else {
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
    }
}

static void ensureMqttConnected() {
    if (client.connected() || WiFi.status() != WL_CONNECTED) return;

    const unsigned long now = millis();
    if (lastMqttReconnectAttempt != 0 && now - lastMqttReconnectAttempt < mqttBackoffMs) return;
    lastMqttReconnectAttempt = now;

    Serial.printf("[MQTT] Connecting %s:%d retry=%lu\n", MQTT_BROKER, MQTT_PORT, mqttBackoffMs);
    if (client.connect(MQTT_DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC_STATUS, 1, true, "offline")) {
        Serial.println("[MQTT] Connected");
        mqttBackoffMs = 2000;

        client.publish(MQTT_TOPIC_STATUS, "online", true);
        client.publish(MQTT_TOPIC_CROP_LIST, "ginseng,salvia,morinda,lettuce,microgreens,tomato,strawberry,cucumber,chili,eggplant,carrot,onion,broccoli", true);
        client.publish(MQTT_TOPIC_CROP_SELECT_STATE, currentCrop.c_str(), true);
        client.publish(MQTT_TOPIC_DIAG_RSSI, String(WiFi.RSSI()).c_str(), true);

        for (int i = 0; i < RELAY_COUNT; i++) {
            const char* cmdTopic = relayCommandTopicByIndex(i);
            if (cmdTopic) client.subscribe(cmdTopic);
        }
        client.subscribe(MQTT_TOPIC_CROP_SELECT_COMMAND);

        publishDiscoveryMessages();
        publishRelayStates();
    } else {
        Serial.printf("[MQTT] Connect failed (code=%d)\n", client.state());
        mqttBackoffMs = (mqttBackoffMs < MQTT_BACKOFF_MAX_MS / 2) ? mqttBackoffMs * 2 : MQTT_BACKOFF_MAX_MS;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========== SmartGarden Startup ==========");

    for (int i = 0; i < RELAY_COUNT; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH);
        relayState[i] = false;
    }
    Serial.println("[Setup] Relays initialized");

    dht.begin();
    Serial.println("[Setup] DHT initialized");

    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW);
    RS485Serial.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX, RS485_TX);
    node.begin(1, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    Serial.println("[Setup] RS485 initialized");

    WiFi.mode(WIFI_STA);
    ensureWiFiConnected();

    client.setServer(MQTT_BROKER, MQTT_PORT);
    client.setBufferSize(MQTT_BUFFER_SIZE);
    client.setCallback(callback);

    Serial.println("========== Setup Complete ==========");
}

void loop() {
    ensureWiFiConnected();
    ensureMqttConnected();

    if (client.connected()) {
        client.loop();
    }

    const unsigned long now = millis();
    if (now - lastSensorRead < SENSOR_READ_INTERVAL) {
        delay(10);
        return;
    }
    lastSensorRead = now;

    float airTemp = dht.readTemperature();
    float airHum = dht.readHumidity();

    if (isnan(airTemp)) {
        airTemp = 0;
        Serial.println("[Sensor] DHT Temperature read failed!");
    }
    if (isnan(airHum)) {
        airHum = 0;
        Serial.println("[Sensor] DHT Humidity read failed!");
    }

    float moisture = 0.0;
    float soilTemp = 0.0;
    float ph = 0.0;
    uint16_t ec = 0;
    uint16_t n = 0;
    uint16_t p = 0;
    uint16_t k = 0;

    if (client.connected()) {
        client.publish(MQTT_TOPIC_AIR_TEMP, String(airTemp, 1).c_str(), true);
        client.publish(MQTT_TOPIC_AIR_HUMIDITY, String(airHum, 1).c_str(), true);
        client.publish(MQTT_TOPIC_SOIL_MOISTURE, String(moisture, 1).c_str(), true);
        client.publish(MQTT_TOPIC_SOIL_TEMP, String(soilTemp, 1).c_str(), true);
        client.publish(MQTT_TOPIC_PH, String(ph, 1).c_str(), true);
        client.publish(MQTT_TOPIC_EC, String(ec).c_str(), true);
        client.publish(MQTT_TOPIC_NITROGEN, String(n).c_str(), true);
        client.publish(MQTT_TOPIC_PHOSPHORUS, String(p).c_str(), true);
        client.publish(MQTT_TOPIC_POTASSIUM, String(k).c_str(), true);
        client.publish(MQTT_TOPIC_DIAG_RSSI, String(WiFi.RSSI()).c_str(), true);
    }

    Serial.println("\n========== SENSOR DATA ==========");
    Serial.printf("Air Temp     : %.1f C\n", airTemp);
    Serial.printf("Air Humidity : %.1f %%\n", airHum);
    Serial.printf("Soil Moisture: %.1f %%\n", moisture);
    Serial.printf("Soil Temp    : %.1f C\n", soilTemp);
    Serial.printf("pH           : %.1f\n", ph);
    Serial.printf("EC           : %u uS/cm\n", ec);
    Serial.printf("Nitrogen     : %u mg/kg\n", n);
    Serial.printf("Phosphorus   : %u mg/kg\n", p);
    Serial.printf("Potassium    : %u mg/kg\n", k);
    Serial.println("=================================");

    delay(10);
}
