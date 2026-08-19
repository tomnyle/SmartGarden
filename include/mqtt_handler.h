#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "climate_manager.h"
#include "crop_profiles.h"

class RelayManager;

class MqttHandler
{
public:
    MqttHandler();

    void begin(RelayManager &relayManager,
               ClimateManager &climateManager,
               CropProfileStore &cropStore);
    void loop();
    bool isConnected() const;

    void publishSensorData(const SensorSnapshot &snapshot);
    void publishCropList();
    void publishCurrentCropConfig();
    void publishRelayStates();

private:
    RelayManager *relayManager = nullptr;
    ClimateManager *climateManager = nullptr;
    CropProfileStore *cropStore = nullptr;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt = 0;

    static MqttHandler *activeInstance;

    void connectWiFi();
    void ensureConnected();
    void subscribeTopics();
    void onMessage(const String &topic, const String &payload);
    void publishTopic(const String &suffix, const String &payload, bool retained = true);
    static void mqttCallback(char *topic, byte *payload, unsigned int length);
};
