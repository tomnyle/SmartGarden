#include "sensor_manager.h"
#include <DHT.h>
#include <HardwareSerial.h>

// DHT sensor setup
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// RS485 Serial (Serial2 on ESP32)
#define RS485_RX 16
#define RS485_TX 17
#define RS485_DE 18

SensorManager::SensorManager()
    : lastReadTime(0), readInterval(5000)
{
}

void SensorManager::begin()
{
    // Initialize DHT22
    dht.begin();
    
    // Initialize RS485 Serial communication
    Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    
    // Set RS485 direction pin
    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW); // Receive mode by default
    
    Serial.println("[SensorManager] Initialized");
}

bool SensorManager::readSensors()
{
    unsigned long now = millis();
    if (now - lastReadTime < readInterval)
    {
        return false; // Not time to read yet
    }
    lastReadTime = now;
    
    // Read DHT22
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    
    if (isnan(humidity) || isnan(temperature))
    {
        Serial.println("[SensorManager] ERROR: Failed to read DHT22 sensor!");
        return false;
    }
    
    currentSnapshot.airHumidity = humidity;
    currentSnapshot.airTemp = temperature;
    currentSnapshot.timestamp = now;
    
    // Read RS485 sensors
    readRS485Sensors();
    
    return true;
}

void SensorManager::readRS485Sensors()
{
    // Send read request to RS485 sensor
    // This is a simplified implementation - adjust based on your actual sensor protocol
    
    digitalWrite(RS485_DE, HIGH); // Transmit mode
    delay(10);
    
    // Example: Send request to sensor
    uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x08, 0x44, 0x09};
    Serial2.write(request, sizeof(request));
    
    delay(100);
    digitalWrite(RS485_DE, LOW); // Receive mode
    
    // Read response
    if (Serial2.available())
    {
        uint8_t response[19];
        int bytesRead = Serial2.readBytes(response, 19);
        
        if (bytesRead == 19)
        {
            // Parse sensor data (adjust based on actual sensor protocol)
            currentSnapshot.soilMoisture = (response[3] << 8 | response[4]) / 10.0f;
            currentSnapshot.soilTemp = (response[5] << 8 | response[6]) / 10.0f;
            currentSnapshot.ph = (response[7] << 8 | response[8]) / 10.0f;
            currentSnapshot.ec = (response[9] << 8 | response[10]);
            currentSnapshot.nitrogen = (response[11] << 8 | response[12]);
            currentSnapshot.phosphorus = (response[13] << 8 | response[14]);
            currentSnapshot.potassium = (response[15] << 8 | response[16]);
            
            Serial.printf("[SensorManager] Soil: Moisture=%.1f%% Temp=%.1f°C pH=%.1f EC=%u\n",
                         currentSnapshot.soilMoisture, currentSnapshot.soilTemp,
                         currentSnapshot.ph, currentSnapshot.ec);
        }
        else
        {
            Serial.println("[SensorManager] ERROR: Invalid RS485 response size");
        }
    }
    else
    {
        Serial.println("[SensorManager] WARNING: No data from RS485 sensor");
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
