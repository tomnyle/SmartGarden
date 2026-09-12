#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include "app_config.h"
#include "pins.h"
#include "network_service.h"
#include "mqtt_service.h"
#include "sensor_manager.h"
#include "relay_manager.h"
#include "garden_profile.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHT_PIN, DHT_TYPE);

namespace {
NetworkService networkService(WIFI_SSID, WIFI_PASSWORD);
MQTTService mqttService(MQTT_BROKER, MQTT_PORT);
SensorManager sensorManager;
RelayManager relayManager;
const CropProfile* currentCrop = nullptr;
unsigned long lastStatusPublish = 0;

void publishRetainedState()
{
    mqttService.publishStatus("online");
    mqttService.publishSensorData(sensorManager.getSnapshot());
    mqttService.publishAllRelayStatus(&relayManager);
    mqttService.publishUptime(millis());

    if (currentCrop)
    {
        mqttService.publishCurrentCrop(currentCrop);
    }
}

bool ensureMqttConnected()
{
    static bool sessionInitialized = false;

    if (!networkService.isConnected())
    {
        sessionInitialized = false;
        return false;
    }

    if (!mqttService.isConnected())
    {
        sessionInitialized = false;
        if (!mqttService.connect())
        {
            return false;
        }
    }

    if (!sessionInitialized)
    {
        Serial.println("[SmartGarden] MQTT session ready, publishing retained SmartGarden state");
        publishRetainedState();
        sessionInitialized = true;
    }

    return true;
}
}

void handleRelayCommand(uint8_t relayIndex, bool state)
{
    if (relayManager.setRelay(relayIndex, state))
    {
        mqttService.publishRelayStatus(relayIndex, state);
    }
}

void handleCropSelect(const char* cropName)
{
    const CropProfile* profile = CropProfileStore::getCropByName(cropName);
    if (!profile)
    {
        Serial.printf("[SmartGarden] Ignoring unknown crop profile: %s\n", cropName);
        return;
    }

    currentCrop = profile;
    Serial.printf("[SmartGarden] Selected crop profile: %s\n", currentCrop->name);
    mqttService.publishCurrentCrop(currentCrop);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========== SmartGarden Startup ==========");
    randomSeed(static_cast<uint32_t>(micros()));

    relayManager.begin();
    sensorManager.begin();

    CropProfileStore::initialize();
    currentCrop = CropProfileStore::getCropById(1);

    mqttService.setClient(&mqttClient);
    mqttService.begin(MQTT_USERNAME, MQTT_PASSWORD);
    mqttService.setRelayCommandCallback(handleRelayCommand);
    mqttService.setCropSelectCallback(handleCropSelect);

    networkService.begin();
    ensureMqttConnected();

    Serial.println("========== Setup Complete ==========\n");
}

void loop()
{
    networkService.loop();

    if (!ensureMqttConnected())
    {
        delay(10);
        return;
    }

    mqttService.loop();

    if (sensorManager.readSensors())
    {
        mqttService.publishSensorData(sensorManager.getSnapshot());
        sensorManager.printSnapshot();
    }

    unsigned long now = millis();
    if (now - lastStatusPublish >= PUBLISH_STATUS_INTERVAL)
    {
        lastStatusPublish = now;
        mqttService.publishStatus("online");
        mqttService.publishUptime(now);
    }

    delay(10);
}
