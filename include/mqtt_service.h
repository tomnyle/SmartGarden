#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "crop_profiles.h"
#include "auto_control.h"

// ========================================
// MqttService - wraps PubSubClient for SmartGarden
// ========================================
class MqttService
{
public:
    using MessageCallback = void (*)(const char *topic, const uint8_t *payload, unsigned int length);

    MqttService(const char *host, uint16_t port,
                const char *username, const char *password,
                const char *clientId);

    void begin(WiFiClient &wifiClient);

    bool connect();
    bool isConnected() const;

    // Subscribe to SmartGarden topics (relay commands, crop set)
    void subscribe(int relayCount = 8);

    // Publish sensor readings
    void publish(const SensorSnapshot &snapshot);

    // Publish list of crop names
    void publishCropList(const CropProfileStore &store);

    // Publish active crop config as JSON
    void publishCropConfig(const CropProfile *profile);

    // Publish relay state
    void publishRelayState(int relayIndex, bool state);

    // Set callback for incoming messages
    void setCallback(MessageCallback cb);

    // Must be called regularly to process MQTT packets
    void loop();

private:
    PubSubClient _client;

    char _host[64];
    uint16_t _port;
    char _username[32];
    char _password[32];
    char _clientId[32];

    int _relayCount;

    static void _internalCallback(char *topic, uint8_t *payload, unsigned int length);
    static MqttService *_instance;
    MessageCallback _userCallback;
};
