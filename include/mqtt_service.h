#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include "discovery_service.h"
#include "sensor_manager.h"

typedef void (*RelayCommandCallback)(uint8_t relayIndex, bool state);

class MQTTService {
public:
    MQTTService(PubSubClient& client, DiscoveryService& discoveryService, const char* clientId);

    void begin(const char* username, const char* password);
    void loop(const SensorSnapshot& snapshot, const bool* relayStates, size_t relayCount);
    void setRelayCommandCallback(RelayCommandCallback callback);

    bool isConnected() const;
    bool publishRelayState(uint8_t relayIndex, bool state);
    bool publishAllRelayStates(const bool* relayStates, size_t relayCount);
    bool publishSensorSnapshot(const SensorSnapshot& snapshot);

private:
    PubSubClient& client;
    DiscoveryService& discoveryService;
    const char* clientId;
    RelayCommandCallback relayCallback;
    char mqttUsername[64];
    char mqttPassword[64];
    bool wasConnected;
    bool discoveryPublishedThisSession;
    unsigned long lastReconnectAttempt;
    unsigned long lastStatePublish;

    bool reconnect(const bool* relayStates, size_t relayCount);
    void subscribeToTopics();
    void publishAvailability(const char* state);
    void publishPeriodicState(const SensorSnapshot& snapshot, const bool* relayStates, size_t relayCount);
    void onMessageReceived(char* topic, byte* payload, unsigned int length);

    friend void mqttMessageCallback(char* topic, byte* payload, unsigned int length);
};

#endif
