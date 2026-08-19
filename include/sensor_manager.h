#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <ModbusMaster.h>

#include "auto_control.h"

class SensorManager
{
public:
    SensorManager();

    void begin();
    bool update();

    const SensorSnapshot &getSnapshot() const;
    bool hasValidSnapshot() const;
    bool isDhtHealthy() const;
    bool isRs485Healthy() const;
    String getLastError() const;

private:
    DHT dht;
    HardwareSerial &rs485Serial;
    ModbusMaster modbus;
    SensorSnapshot lastSnapshot = {};
    bool snapshotValid = false;
    bool dhtHealthy = false;
    bool rs485Healthy = false;
    String lastError;

    static SensorManager *activeInstance;

    static void preTransmission();
    static void postTransmission();
};