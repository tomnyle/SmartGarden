#include "mqtt_service.h"

namespace {
MQTTService* g_mqttService = nullptr;

constexpr unsigned long RECONNECT_INTERVAL_MS = 3000;
constexpr unsigned long STATE_PUBLISH_INTERVAL_MS = 10000;
constexpr const char* AVAILABILITY_TOPIC = "smartgarden/status";

const char* SENSOR_TOPICS[] = {
    "smartgarden/sensors/air_temp",
    "smartgarden/sensors/air_humidity",
    "smartgarden/sensors/soil_moisture",
    "smartgarden/sensors/soil_temp",
    "smartgarden/sensors/ph",
    "smartgarden/sensors/ec",
    "smartgarden/sensors/nitrogen",
    "smartgarden/sensors/phosphorus",
    "smartgarden/sensors/potassium",
};
}

void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
    if (g_mqttService != nullptr) {
        g_mqttService->onMessageReceived(topic, payload, length);
    }
}

MQTTService::MQTTService(PubSubClient& client, DiscoveryService& discoveryService, const char* clientId)
    : client(client),
      discoveryService(discoveryService),
      clientId(clientId),
      relayCallback(nullptr),
      wasConnected(false),
      discoveryPublishedThisSession(false),
      lastReconnectAttempt(0),
      lastStatePublish(0) {
    mqttUsername[0] = '\0';
    mqttPassword[0] = '\0';
    g_mqttService = this;
}

void MQTTService::begin(const char* username, const char* password) {
    strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
    mqttUsername[sizeof(mqttUsername) - 1] = '\0';
    strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);
    mqttPassword[sizeof(mqttPassword) - 1] = '\0';

    client.setBufferSize(1024);
    client.setCallback(mqttMessageCallback);
    Serial.println("[MQTT] Service initialized");
}

void MQTTService::loop(const SensorSnapshot& snapshot, const bool* relayStates, size_t relayCount) {
    if (!client.connected()) {
        if (wasConnected) {
            wasConnected = false;
            discoveryPublishedThisSession = false;
            Serial.println("[MQTT] Session lost; discovery will be republished after reconnect");
        }

        const unsigned long now = millis();
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            reconnect(relayStates, relayCount);
        }
        return;
    }

    client.loop();
    publishPeriodicState(snapshot, relayStates, relayCount);
}

void MQTTService::setRelayCommandCallback(RelayCommandCallback callback) {
    relayCallback = callback;
}

bool MQTTService::isConnected() const {
    return client.connected();
}

bool MQTTService::reconnect(const bool* relayStates, size_t relayCount) {
    Serial.print("[MQTT] Connecting...");

    const bool connected = client.connect(clientId,
                                          mqttUsername,
                                          mqttPassword,
                                          AVAILABILITY_TOPIC,
                                          0,
                                          true,
                                          "offline");

    if (!connected) {
        Serial.printf(" Failed (code=%d)\n", client.state());
        return false;
    }

    Serial.println(" Connected!");
    wasConnected = true;
    publishAvailability("online");
    subscribeToTopics();

    if (!discoveryPublishedThisSession) {
        discoveryService.begin();
        discoveryPublishedThisSession = true;
    }

    publishAllRelayStates(relayStates, relayCount);
    lastStatePublish = millis() - STATE_PUBLISH_INTERVAL_MS;
    return true;
}

void MQTTService::subscribeToTopics() {
    for (uint8_t index = 0; index < 8; ++index) {
        String topic = "smartgarden/relay/";
        topic += String(index + 1);
        topic += "/set";

        const bool subscribed = client.subscribe(topic.c_str());
        Serial.printf("[MQTT] %s subscribe %s\n",
                      subscribed ? "OK" : "FAILED",
                      topic.c_str());
    }
}

void MQTTService::publishAvailability(const char* state) {
    const bool published = client.publish(AVAILABILITY_TOPIC, state, true);
    Serial.printf("[MQTT] %s availability %s\n",
                  published ? "OK" : "FAILED",
                  state);
}

bool MQTTService::publishRelayState(uint8_t relayIndex, bool state) {
    if (!client.connected()) {
        return false;
    }

    String topic = "smartgarden/relay/";
    topic += String(relayIndex + 1);
    topic += "/state";

    return client.publish(topic.c_str(), state ? "ON" : "OFF", true);
}

bool MQTTService::publishAllRelayStates(const bool* relayStates, size_t relayCount) {
    if (!client.connected() || relayStates == nullptr) {
        return false;
    }

    bool allPublished = true;
    for (size_t index = 0; index < relayCount; ++index) {
        allPublished = publishRelayState(static_cast<uint8_t>(index), relayStates[index]) && allPublished;
    }

    Serial.println("[MQTT] Relay states published");
    return allPublished;
}

bool MQTTService::publishSensorSnapshot(const SensorSnapshot& snapshot) {
    if (!client.connected()) {
        return false;
    }

    char payload[32];
    bool allPublished = true;

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airTemp);
    allPublished = client.publish(SENSOR_TOPICS[0], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%.1f", snapshot.airHumidity);
    allPublished = client.publish(SENSOR_TOPICS[1], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilMoisture);
    allPublished = client.publish(SENSOR_TOPICS[2], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%.1f", snapshot.soilTemp);
    allPublished = client.publish(SENSOR_TOPICS[3], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%.1f", snapshot.ph);
    allPublished = client.publish(SENSOR_TOPICS[4], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%u", snapshot.ec);
    allPublished = client.publish(SENSOR_TOPICS[5], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%u", snapshot.nitrogen);
    allPublished = client.publish(SENSOR_TOPICS[6], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%u", snapshot.phosphorus);
    allPublished = client.publish(SENSOR_TOPICS[7], payload, true) && allPublished;

    snprintf(payload, sizeof(payload), "%u", snapshot.potassium);
    allPublished = client.publish(SENSOR_TOPICS[8], payload, true) && allPublished;

    Serial.printf("[MQTT] Sensor snapshot %s\n", allPublished ? "published" : "publish failed");
    return allPublished;
}

void MQTTService::publishPeriodicState(const SensorSnapshot& snapshot, const bool* relayStates, size_t relayCount) {
    const unsigned long now = millis();
    if (now - lastStatePublish < STATE_PUBLISH_INTERVAL_MS) {
        return;
    }

    lastStatePublish = now;
    publishSensorSnapshot(snapshot);
    publishAllRelayStates(relayStates, relayCount);
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length) {
    char message[16];
    if (length >= sizeof(message)) {
        length = sizeof(message) - 1;
    }

    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("[MQTT RX] %s = %s\n", topic, message);

    unsigned int relayNumber = 0;
    if (sscanf(topic, "smartgarden/relay/%u/set", &relayNumber) != 1 || relayNumber < 1 || relayNumber > 8) {
        Serial.println("[MQTT] Ignored unsupported topic");
        return;
    }

    if (strcmp(message, "ON") != 0 && strcmp(message, "OFF") != 0) {
        Serial.println("[MQTT] Ignored unsupported relay payload");
        return;
    }

    if (relayCallback != nullptr) {
        relayCallback(static_cast<uint8_t>(relayNumber - 1), strcmp(message, "ON") == 0);
    }
}