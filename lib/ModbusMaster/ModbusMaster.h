#ifndef MODBUSMASTER_H
#define MODBUSMASTER_H

#include <Arduino.h>
#include <HardwareSerial.h>

class ModbusMaster
{
public:
    static const uint8_t ku8MBSuccess = 0;
    static const uint8_t ku8MBIllegalFunction = 1;
    static const uint8_t ku8MBIllegalDataAddress = 2;
    static const uint8_t ku8MBIllegalDataValue = 3;
    static const uint8_t ku8MBDeviceFailure = 4;
    static const uint8_t ku8MBServerFailure = 1;

    ModbusMaster();
    ~ModbusMaster();

    void begin(uint8_t u8id, HardwareSerial& serial);
    void preTransmission(void (*f)());
    void postTransmission(void (*f)());

    uint8_t readHoldingRegisters(uint16_t u16ReadAddress, uint16_t u16Quantity);
    uint16_t getResponseBuffer(uint16_t u16BufferIndex);

private:
    uint8_t u8id;
    HardwareSerial* serial;
    uint16_t au16responseBuffer[256];
    uint16_t u16responseBufferLength;
    void (*preTransmissionCallback)();
    void (*postTransmissionCallback)();

    uint16_t crc16Update(uint16_t crc, uint8_t a);
    uint16_t calculateCRC16(uint8_t *buffer, uint16_t buffer_length);
};

#endif
