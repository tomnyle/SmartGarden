#include "sensor_manager.h"
#include <Arduino.h>
#include <DHT.h>
#include <HardwareSerial.h>

// Declare as extern - defined in main.cpp
extern DHT dht;

// RS485 Serial (Serial2 on ESP32)
#define RS485_RX 16
#define RS485_TX 17
#define RS485_DE 18

SensorManager::SensorManager()
    : lastReadTime(0), readInterval(5000)
{
    memset(&currentSnapshot, 0, sizeof(SensorSnapshot));
    // Set default values for testing
    currentSnapshot.airTemp = 22.0f;
    currentSnapshot.airHumidity = 65.0f;
    currentSnapshot.soilMoisture = 60.0f;
    currentSnapshot.soilTemp = 20.0f;
    currentSnapshot.ph = 6.8f;
    currentSnapshot.ec = 1200;
    currentSnapshot.nitrogen = 45;
    currentSnapshot.phosphorus = 35;
    currentSnapshot.potassium = 50;
}

void SensorManager::begin()
{
    // Wait for DHT to stabilize (if connected)
    delay(2000);
    
    // Initialize RS485 Serial communication
    Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    
    // Set RS485 direction pin
    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW); // Receive mode by default
    
    Serial.println("[SensorManager] Initialized with default test values");
    Serial.println("[SensorManager] Connect DHT22 to GPIO4 to enable real sensor readings");
}

bool SensorManager::readSensors()
{
    unsigned long now = millis();
    if (now - lastReadTime < readInterval)
    {
        return false; // Not time to read yet
    }
    lastReadTime = now;
    
    // Try to read DHT22
    float humidity = dht.readHumidity(false);
    float temperature = dht.readTemperature(false);
    
    // If DHT22 is connected and reading succeeds, update values
    if (!isnan(humidity) && !isnan(temperature))
    {
        // Validate readings
        if (temperature >= -40 && temperature <= 80 && humidity >= 0 && humidity <= 100)
        {
            currentSnapshot.airTemp = temperature;
            currentSnapshot.airHumidity = humidity;
            Serial.printf("[SensorManager] DHT22 OK: Temp=%.1f°C, Humidity=%.1f%%\n", temperature, humidity);
        }
    }
    else
    {
        // DHT22 not connected, use simulated values
        // Simulate realistic changes
        currentSnapshot.airTemp += (random(-10, 11) / 100.0f);  // ±0.1°C
        currentSnapshot.airHumidity += (random(-5, 6) / 100.0f); // ±0.05%
        
        // Clamp values
        if (currentSnapshot.airTemp < 15.0f) currentSnapshot.airTemp = 15.0f;
        if (currentSnapshot.airTemp > 35.0f) currentSnapshot.airTemp = 35.0f;
        if (currentSnapshot.airHumidity < 40.0f) currentSnapshot.airHumidity = 40.0f;
        if (currentSnapshot.airHumidity > 90.0f) currentSnapshot.airHumidity = 90.0f;
    }
    
    currentSnapshot.timestamp = now;
    
    Serial.printf("[SensorManager] Values: Temp=%.1f°C, Humidity=%.1f%%, Moisture=%.1f%%, EC=%u\n",
                 currentSnapshot.airTemp, currentSnapshot.airHumidity,
                 currentSnapshot.soilMoisture, currentSnapshot.ec);
    
    // Try to read RS485 sensors
    readRS485Sensors();
    
    return true;
}

void SensorManager::readRS485Sensors()
{
    // Send read request to RS485 sensor
    digitalWrite(RS485_DE, HIGH); // Transmit mode
    delay(10);
    
    // Send Modbus RTU request
    uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x08, 0x44, 0x09};
    Serial2.write(request, sizeof(request));
    Serial2.flush();
    
    delay(100);
    digitalWrite(RS485_DE, LOW); // Receive mode
    
    // Wait for response
    delay(100);
    
    if (Serial2.available())
    {
        uint8_t response[19];
        int bytesRead = Serial2.readBytes(response, 19);
        
        if (bytesRead == 19)
        {
            // Parse sensor data
            currentSnapshot.soilMoisture = (response[3] << 8 | response[4]) / 10.0f;
            currentSnapshot.soilTemp = (response[5] << 8 | response[6]) / 10.0f;
            currentSnapshot.ph = (response[7] << 8 | response[8]) / 10.0f;
            currentSnapshot.ec = (response[9] << 8 | response[10]);
            currentSnapshot.nitrogen = (response[11] << 8 | response[12]);
            currentSnapshot.phosphorus = (response[13] << 8 | response[14]);
            currentSnapshot.potassium = (response[15] << 8 | response[16]);
            
            Serial.printf("[SensorManager] RS485 OK: Moisture=%.1f%% pH=%.1f EC=%u\n",
                         currentSnapshot.soilMoisture, currentSnapshot.ph, currentSnapshot.ec);
        }
    }
}

const SensorSnapshot& SensorManager::getSnapshot() const
{
    return currentSnapshot;
}

void SensorManager::printSnapshot() const
{
    Serial.println("=== Sensor Snapshot ===");
    Serial.printf("Air Temp: %.1f°C\n", currentSnapshot.airTemp);
    Serial.printf("Air Humidity: %.1f%%\n", currentSnapshot.airHumidity);
    Serial.printf("Soil Moisture: %.1f%%\n", currentSnapshot.soilMoisture);
    Serial.printf("Soil Temp: %.1f°C\n", currentSnapshot.soilTemp);
    Serial.printf("pH: %.1f\n", currentSnapshot.ph);
    Serial.printf("EC: %u µS/cm\n", currentSnapshot.ec);
    Serial.printf("Nitrogen: %u mg/kg\n", currentSnapshot.nitrogen);
    Serial.printf("Phosphorus: %u mg/kg\n", currentSnapshot.phosphorus);
    Serial.printf("Potassium: %u mg/kg\n", currentSnapshot.potassium);
    Serial.println("=======================");
}