#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "climate_manager.h"
#include "config.h"
#include "crop_profiles.h"
#include "garden_profile.h"
#include "irrigation_manager.h"
#include "mqtt_service.h"
#include "network_service.h"
#include "relay_manager.h"
#include "sensor_manager.h"
#include "zone_manager.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

SystemConfig systemConfig;
RelayManager relayManager;
SensorManager sensorManager;
ClimateManager climateManager;
IrrigationManager irrigationManager;
ZoneManager zoneManager;
NetworkService* networkService = nullptr;
MQTTService* mqttService = nullptr;

const CropProfile* activeCrop = nullptr;

void onRelayCommand(uint8_t relayIndex, bool state)
{
    if (relayManager.setRelay(relayIndex, state) && mqttService) {
        mqttService->publishRelayStatus(relayIndex, state);
    }
}

void onCropSelect(const char* cropName)
{
    const CropProfile* profile = CropProfileStore::getCropByName(cropName);
    if (!profile) {
        Serial.printf("[SmartGarden] Unknown crop profile: %s\n", cropName);
        return;
    }

    activeCrop = profile;
    climateManager.setCurrentProfile(activeCrop);
    if (mqttService) {
        mqttService->publishCurrentCrop(activeCrop);
    }

    Serial.printf("[SmartGarden] Active crop profile: %s\n", activeCrop->name);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== SmartGarden Boot ===");

    systemConfig = getDefaultConfig();
    loadConfig(systemConfig);
    printConfig(systemConfig);

    networkService = new NetworkService(systemConfig.wifiSSID, systemConfig.wifiPassword);
    networkService->begin();

    relayManager.begin();
    sensorManager.begin();

    CropProfileStore::initialize();
    activeCrop = CropProfileStore::getCropById(CROP_TOMATO);
    climateManager.begin(&relayManager);
    if (activeCrop) {
        climateManager.setCurrentProfile(activeCrop);
    }

    irrigationManager.begin(&relayManager, IRRIGATION_RELAY_INDEX);

    zoneManager.begin(&relayManager);
    zoneManager.addZone("Zone 1", 0, activeCrop);
    zoneManager.addZone("Zone 2", 1, activeCrop);

    mqttService = new MQTTService(systemConfig.mqttBroker, systemConfig.mqttPort);
    mqttService->setClient(&mqttClient);
    mqttService->begin(systemConfig.mqttUsername, systemConfig.mqttPassword);
    mqttService->setRelayCommandCallback(onRelayCommand);
    mqttService->setCropSelectCallback(onCropSelect);

    if (mqttService->connect()) {
        mqttService->publishAllRelayStatus(&relayManager);
        if (activeCrop) {
            mqttService->publishCurrentCrop(activeCrop);
        }
    }
}

void loop()
{
    if (networkService) {
        networkService->loop();
    }

    if (mqttService) {
        mqttService->loop();
    }

    if (sensorManager.readSensors()) {
        const SensorSnapshot& snapshot = sensorManager.getSnapshot();
        climateManager.control(snapshot);
        for (uint8_t zoneId = 1; zoneId <= zoneManager.getZoneCount(); zoneId++) {
            zoneManager.controlZone(zoneId, snapshot, &climateManager);
        }

        if (mqttService) {
            mqttService->publishSensorData(snapshot);
            mqttService->publishUptime(millis());
        }
    }

    irrigationManager.loop();
    delay(10);
}
