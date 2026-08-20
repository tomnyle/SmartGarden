#include "data_logger.h"
#include <Arduino.h>

DataLogger::DataLogger() : entryCount(0), lastLogTime(0)
{
    memset(logBuffer, 0, sizeof(logBuffer));
}

void DataLogger::begin()
{
    Serial.println("[DataLogger] Initialized");
    entryCount = 0;
}

bool DataLogger::logSensorData(const SensorSnapshot& snapshot)
{
    unsigned long now = millis();
    
    // Check if enough time has passed since last log
    if (now - lastLogTime < SENSOR_READ_INTERVAL)
    {
        return false;
    }
    
    lastLogTime = now;
    
    // Check if buffer is full
    if (entryCount >= LOG_BUFFER_SIZE)
    {
        Serial.println("[DataLogger] Buffer full, overwriting oldest entries");
        // Shift entries to the left
        memmove(&logBuffer[0], &logBuffer[1], sizeof(DataLogEntry) * (LOG_BUFFER_SIZE - 1));
        entryCount = LOG_BUFFER_SIZE - 1;
    }
    
    // Create new entry
    DataLogEntry entry;
    entry.timestamp = now / 1000; // Convert to seconds
    entry.airTemp = snapshot.airTemp;
    entry.airHumidity = snapshot.airHumidity;
    entry.soilMoisture = snapshot.soilMoisture;
    entry.soilTemp = snapshot.soilTemp;
    entry.ph = snapshot.ph;
    entry.ec = snapshot.ec;
    entry.nitrogen = snapshot.nitrogen;
    entry.phosphorus = snapshot.phosphorus;
    entry.potassium = snapshot.potassium;
    
    // Add to buffer
    logBuffer[entryCount] = entry;
    entryCount++;
    
    Serial.printf("[DataLogger] Logged entry #%u at %lu\n", entryCount, entry.timestamp);
    return true;
}

uint32_t DataLogger::getEntryCount() const
{
    return entryCount;
}

void DataLogger::clearLogs()
{
    memset(logBuffer, 0, sizeof(logBuffer));
    entryCount = 0;
    Serial.println("[DataLogger] All logs cleared");
}

String DataLogger::exportAsJSON() const
{
    String json = "[";
    
    for (uint32_t i = 0; i < entryCount; i++)
    {
        if (i > 0) json += ",";
        
        json += "{";
        json += "\"timestamp\":" + String(logBuffer[i].timestamp) + ",";
        json += "\"airTemp\":" + String(logBuffer[i].airTemp, 1) + ",";
        json += "\"airHumidity\":" + String(logBuffer[i].airHumidity, 1) + ",";
        json += "\"soilMoisture\":" + String(logBuffer[i].soilMoisture, 1) + ",";
        json += "\"soilTemp\":" + String(logBuffer[i].soilTemp, 1) + ",";
        json += "\"ph\":" + String(logBuffer[i].ph, 1) + ",";
        json += "\"ec\":" + String(logBuffer[i].ec) + ",";
        json += "\"nitrogen\":" + String(logBuffer[i].nitrogen) + ",";
        json += "\"phosphorus\":" + String(logBuffer[i].phosphorus) + ",";
        json += "\"potassium\":" + String(logBuffer[i].potassium);
        json += "}";
    }
    
    json += "]";
    return json;
}

void DataLogger::printAllLogs() const
{
    Serial.println("\n========== Data Logger ==========");
    Serial.printf("Total entries: %u\n", entryCount);
    
    for (uint32_t i = 0; i < entryCount; i++)
    {
        Serial.printf("Entry %u (Time: %lu):\n", i + 1, logBuffer[i].timestamp);
        Serial.printf("  Air Temp    : %.1f°C\n", logBuffer[i].airTemp);
        Serial.printf("  Air Humidity: %.1f%%\n", logBuffer[i].airHumidity);
        Serial.printf("  Soil Moisture: %.1f%%\n", logBuffer[i].soilMoisture);
        Serial.printf("  Soil Temp   : %.1f°C\n", logBuffer[i].soilTemp);
        Serial.printf("  pH          : %.1f\n", logBuffer[i].ph);
        Serial.printf("  EC          : %u µS/cm\n", logBuffer[i].ec);
        Serial.printf("  N           : %u mg/kg\n", logBuffer[i].nitrogen);
        Serial.printf("  P           : %u mg/kg\n", logBuffer[i].phosphorus);
        Serial.printf("  K           : %u mg/kg\n", logBuffer[i].potassium);
    }
    
    Serial.println("================================\n");
}
