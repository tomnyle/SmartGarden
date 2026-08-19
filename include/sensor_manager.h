#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <time.h>

typedef struct {
    float airTemp;        // Air temperature in Celsius
    float airHumidity;    // Air humidity in percentage
    float soilMoisture;   // Soil moisture in percentage
    float soilTemp;       // Soil temperature in Celsius
    float ph;             // Soil pH
    uint16_t ec;          // Electrical conductivity in µS/cm
    uint16_t nitrogen;    // Nitrogen level in mg/kg
    uint16_t phosphorus;  // Phosphorus level in mg/kg
    uint16_t potassium;   // Potassium level in mg/kg
    unsigned long timestamp;  // Time of reading
} SensorSnapshot;

class SensorManager {
public:
    SensorManager();
    
    void begin();
    
    // Read all sensors
    bool readSensors();
    
    // Read RS485 sensors specifically
    void readRS485Sensors();
    
    // Get current sensor snapshot
    const SensorSnapshot& getSnapshot() const;
    
    // Print snapshot to serial
    void printSnapshot() const;
    
private:
    SensorSnapshot currentSnapshot;
    unsigned long lastReadTime;
    unsigned long readInterval;
};

#endif // SENSOR_MANAGER_H
