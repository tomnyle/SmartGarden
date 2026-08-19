#include <Arduino.h>

#include "config.h"
#include "crop_profiles.h"
#include "sensor_manager.h"
#include "relay_manager.h"
#include "climate_manager.h"
#include "mqtt_handler.h"

SensorManager sensorManager;
RelayManager relayManager;
ClimateManager climateManager;
CropProfileStore cropStore;
MqttHandler mqttHandler;

unsigned long lastSensorRead = 0;

static void printSnapshot(const CropProfile *activeCrop, const SensorSnapshot &snapshot)
{
    Serial.println("\n===== GARDEN DATA =====");
    Serial.printf("Active Crop: %s\n", activeCrop ? activeCrop->name : "None");
    Serial.printf("Air Temp : %.1f C\n", snapshot.airTemp);
    Serial.printf("Humidity : %.1f %%\n", snapshot.airHumidity);
    Serial.printf("Moisture : %.1f %%\n", snapshot.soilMoisture);
    Serial.printf("SoilTemp : %.1f C\n", snapshot.soilTemp);
    Serial.printf("PH       : %.1f\n", snapshot.ph);
    Serial.printf("EC       : %u uS/cm\n", snapshot.ec);
    Serial.printf("N        : %u mg/kg\n", snapshot.nitrogen);
    Serial.printf("P        : %u mg/kg\n", snapshot.phosphorus);
    Serial.printf("K        : %u mg/kg\n", snapshot.potassium);
    Serial.println("=======================");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("      SMART GARDEN");
    Serial.println("==============================");

    if (!cropStore.load())
    {
        cropStore.save();
    }

    relayManager.begin();
    climateManager.begin(relayManager);
    sensorManager.begin();
    mqttHandler.begin(relayManager, climateManager, cropStore);

    Serial.println("SmartGarden initialized");
}

void loop()
{
    mqttHandler.loop();

    const unsigned long now = millis();
    if (now - lastSensorRead < SENSOR_UPDATE_MS)
    {
        return;
    }

    lastSensorRead = now;
    if (!sensorManager.update())
    {
        Serial.println(sensorManager.getLastError());
        return;
    }

    const SensorSnapshot &snapshot = sensorManager.getSnapshot();
    const CropProfile *activeCrop = cropStore.getActive();

    if (activeCrop != nullptr)
    {
        climateManager.applyProfile(*activeCrop, snapshot);
    }

    mqttHandler.publishSensorData(snapshot);
    mqttHandler.publishCurrentCropConfig();
    mqttHandler.publishRelayStates();
    printSnapshot(activeCrop, snapshot);
}
