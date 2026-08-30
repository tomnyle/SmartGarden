#include "sensor_manager.h"

#include <Arduino.h>
#include <DHT.h>
#include <ModbusMaster.h>

#include "app_config.h"
#include "pins.h"

static DHT g_dht(DHT_PIN, DHT_TYPE);
static ModbusMaster g_modbus;

static void preTransmission() {
    digitalWrite(RS485_DE, HIGH);
}

static void postTransmission() {
    digitalWrite(RS485_DE, LOW);
}

SensorManager::SensorManager()
    : lastReadTime(0), readInterval(SENSOR_READ_INTERVAL)
{
    memset(&currentSnapshot, 0, sizeof(SensorSnapshot));
}

void SensorManager::begin()
{
    g_dht.begin();

    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW);

    Serial2.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX, RS485_TX);
    g_modbus.begin(1, Serial2);
    g_modbus.preTransmission(preTransmission);
    g_modbus.postTransmission(postTransmission);

    Serial.println("[SensorManager] Initialized DHT22 + RS485 Modbus");
}

bool SensorManager::readSensors()
{
    unsigned long now = millis();
    if (now - lastReadTime < readInterval) {
        return false;
    }
    lastReadTime = now;

    float humidity = g_dht.readHumidity();
    float temperature = g_dht.readTemperature();

    if (!isnan(humidity) && !isnan(temperature)) {
        currentSnapshot.airTemp = temperature;
        currentSnapshot.airHumidity = humidity;
    } else {
        Serial.println("[SensorManager] DHT22 read failed");
    }

    readRS485Sensors();
    currentSnapshot.timestamp = now;
    return true;
}

void SensorManager::readRS485Sensors()
{
    uint8_t result = g_modbus.readHoldingRegisters(0x0000, 40);
    if (result != g_modbus.ku8MBSuccess) {
        Serial.printf("[SensorManager] Modbus read failed: %u\n", result);
        return;
    }

    currentSnapshot.soilMoisture = g_modbus.getResponseBuffer(0) / 10.0f;
    currentSnapshot.soilTemp = g_modbus.getResponseBuffer(1) / 10.0f;
    currentSnapshot.ph = g_modbus.getResponseBuffer(3) / 10.0f;
    currentSnapshot.nitrogen = g_modbus.getResponseBuffer(4);
    currentSnapshot.phosphorus = g_modbus.getResponseBuffer(5);
    currentSnapshot.potassium = g_modbus.getResponseBuffer(6);
    currentSnapshot.ec = g_modbus.getResponseBuffer(9);
}

const SensorSnapshot& SensorManager::getSnapshot() const
{
    return currentSnapshot;
}

void SensorManager::printSnapshot() const
{
    Serial.println("========== Sensor Snapshot ==========");
    Serial.printf("Air Temp     : %.1f C\n", currentSnapshot.airTemp);
    Serial.printf("Air Humidity : %.1f %%\n", currentSnapshot.airHumidity);
    Serial.printf("Soil Moisture: %.1f %%\n", currentSnapshot.soilMoisture);
    Serial.printf("Soil Temp    : %.1f C\n", currentSnapshot.soilTemp);
    Serial.printf("pH           : %.1f\n", currentSnapshot.ph);
    Serial.printf("EC           : %u uS/cm\n", currentSnapshot.ec);
    Serial.printf("Nitrogen     : %u mg/kg\n", currentSnapshot.nitrogen);
    Serial.printf("Phosphorus   : %u mg/kg\n", currentSnapshot.phosphorus);
    Serial.printf("Potassium    : %u mg/kg\n", currentSnapshot.potassium);
    Serial.println("=====================================");
}
