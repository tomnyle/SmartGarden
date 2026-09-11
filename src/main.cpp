#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "app_config.h"
#include "pins.h"
#include "network_service.h"
#include "relay_manager.h"
#include "sensor_manager.h"
#include "climate_manager.h"
#include "irrigation_manager.h"
#include "data_logger.h"
#include "mqtt_service.h"
#include "garden_profile.h"

DHT dht(DHT_PIN, DHT_TYPE);

static WiFiClient wifiClient;
static PubSubClient pubSubClient(wifiClient);

static NetworkService networkService(WIFI_SSID, WIFI_PASSWORD);
static RelayManager relayManager;
static SensorManager sensorManager;
static ClimateManager climateManager;
static IrrigationManager irrigationManager;
static DataLogger dataLogger;
static MQTTService mqttService(MQTT_BROKER, MQTT_PORT);

static const CropProfile* currentProfile = nullptr;
static bool lastMqttConnected = false;
static unsigned long lastStatusPublish = 0;

static void publishRetainedState()
{
    if (!mqttService.isConnected()) return;

    mqttService.publishDiscoveryMessages();
    mqttService.publishStatus("online");
    mqttService.publishCropList();
    mqttService.publishAllRelayStatus(&relayManager);
    mqttService.publishSensorData(sensorManager.getSnapshot());
    if (currentProfile) mqttService.publishCurrentCrop(currentProfile);
}

static void onRelayCommand(uint8_t relayIndex, bool state)
{
    if (!relayManager.setRelay(relayIndex, state)) return;
    mqttService.publishRelayStatus(relayIndex, state);
}

static void onCropSelect(const char* cropName)
{
    const CropProfile* profile = CropProfileStore::getCropByName(cropName);
    if (!profile) {
        Serial.printf("[Main] Unknown crop requested: %s\n", cropName);
        return;
    }

    currentProfile = profile;
    climateManager.setCurrentProfile(currentProfile);
    mqttService.publishCurrentCrop(currentProfile);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========== SmartGarden Startup ==========");
    dht.begin();
    randomSeed(millis());

    CropProfileStore::initialize();
    currentProfile = CropProfileStore::getCropById(1);

    relayManager.begin();
    sensorManager.begin();
    climateManager.begin(&relayManager);
    climateManager.setCurrentProfile(currentProfile);
    irrigationManager.begin(&relayManager);
    dataLogger.begin();

    networkService.begin();

    mqttService.setClient(&pubSubClient);
    mqttService.setRelayCommandCallback(onRelayCommand);
    mqttService.setCropSelectCallback(onCropSelect);
    mqttService.begin(MQTT_USERNAME, MQTT_PASSWORD);
    if (mqttService.connect()) {
        publishRetainedState();
        lastMqttConnected = true;
    }
}

void loop()
{
    networkService.loop();
    mqttService.loop();
    irrigationManager.loop();

    bool mqttConnected = mqttService.isConnected();
    if (mqttConnected && !lastMqttConnected) {
        Serial.println("[Main] MQTT reconnected, republishing retained state");
        publishRetainedState();
    }
    lastMqttConnected = mqttConnected;

    if (sensorManager.readSensors()) {
        const SensorSnapshot& snapshot = sensorManager.getSnapshot();
        climateManager.control(snapshot);
        dataLogger.logSensorData(snapshot);

        if (mqttConnected) {
            mqttService.publishSensorData(snapshot);
            mqttService.publishAllRelayStatus(&relayManager);
        }
    }

    unsigned long now = millis();
    if (mqttConnected && (now - lastStatusPublish >= PUBLISH_STATUS_INTERVAL)) {
        mqttService.publishStatus("online");
        mqttService.publishUptime(now);
        lastStatusPublish = now;
    }

    delay(10);
}
