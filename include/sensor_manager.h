#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <ModbusMaster.h>

#include "auto_control.h"

class SensorManager
{
public:
    SensorManager(uint8_t dhtPin,
                  uint8_t dhtType,
                  HardwareSerial &rs485Serial,
                  ModbusMaster &modbusNode,
                  uint8_t rxPin,
                  uint8_t txPin,
                  uint8_t directionPin,
                  uint32_t baudRate = RS485_BAUDRATE,
                  uint8_t slaveId = SOIL_SENSOR_ID)
        : _dht(dhtPin, dhtType),
          _rs485Serial(rs485Serial),
          _modbusNode(modbusNode),
          _rxPin(rxPin),
          _txPin(txPin),
          _directionPin(directionPin),
          _baudRate(baudRate),
          _slaveId(slaveId)
    {
        _lastSnapshot = {};
    }

    void begin()
    {
        _dht.begin();
        pinMode(_directionPin, OUTPUT);
        digitalWrite(_directionPin, LOW);
        _rs485Serial.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
        _modbusNode.begin(_slaveId, _rs485Serial);
        activeDirectionPin() = _directionPin;
        _modbusNode.preTransmission(preTransmission);
        _modbusNode.postTransmission(postTransmission);
    }

    bool read(SensorSnapshot &snapshot)
    {
        snapshot = {};
        snapshot.airTemp = _dht.readTemperature();
        snapshot.airHumidity = _dht.readHumidity();
        snapshot.timestamp = millis();

        activeInstance() = this;
        uint8_t result = _modbusNode.readHoldingRegisters(0x0000, 10);
        activeInstance() = nullptr;

        if (result == _modbusNode.ku8MBSuccess)
        {
            snapshot.soilMoisture = _modbusNode.getResponseBuffer(0) / 10.0f;
            snapshot.soilTemp = _modbusNode.getResponseBuffer(1) / 10.0f;
            snapshot.ph = _modbusNode.getResponseBuffer(3) / 10.0f;
            snapshot.nitrogen = _modbusNode.getResponseBuffer(4);
            snapshot.phosphorus = _modbusNode.getResponseBuffer(5);
            snapshot.potassium = _modbusNode.getResponseBuffer(6);
            snapshot.ec = _modbusNode.getResponseBuffer(9);
        }

        _lastSnapshot = snapshot;
        return !isnan(snapshot.airTemp) && !isnan(snapshot.airHumidity);
    }

    const SensorSnapshot &lastSnapshot() const
    {
        return _lastSnapshot;
    }

private:
    static SensorManager *&activeInstance()
    {
        static SensorManager *instance = nullptr;
        return instance;
    }

    static void preTransmission()
    {
        SensorManager *instance = activeInstance();
        if (instance != nullptr)
        {
            digitalWrite(instance->_directionPin, HIGH);
        }
    }

    static void postTransmission()
    {
        SensorManager *instance = activeInstance();
        if (instance != nullptr)
        {
            digitalWrite(instance->_directionPin, LOW);
        }
    }

    DHT _dht;
    HardwareSerial &_rs485Serial;
    ModbusMaster &_modbusNode;
    uint8_t _rxPin;
    uint8_t _txPin;
    uint8_t _directionPin;
    uint32_t _baudRate;
    uint8_t _slaveId;
    SensorSnapshot _lastSnapshot;
};