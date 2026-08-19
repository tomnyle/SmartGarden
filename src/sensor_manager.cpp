#include "sensor_manager.h"

// Static pointers used in pre/post-transmission callbacks
static SensorManager *_sensorManagerInstance = nullptr;

SensorManager::SensorManager(uint8_t dhtPin, uint8_t rs485Rx, uint8_t rs485Tx, uint8_t rs485De)
    : _dht(dhtPin, DHT22),
      _rs485Serial(2),
      _rs485Rx(rs485Rx),
      _rs485Tx(rs485Tx),
      _rs485De(rs485De),
      _dhtValid(false),
      _rs485Valid(false)
{
    memset(&_snapshot, 0, sizeof(_snapshot));
    _sensorManagerInstance = this;
}

void SensorManager::begin()
{
    _dht.begin();
    Serial.println("[SensorManager] DHT22 initialised");

    pinMode(_rs485De, OUTPUT);
    digitalWrite(_rs485De, LOW);

    _rs485Serial.begin(RS485_BAUDRATE, SERIAL_8N1, _rs485Rx, _rs485Tx);
    _node.begin(SOIL_SENSOR_ID, _rs485Serial);

    _node.preTransmission([]() {
        if (_sensorManagerInstance)
            _sensorManagerInstance->preTransmission();
    });
    _node.postTransmission([]() {
        if (_sensorManagerInstance)
            _sensorManagerInstance->postTransmission();
    });

    Serial.println("[SensorManager] RS485 Modbus initialised");
}

void SensorManager::preTransmission()
{
    digitalWrite(_rs485De, HIGH);
}

void SensorManager::postTransmission()
{
    digitalWrite(_rs485De, LOW);
}

void SensorManager::readSensors()
{
    float temp = _dht.readTemperature();
    float hum  = _dht.readHumidity();

    if (isnan(temp) || isnan(hum))
    {
        Serial.println("[SensorManager] DHT22 read error");
        _dhtValid = false;
    }
    else
    {
        _snapshot.airTemp     = temp;
        _snapshot.airHumidity = hum;
        _dhtValid             = true;
        Serial.printf("[SensorManager] DHT22 - Temp: %.1f C, Hum: %.1f %%\n", temp, hum);
    }

    readRS485Sensors();

    _snapshot.timestamp = millis();
}

void SensorManager::readRS485Sensors()
{
    uint8_t result = _node.readHoldingRegisters(0x0000, 10);

    if (result == ModbusMaster::ku8MBSuccess)
    {
        _snapshot.soilMoisture = _node.getResponseBuffer(0) / 10.0f;
        _snapshot.soilTemp     = _node.getResponseBuffer(1) / 10.0f;
        _snapshot.ph           = _node.getResponseBuffer(3) / 10.0f;
        _snapshot.nitrogen     = _node.getResponseBuffer(4);
        _snapshot.phosphorus   = _node.getResponseBuffer(5);
        _snapshot.potassium    = _node.getResponseBuffer(6);
        _snapshot.ec           = _node.getResponseBuffer(9);
        _rs485Valid            = true;

        Serial.printf("[SensorManager] RS485 - Moisture: %.1f %%, SoilTemp: %.1f C, "
                      "pH: %.1f, EC: %u, N: %u, P: %u, K: %u\n",
                      _snapshot.soilMoisture, _snapshot.soilTemp, _snapshot.ph,
                      _snapshot.ec, _snapshot.nitrogen,
                      _snapshot.phosphorus, _snapshot.potassium);
    }
    else
    {
        Serial.printf("[SensorManager] RS485 Modbus error: 0x%02X\n", result);
        _rs485Valid = false;
    }
}

SensorSnapshot SensorManager::getSnapshot() const
{
    return _snapshot;
}
