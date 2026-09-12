#include <Arduino.h>
#include <DHT.h>

#include "app_config.h"
#include "pins.h"
#include "network_service.h"
#include "mqtt_service.h"
#include "sensor_manager.h"
#include "relay_manager.h"
#include "climate_manager.h"
#include "garden_profile.h"
#include "discovery_service.h"

DHT dht(DHT_PIN, DHT_TYPE);

namespace {

NetworkService network(WIFI_SSID, WIFI_PASSWORD);
MQTTService mqtt(MQTT_BROKER, MQTT_PORT);
SensorManager sensors;
RelayManager relays;
ClimateManager climate;
DiscoveryService discovery(mqtt);
const CropProfile* currentCrop = nullptr;

void onRelayCommand(uint8_t relayIndex, bool state)
{
    if (relays.setRelay(relayIndex, state)) {
        mqtt.publishRelayStatus(relayIndex, state);
    }
}

void onCropSelected(const char* cropName)
{
    const CropProfile* nextCrop = CropProfileStore::getCropByName(cropName);
    if (nextCrop == nullptr) {
        Serial.printf("[Main] Unknown crop selection: %s\n", cropName);
        if (currentCrop != nullptr) {
            mqtt.publishCurrentCrop(currentCrop);
        }
        return;
    }

    currentCrop = nextCrop;
    climate.setCurrentProfile(currentCrop);
    mqtt.publishCurrentCrop(currentCrop);
    Serial.printf("[Main] Crop changed to: %s\n", currentCrop->name);
}

void publishRuntimeState()
{
    const SensorSnapshot& snapshot = sensors.getSnapshot();
    climate.control(snapshot);
    mqtt.publishSensorData(snapshot);
    mqtt.publishAllRelayStatus(&relays);
    mqtt.publishCropList();
    if (currentCrop != nullptr) {
        mqtt.publishCurrentCrop(currentCrop);
    }
    mqtt.publishUptime(millis());
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("[BOOT] SmartGarden starting");
    dht.begin();

    CropProfileStore::initialize();
    currentCrop = CropProfileStore::getCropById(1);

    relays.begin();
    sensors.begin();
    climate.begin(&relays);
    if (currentCrop != nullptr) {
        climate.setCurrentProfile(currentCrop);
    }

    network.begin();

    mqtt.setRelayCommandCallback(onRelayCommand);
    mqtt.setCropSelectCallback(onCropSelected);
    mqtt.attachStateProviders(&sensors, &relays, &currentCrop);
    mqtt.begin(MQTT_USERNAME, MQTT_PASSWORD);
}

void loop()
{
    network.loop();
    mqtt.loop();

    if (mqtt.consumeConnectEvent()) {
        discovery.begin();
    }

    if (sensors.readSensors()) {
        publishRuntimeState();
    }

    delay(10);
}
