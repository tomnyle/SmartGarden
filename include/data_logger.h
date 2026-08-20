#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>
#include <time.h>
#include <Arduino.h>
#include "sensor_manager.h"
#include "app_config.h"

typedef struct {
    unsigned long timestamp;       // Unix timestamp
    float airTemp;                 // Air temperature
    float airHumidity;             // Air humidity
    float soilMoisture;            // Soil moisture
    float soilTemp;                // Soil temperature
    float ph;                      // Soil pH
    uint16_t ec;                   // Electrical conductivity
    uint16_t nitrogen;             // Nitrogen level
    uint16_t phosphorus;           // Phosphorus level
    uint16_t potassium;            // Potassium level
} DataLogEntry;

class DataLogger {
public:
    DataLogger();
    
    void begin();
    
    // Log sensor data
    bool logSensorData(const SensorSnapshot& snapshot);
    
    // Get number of entries logged
    uint32_t getEntryCount() const;
    
    // Clear all logs
    void clearLogs();
    
    // Export logs to JSON format
    String exportAsJSON() const;
    
    // Print all logs to serial
    void printAllLogs() const;
    
private:
    DataLogEntry logBuffer[LOG_BUFFER_SIZE];
    uint32_t entryCount;
    unsigned long lastLogTime;
};

#endif // DATA_LOGGER_H
