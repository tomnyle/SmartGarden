#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <string.h>

#include "sensor_manager.h"
#include "mqtt_service.h"
#include "network_service.h"
#include "climate_manager.h"
#include "irrigation_manager.h"
#include "relay_manager.h"
#include "data_logger.h"
#include "garden_profile.h"
#include "auto_control.h"
#include "config.h"
#include "pins.h"
#include "app_config.h"

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHT_TYPE);

SensorManager sensorManager;
NetworkService* networkService = nullptr;
MQTTService* mqttService = nullptr;
ClimateManager climateManager;
IrrigationManager irrigationManager;
RelayManager relayManager;
DataLogger dataLogger;
SystemConfig systemConfig;

unsigned long lastSensorRead = 0;
unsigned long lastControlUpdate = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastMqttReconnect = 0;
unsigned long bootMillis = 0;
bool mqttWasConnected = false;
bool hasValidSensorSnapshot = false;

const CropProfile* activeProfile = nullptr;

static const char* RELAY_NAMES[] = {
    "fan", "heater", "cooler", "humidifier", "dehumidifier", "irrigation"
};

static const uint8_t RELAY_INDICES[] = {
    CLIMATE_FAN_RELAY_INDEX,
    CLIMATE_HEATER_RELAY_INDEX,
    CLIMATE_COOLER_RELAY_INDEX,
    CLIMATE_HUMIDIFIER_RELAY_INDEX,
    CLIMATE_DEHUMIDIFIER_RELAY_INDEX,
    IRRIGATION_RELAY_INDEX
};

int getRelayIndexByName(const char* relayName)
{
    for (size_t i = 0; i < sizeof(RELAY_NAMES) / sizeof(RELAY_NAMES[0]); ++i)
    {
        if (strcmp(relayName, RELAY_NAMES[i]) == 0)
        {
            return RELAY_INDICES[i];
        }
    }
    return -1;
}

void publishAllRelayStates()
{
    if (mqttService == nullptr || !mqttService->isConnected())
    {
        return;
    }

    for (size_t i = 0; i < sizeof(RELAY_NAMES) / sizeof(RELAY_NAMES[0]); ++i)
    {
        bool state = relayManager.getRelayState(RELAY_INDICES[i]);
        mqttService->publishRelayState(RELAY_NAMES[i], state);
    }
}

void onRelayCommand(const char* relayName, bool state)
{
    int relayIndex = getRelayIndexByName(relayName);
    if (relayIndex < 0)
    {
        return;
    }

    if (relayIndex == IRRIGATION_RELAY_INDEX)
    {
        if (state)
        {
            irrigationManager.start();
        }
        else
        {
            irrigationManager.stop();
        }
    }
    else
    {
        relayManager.setRelay(static_cast<uint8_t>(relayIndex), state);
    }

    if (mqttService != nullptr && mqttService->isConnected())
    {
        bool currentState = (relayIndex == IRRIGATION_RELAY_INDEX)
                                ? irrigationManager.isActive()
                                : relayManager.getRelayState(static_cast<uint8_t>(relayIndex));
        mqttService->publishRelayState(relayName, currentState);
    }
}

void onCropSelect(const char* cropName)
{
    const CropProfile* selected = CropProfileStore::getCropByName(cropName);
    if (selected == nullptr)
    {
        return;
    }

    activeProfile = selected;
    climateManager.setCurrentProfile(activeProfile);
    Serial.printf("[Main] Active crop set to: %s\n", activeProfile->name);

    if (mqttService != nullptr && mqttService->isConnected())
    {
        mqttService->publishCurrentCrop(activeProfile->name);
    }
}

void publishBootstrapTopics()
{
    if (mqttService == nullptr || !mqttService->isConnected())
    {
        return;
    }

    mqttService->publishStatus(true);
    mqttService->publishDiscovery();
    mqttService->publishCropList();
    if (activeProfile != nullptr)
    {
        mqttService->publishCurrentCrop(activeProfile->name);
    }
    publishAllRelayStates();
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    bootMillis = millis();

    Serial.println("\n\n========== SmartGarden System Starting ==========");

    if (!loadConfig(systemConfig))
    {
        systemConfig = getDefaultConfig();
        saveConfig(systemConfig);
    }
    printConfig(systemConfig);

    CropProfileStore::initialize();
    activeProfile = CropProfileStore::getCropById(1);

    relayManager.begin();
    dht.begin();
    sensorManager.begin();
    dataLogger.begin();
    climateManager.begin(&relayManager);
    climateManager.setCurrentProfile(activeProfile);
    irrigationManager.begin(&relayManager);

    networkService = new NetworkService(systemConfig.wifiSSID, systemConfig.wifiPassword);
    networkService->begin();

    uint8_t wifiAttempts = 0;
    while (!networkService->isConnected() && wifiAttempts < 20)
    {
        delay(500);
        networkService->loop();
        ++wifiAttempts;
    }

    mqttService = new MQTTService(systemConfig.mqttBroker, systemConfig.mqttPort);
    mqttService->begin(systemConfig.mqttUsername, systemConfig.mqttPassword);
    mqttService->setRelayCommandCallback(onRelayCommand);
    mqttService->setCropSelectCallback(onCropSelect);

    if (mqttService->connect())
    {
        publishBootstrapTopics();
        mqttWasConnected = true;
    }

    lastSensorRead = millis();
    lastControlUpdate = millis();
    lastStatusPublish = millis();
}

void loop()
{
    unsigned long now = millis();

    if (networkService != nullptr)
    {
        if (!networkService->isConnected())
        {
            networkService->begin();
        }
        networkService->loop();
    }

    if (mqttService != nullptr)
    {
        if (!mqttService->isConnected() && (now - lastMqttReconnect >= MQTT_RECONNECT_INTERVAL))
        {
            mqttWasConnected = false;
            lastMqttReconnect = now;
            mqttService->connect();
        }

        mqttService->loop();

        bool mqttConnected = mqttService->isConnected();
        if (mqttConnected && !mqttWasConnected)
        {
            publishBootstrapTopics();
        }
        mqttWasConnected = mqttConnected;
    }

    if (now - lastSensorRead >= systemConfig.sensorReadInterval)
    {
        lastSensorRead = now;
        if (sensorManager.readSensors())
        {
            hasValidSensorSnapshot = true;
            const SensorSnapshot& snapshot = sensorManager.getSnapshot();
            dataLogger.logSensorData(snapshot);
            if (mqttService != nullptr && mqttService->isConnected())
            {
                mqttService->publishSensorData(snapshot);
            }
        }
    }

    if (now - lastControlUpdate >= systemConfig.climateControlInterval)
    {
        lastControlUpdate = now;
        if (activeProfile != nullptr && hasValidSensorSnapshot)
        {
            const SensorSnapshot& snapshot = sensorManager.getSnapshot();
            CommandQueue commands;
            AutoControlSystem::evaluateAndControl(*activeProfile, snapshot, commands);

            for (uint8_t i = 0; i < commands.count; ++i)
            {
                relayManager.setRelay(commands.commands[i].relayIndex, commands.commands[i].state);
            }

            publishAllRelayStates();
        }
    }

    irrigationManager.loop();

    if (mqttService != nullptr && mqttService->isConnected() &&
        (now - lastStatusPublish >= PUBLISH_STATUS_INTERVAL))
    {
        lastStatusPublish = now;
        mqttService->publishStatus(true);
        mqttService->publishUptime((now - bootMillis) / 1000U);
        publishAllRelayStates();
        if (activeProfile != nullptr)
        {
            mqttService->publishCurrentCrop(activeProfile->name);
        }
    }

    delay(10);
}
