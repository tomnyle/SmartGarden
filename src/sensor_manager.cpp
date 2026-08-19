#include "sensor_manager.h"

#include "config.h"
#include "pins.h"

SensorManager *SensorManager::activeInstance = nullptr;

SensorManager::SensorManager()
    : dht(PIN_DHT22, DHT22), rs485Serial(Serial2)
{
}

void SensorManager::begin()
{
    activeInstance = this;

    dht.begin();

    pinMode(PIN_RS485_RE_DE, OUTPUT);
    digitalWrite(PIN_RS485_RE_DE, LOW);

    rs485Serial.begin(RS485_BAUDRATE, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
    modbus.begin(SOIL_SENSOR_ID, rs485Serial);
    modbus.preTransmission(preTransmission);
    modbus.postTransmission(postTransmission);

    lastSnapshot.timestamp = millis();
    Serial.println("Sensor manager initialized");
}

bool SensorManager::update()
{
    SensorSnapshot nextSnapshot = lastSnapshot;
    nextSnapshot.timestamp = millis();
    lastError = "";

    const float airTemp = dht.readTemperature();
    const float airHumidity = dht.readHumidity();

    dhtHealthy = !isnan(airTemp) && !isnan(airHumidity);
    if (dhtHealthy)
    {
        nextSnapshot.airTemp = airTemp;
        nextSnapshot.airHumidity = airHumidity;
    }
    else
    {
        lastError += "DHT22 read failed. ";
    }

    const uint8_t result = modbus.readHoldingRegisters(0x0000, 10);
    rs485Healthy = (result == modbus.ku8MBSuccess);
    if (rs485Healthy)
    {
        nextSnapshot.soilMoisture = modbus.getResponseBuffer(0) / 10.0f;
        nextSnapshot.soilTemp = modbus.getResponseBuffer(1) / 10.0f;
        nextSnapshot.ph = modbus.getResponseBuffer(3) / 10.0f;
        nextSnapshot.nitrogen = modbus.getResponseBuffer(4);
        nextSnapshot.phosphorus = modbus.getResponseBuffer(5);
        nextSnapshot.potassium = modbus.getResponseBuffer(6);
        nextSnapshot.ec = modbus.getResponseBuffer(9);
    }
    else
    {
        lastError += "RS485/Modbus read failed (" + String(result) + ").";
    }

    if (dhtHealthy || rs485Healthy || snapshotValid)
    {
        lastSnapshot = nextSnapshot;
        snapshotValid = true;
        return true;
    }

    if (lastError.isEmpty())
    {
        lastError = "No valid sensor data available.";
    }
    return false;
}

const SensorSnapshot &SensorManager::getSnapshot() const
{
    return lastSnapshot;
}

bool SensorManager::hasValidSnapshot() const
{
    return snapshotValid;
}

bool SensorManager::isDhtHealthy() const
{
    return dhtHealthy;
}

bool SensorManager::isRs485Healthy() const
{
    return rs485Healthy;
}

String SensorManager::getLastError() const
{
    return lastError;
}

void SensorManager::preTransmission()
{
    if (activeInstance != nullptr)
    {
        digitalWrite(PIN_RS485_RE_DE, HIGH);
    }
}

void SensorManager::postTransmission()
{
    if (activeInstance != nullptr)
    {
        digitalWrite(PIN_RS485_RE_DE, LOW);
    }
}