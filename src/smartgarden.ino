#include <Arduino.h>

#include "auto_control.h"
#include "config.h"
#include "crop_profiles.h"
#include "mqtt_handler.h"
#include "pins.h"

CropProfileStore cropStore;
AutoControlEngine autoControl;
MqttHandler mqttHandler;

SensorSnapshot dataLog[DATA_LOG_CAPACITY]{};
size_t dataLogSize = 0;
size_t dataLogCursor = 0;

bool relayStates[RELAY_COUNT]{};
unsigned long lastSampleAt = 0;
unsigned long lastDashboardAt = 0;

SensorSnapshot readSensors()
{
    const unsigned long now = millis();
    return {
        22.0F + static_cast<float>((now / 7000UL) % 5),
        60.0F,
        52.0F - static_cast<float>((now / 9000UL) % 8),
        6.0F,
        1.6F,
        180.0F,
        65.0F,
        210.0F,
        now};
}

void logData(const SensorSnapshot &snapshot)
{
    dataLog[dataLogCursor] = snapshot;
    dataLogCursor = (dataLogCursor + 1) % DATA_LOG_CAPACITY;
    if (dataLogSize < DATA_LOG_CAPACITY)
    {
        ++dataLogSize;
    }
}

void applyRelayCommand(const RelayCommand &command)
{
    if (command.relayIndex >= RELAY_COUNT)
    {
        return;
    }

    relayStates[command.relayIndex] = command.turnOn;
    Serial.printf("[relay] relay_%u => %s\n", command.relayIndex, command.turnOn ? "ON" : "OFF");
}

void printAlerts(const AutoControlResult &result)
{
    for (size_t i = 0; i < result.alertCount; ++i)
    {
        const AlertMessage &alert = result.alerts[i];
        Serial.printf(
            "[alert] %s=%.2f out_of_range [%.2f, %.2f]\n",
            alert.metric,
            alert.value,
            alert.expected.min,
            alert.expected.max);
    }
}

void printDashboard(const SensorSnapshot &snapshot)
{
    const CropProfile *active = cropStore.getActive();
    Serial.println("------------------------------");
    Serial.printf("Device: %s (%s)\n", SMARTGARDEN_DEVICE_NAME, SMARTGARDEN_DEVICE_ID);
    Serial.printf("WiFi SSID: %s\n", SMARTGARDEN_WIFI_SSID);
    Serial.printf("MQTT Broker: %s:%d\n", SMARTGARDEN_MQTT_HOST, SMARTGARDEN_MQTT_PORT);
    Serial.printf("Active crop: %s\n", active != nullptr ? active->name : "none");
    Serial.printf("Temp: %.2f | AirH: %.2f | SoilH: %.2f | pH: %.2f | EC: %.2f\n",
                  snapshot.temperature,
                  snapshot.airHumidity,
                  snapshot.soilHumidity,
                  snapshot.ph,
                  snapshot.ec);
    Serial.printf("NPK: %.2f / %.2f / %.2f\n", snapshot.nitrogen, snapshot.phosphorus, snapshot.potassium);
    Serial.printf("Data log entries: %u\n", static_cast<unsigned>(dataLogSize));
    Serial.println("------------------------------");
}

void processLocalMqttSimulation()
{
    static bool simulated = false;
    if (simulated)
    {
        return;
    }

    simulated = true;
    String response;
    const String topic = mqttTopic("crop/select");
    if (mqttHandler.handleMessage(topic, "tomato", cropStore, response))
    {
        Serial.printf("[mqtt] %s => %s\n", topic.c_str(), response.c_str());
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("      SMART GARDEN");
    Serial.println("==============================");

    for (size_t i = 0; i < RELAY_COUNT; ++i)
    {
        pinMode(RELAY_PINS[i], OUTPUT);
        relayStates[i] = false;
    }

    if (!cropStore.load())
    {
        cropStore.loadDefaults();
    }

    Serial.printf("[boot] loaded %u crop profiles\n", static_cast<unsigned>(cropStore.count()));
}

void loop()
{
    const unsigned long now = millis();

    if (now - lastSampleAt >= SENSOR_UPDATE_MS)
    {
        lastSampleAt = now;

        const SensorSnapshot snapshot = readSensors();
        logData(snapshot);

        const CropProfile *active = cropStore.getActive();
        if (active != nullptr)
        {
            const AutoControlResult result = autoControl.evaluate(*active, snapshot, relayStates);
            for (size_t i = 0; i < result.commandCount; ++i)
            {
                applyRelayCommand(result.commands[i]);
            }
            printAlerts(result);
        }

        if (now - lastDashboardAt >= DASHBOARD_UPDATE_MS)
        {
            lastDashboardAt = now;
            printDashboard(snapshot);
        }
    }

    processLocalMqttSimulation();
}
