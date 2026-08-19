#ifndef GARDEN_PROFILE_H
#define GARDEN_PROFILE_H

#include <stdint.h>
#include <string.h>

typedef struct {
    float min;
    float max;
} Range;

typedef struct {
    uint16_t min;
    uint16_t max;
} RangeUInt16;

typedef struct {
    uint8_t id;                    // Crop ID (1-13)
    const char* name;              // Crop name
    Range temperature;             // Optimal temperature range (°C)
    Range airHumidity;             // Optimal air humidity range (%)
    Range soilHumidity;            // Optimal soil humidity range (%)
    Range ph;                      // Optimal pH range
    RangeUInt16 ec;                // Optimal EC range (µS/cm)
    RangeUInt16 nitrogen;          // Optimal nitrogen range (mg/kg)
    RangeUInt16 phosphorus;        // Optimal phosphorus range (mg/kg)
    RangeUInt16 potassium;         // Optimal potassium range (mg/kg)
} CropProfile;

class CropProfileStore {
public:
    // Initialize all crop profiles
    static void initialize();
    
    // Get crop by ID (1-13)
    static const CropProfile* getCropById(uint8_t id);
    
    // Get crop by name
    static const CropProfile* getCropByName(const char* name);
    
    // Get all crops
    static const CropProfile* getAllCrops(uint8_t& count);
    
    // Print all crop profiles
    static void printAllCrops();
    
private:
    static CropProfile cropProfiles[13];
    static bool initialized;
};

#endif // GARDEN_PROFILE_H
