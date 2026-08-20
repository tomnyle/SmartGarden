#include "ModbusMaster.h"

ModbusMaster::ModbusMaster()
    : u8id(0), serial(nullptr), u16responseBufferLength(0),
      preTransmissionCallback(nullptr), postTransmissionCallback(nullptr)
{
}

ModbusMaster::~ModbusMaster()
{
}

void ModbusMaster::begin(uint8_t u8id_val, HardwareSerial& serial_ref)
{
    u8id = u8id_val;
    serial = &serial_ref;
}

void ModbusMaster::preTransmission(void (*f)())
{
    preTransmissionCallback = f;
}

void ModbusMaster::postTransmission(void (*f)())
{
    postTransmissionCallback = f;
}

uint16_t ModbusMaster::crc16Update(uint16_t crc, uint8_t a)
{
    crc ^= a;
    for (int i = 0; i < 8; ++i)
    {
        if (crc & 1)
            crc = (crc >> 1) ^ 0xA001;
        else
            crc = (crc >> 1);
    }
    return crc;
}

uint16_t ModbusMaster::calculateCRC16(uint8_t *buffer, uint16_t buffer_length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < buffer_length; i++)
    {
        crc = crc16Update(crc, buffer[i]);
    }
    return crc;
}

uint8_t ModbusMaster::readHoldingRegisters(uint16_t u16ReadAddress, uint16_t u16Quantity)
{
    if (!serial) return ku8MBServerFailure;

    uint8_t u8ModbusADU[8];
    uint8_t u8ModbusADUSize = 0;
    uint8_t u8CRC[] = {0, 0};

    // Build request
    u8ModbusADU[u8ModbusADUSize++] = u8id;
    u8ModbusADU[u8ModbusADUSize++] = 0x03;  // Read Holding Registers
    u8ModbusADU[u8ModbusADUSize++] = highByte(u16ReadAddress);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(u16ReadAddress);
    u8ModbusADU[u8ModbusADUSize++] = highByte(u16Quantity);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(u16Quantity);

    // Calculate CRC
    uint16_t crc16 = calculateCRC16(u8ModbusADU, u8ModbusADUSize);
    u8ModbusADU[u8ModbusADUSize++] = lowByte(crc16);
    u8ModbusADU[u8ModbusADUSize++] = highByte(crc16);

    // Transmit
    if (preTransmissionCallback) preTransmissionCallback();

    serial->write(u8ModbusADU, u8ModbusADUSize);
    delay(10);

    if (postTransmissionCallback) postTransmissionCallback();

    // Wait for response
    unsigned long u32time = millis();
    while (!serial->available() && millis() - u32time < 5000);

    if (!serial->available()) return ku8MBServerFailure;

    // Read response
    uint8_t u8response[256];
    uint16_t u8responseSize = 0;

    while (serial->available() && u8responseSize < 256)
    {
        u8response[u8responseSize++] = serial->read();
    }

    if (u8responseSize < 5) return ku8MBServerFailure;

    // Check CRC
    uint16_t crc16Response = calculateCRC16(u8response, u8responseSize - 2);
    if (lowByte(crc16Response) != u8response[u8responseSize - 2] ||
        highByte(crc16Response) != u8response[u8responseSize - 1])
    {
        return ku8MBServerFailure;
    }

    // Parse response
    u16responseBufferLength = 0;
    uint8_t byteCount = u8response[2];

    for (int i = 0; i < byteCount / 2; i++)
    {
        au16responseBuffer[i] = word(u8response[3 + i * 2], u8response[4 + i * 2]);
        u16responseBufferLength++;
    }

    return ku8MBSuccess;
}

uint16_t ModbusMaster::getResponseBuffer(uint16_t u16BufferIndex)
{
    if (u16BufferIndex < u16responseBufferLength)
    {
        return au16responseBuffer[u16BufferIndex];
    }
    return 0;
}
