#include "garden_profile.h"

// Initialize static member
CropProfile CropProfileStore::cropProfiles[13] = {};
bool CropProfileStore::initialized = false;

void CropProfileStore::initialize()
{
    if (initialized)
    {
        return; // Already initialized
    }
    
    // 1. Sâm (Ginseng)
    cropProfiles[0] = {
        .id = 1,
        .name = "Sâm",
        .temperature = {18.0f, 25.0f},
        .airHumidity = {60.0f, 75.0f},
        .soilHumidity = {50.0f, 70.0f},
        .ph = {6.0f, 7.0f},
        .ec = {800, 1200},
        .nitrogen = {100, 200},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 2. Cà chua (Tomato)
    cropProfiles[1] = {
        .id = 2,
        .name = "Cà chua",
        .temperature = {20.0f, 28.0f},
        .airHumidity = {65.0f, 85.0f},
        .soilHumidity = {60.0f, 80.0f},
        .ph = {6.0f, 6.8f},
        .ec = {1200, 1600},
        .nitrogen = {150, 250},
        .phosphorus = {100, 200},
        .potassium = {150, 300}
    };
    
    // 3. Dâu tây (Strawberry)
    cropProfiles[2] = {
        .id = 3,
        .name = "Dâu tây",
        .temperature = {18.0f, 25.0f},
        .airHumidity = {70.0f, 85.0f},
        .soilHumidity = {65.0f, 80.0f},
        .ph = {6.0f, 6.8f},
        .ec = {1000, 1400},
        .nitrogen = {120, 200},
        .phosphorus = {100, 180},
        .potassium = {150, 250}
    };
    
    // 4. Rau mầm (Microgreens)
    cropProfiles[3] = {
        .id = 4,
        .name = "Rau mầm",
        .temperature = {18.0f, 24.0f},
        .airHumidity = {65.0f, 75.0f},
        .soilHumidity = {60.0f, 75.0f},
        .ph = {6.0f, 7.0f},
        .ec = {800, 1200},
        .nitrogen = {100, 180},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 5. Cải kale (Kale)
    cropProfiles[4] = {
        .id = 5,
        .name = "Cải kale",
        .temperature = {16.0f, 24.0f},
        .airHumidity = {70.0f, 85.0f},
        .soilHumidity = {60.0f, 75.0f},
        .ph = {6.0f, 7.5f},
        .ec = {1000, 1400},
        .nitrogen = {150, 250},
        .phosphorus = {100, 180},
        .potassium = {150, 250}
    };
    
    // 6. Bánh chua (Lettuce)
    cropProfiles[5] = {
        .id = 6,
        .name = "Bánh chua",
        .temperature = {18.0f, 24.0f},
        .airHumidity = {65.0f, 75.0f},
        .soilHumidity = {60.0f, 75.0f},
        .ph = {6.0f, 7.0f},
        .ec = {1000, 1400},
        .nitrogen = {120, 200},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 7. Thơm (Basil)
    cropProfiles[6] = {
        .id = 7,
        .name = "Thơm",
        .temperature = {20.0f, 28.0f},
        .airHumidity = {60.0f, 75.0f},
        .soilHumidity = {55.0f, 70.0f},
        .ph = {6.0f, 7.5f},
        .ec = {1000, 1400},
        .nitrogen = {120, 200},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 8. Xà lách (Spinach)
    cropProfiles[7] = {
        .id = 8,
        .name = "Xà lách",
        .temperature = {16.0f, 24.0f},
        .airHumidity = {65.0f, 80.0f},
        .soilHumidity = {60.0f, 75.0f},
        .ph = {6.5f, 7.5f},
        .ec = {1000, 1400},
        .nitrogen = {150, 250},
        .phosphorus = {100, 180},
        .potassium = {150, 250}
    };
    
    // 9. Ớt (Pepper)
    cropProfiles[8] = {
        .id = 9,
        .name = "Ớt",
        .temperature = {21.0f, 29.0f},
        .airHumidity = {65.0f, 80.0f},
        .soilHumidity = {60.0f, 75.0f},
        .ph = {6.0f, 6.8f},
        .ec = {1200, 1600},
        .nitrogen = {150, 250},
        .phosphorus = {100, 200},
        .potassium = {150, 300}
    };
    
    // 10. Cúc họa mi (Chamomile)
    cropProfiles[9] = {
        .id = 10,
        .name = "Cúc họa mi",
        .temperature = {18.0f, 25.0f},
        .airHumidity = {60.0f, 75.0f},
        .soilHumidity = {50.0f, 70.0f},
        .ph = {6.0f, 7.0f},
        .ec = {800, 1200},
        .nitrogen = {100, 180},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 11. Chanh (Lemon)
    cropProfiles[10] = {
        .id = 11,
        .name = "Chanh",
        .temperature = {20.0f, 28.0f},
        .airHumidity = {70.0f, 85.0f},
        .soilHumidity = {60.0f, 75.0f},
        .ph = {6.0f, 7.0f},
        .ec = {1000, 1400},
        .nitrogen = {120, 200},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 12. Bạc hà (Mint)
    cropProfiles[11] = {
        .id = 12,
        .name = "Bạc hà",
        .temperature = {18.0f, 26.0f},
        .airHumidity = {65.0f, 75.0f},
        .soilHumidity = {55.0f, 70.0f},
        .ph = {6.0f, 7.5f},
        .ec = {1000, 1400},
        .nitrogen = {120, 200},
        .phosphorus = {80, 150},
        .potassium = {100, 180}
    };
    
    // 13. Tỏi (Garlic)
    cropProfiles[12] = {
        .id = 13,
        .name = "Tỏi",
        .temperature = {18.0f, 24.0f},
        .airHumidity = {60.0f, 75.0f},
        .soilHumidity = {50.0f, 70.0f},
        .ph = {6.0f, 8.0f},
        .ec = {1000, 1400},
        .nitrogen = {120, 200},
        .phosphorus = {80, 150},
        .potassium = {150, 250}
    };
    
    initialized = true;
    Serial.println("[CropProfileStore] All 13 crop profiles initialized");
}

const CropProfile* CropProfileStore::getCropById(uint8_t id)
{
    if (!initialized)
    {
        initialize();
    }
    
    if (id < 1 || id > 13)
    {
        Serial.printf("[CropProfileStore] ERROR: Invalid crop ID %u\n", id);
        return nullptr;
    }
    
    return &cropProfiles[id - 1];
}

const CropProfile* CropProfileStore::getCropByName(const char* name)
{
    if (!initialized)
    {
        initialize();
    }
    
    for (int i = 0; i < 13; i++)
    {
        if (strcmp(cropProfiles[i].name, name) == 0)
        {
            return &cropProfiles[i];
        }
    }
    
    Serial.printf("[CropProfileStore] ERROR: Crop '%s' not found\n", name);
    return nullptr;
}

const CropProfile* CropProfileStore::getAllCrops(uint8_t& count)
{
    if (!initialized)
    {
        initialize();
    }
    
    count = 13;
    return cropProfiles;
}

void CropProfileStore::printAllCrops()
{
    if (!initialized)
    {
        initialize();
    }
    
    Serial.println("\n===== Available Crop Profiles =====");
    for (int i = 0; i < 13; i++)
    {
        const CropProfile& profile = cropProfiles[i];
        Serial.printf("\n[%d] %s\n", profile.id, profile.name);
        Serial.printf("  Temperature: %.1f-%.1f°C\n", profile.temperature.min, profile.temperature.max);
        Serial.printf("  Air Humidity: %.1f-%.1f%%\n", profile.airHumidity.min, profile.airHumidity.max);
        Serial.printf("  Soil Humidity: %.1f-%.1f%%\n", profile.soilHumidity.min, profile.soilHumidity.max);
        Serial.printf("  pH: %.1f-%.1f\n", profile.ph.min, profile.ph.max);
        Serial.printf("  EC: %u-%u µS/cm\n", profile.ec.min, profile.ec.max);
    }
    Serial.println("\n===================================");
}
