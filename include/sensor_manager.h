#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <ModbusMaster.h>
#include "auto_control.h"
#include "app_config.h"

// ========================================
// SensorManager - reads DHT22 and RS485 Modbus sensors
// ========================================
class SensorManager
{
public:
    // dhtPin   : GPIO pin for DHT22 data line
    // rs485Rx  : UART RX pin for RS485
    // rs485Tx  : UART TX pin for RS485
    // rs485De  : DE/RE control pin for MAX485
    SensorManager(uint8_t dhtPin, uint8_t rs485Rx, uint8_t rs485Tx, uint8_t rs485De);

    void begin();

    // Read DHT22 and RS485 sensors and update internal snapshot
    void readSensors();
    void readRS485Sensors();

    // Return a copy of the latest sensor snapshot
    SensorSnapshot getSnapshot() const;

    bool isDHTValid()    const { return _dhtValid; }
    bool isRS485Valid()  const { return _rs485Valid; }

private:
    DHT            _dht;
    HardwareSerial _rs485Serial;
    ModbusMaster   _node;

    uint8_t _rs485Rx;
    uint8_t _rs485Tx;
    uint8_t _rs485De;

    SensorSnapshot _snapshot;
    bool           _dhtValid;
    bool           _rs485Valid;

    void preTransmission();
    void postTransmission();
};
