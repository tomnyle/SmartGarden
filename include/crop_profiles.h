#pragma once

#include <Arduino.h>
#include <Preferences.h>

// ========================================
// SmartGarden - Crop Profile Definitions
// ========================================

#define IRRIGATION_RELAY_INDEX 0
#define FAN_RELAY_INDEX        1
#define LIGHT_RELAY_INDEX      2
#define HEATER_RELAY_INDEX     3

#define MAX_CROPS       16
#define MAX_RELAY_RULES  8

struct RangeValue
{
    float min;
    float max;

    bool contains(float value) const
    {
        return value >= min && value <= max;
    }
};

struct RelayRule
{
    uint8_t  relayIndex;
    uint32_t onDurationMs;
    uint32_t offDurationMs;
};

struct CropProfile
{
    char     name[32];
    RangeValue temperature;
    RangeValue airHumidity;
    RangeValue soilHumidity;
    RangeValue ph;
    RangeValue ec;
    RangeValue nitrogen;
    RangeValue phosphorus;
    RangeValue potassium;
    RelayRule  relayRules[MAX_RELAY_RULES];
    size_t     relayRuleCount;
    uint32_t   irrigationDurationMs;
};

// ========================================
// Helper to build a CropProfile
// ========================================
inline CropProfile makeProfile(
    const char *name,
    RangeValue temperature,
    RangeValue airHumidity,
    RangeValue soilHumidity,
    RangeValue ph,
    RangeValue ec,
    RangeValue nitrogen,
    RangeValue phosphorus,
    RangeValue potassium,
    uint32_t   irrigationDurationMs = 30000)
{
    CropProfile p;
    strncpy(p.name, name, sizeof(p.name) - 1);
    p.name[sizeof(p.name) - 1] = '\0';
    p.temperature         = temperature;
    p.airHumidity         = airHumidity;
    p.soilHumidity        = soilHumidity;
    p.ph                  = ph;
    p.ec                  = ec;
    p.nitrogen            = nitrogen;
    p.phosphorus          = phosphorus;
    p.potassium           = potassium;
    p.relayRuleCount      = 0;
    p.irrigationDurationMs = irrigationDurationMs;
    return p;
}

// ========================================
// CropProfileStore - manages up to MAX_CROPS
// ========================================
class CropProfileStore
{
public:
    CropProfileStore() : _count(0), _activeIndex(0) {}

    // Add a profile; returns false if store is full
    bool create(const CropProfile &profile)
    {
        if (_count >= MAX_CROPS)
            return false;
        _profiles[_count++] = profile;
        return true;
    }

    // Get active profile (nullptr if none)
    const CropProfile *getActive() const
    {
        if (_count == 0)
            return nullptr;
        return &_profiles[_activeIndex];
    }

    // Select active crop by name; returns true on success
    bool setActiveByName(const char *name)
    {
        for (size_t i = 0; i < _count; i++)
        {
            if (strcasecmp(_profiles[i].name, name) == 0)
            {
                _activeIndex = i;
                return true;
            }
        }
        return false;
    }

    size_t count() const { return _count; }

    const CropProfile *getAt(size_t index) const
    {
        if (index >= _count)
            return nullptr;
        return &_profiles[index];
    }

    // Persist active crop name in NVS
    void save()
    {
        Preferences prefs;
        if (prefs.begin("smartgarden", false))
        {
            const CropProfile *active = getActive();
            if (active)
                prefs.putString("activeCrop", active->name);
            prefs.end();
        }
    }

    // Load active crop from NVS; returns true if found
    bool load()
    {
        Preferences prefs;
        if (!prefs.begin("smartgarden", true))
            return false;

        String savedName = prefs.getString("activeCrop", "");
        prefs.end();

        if (savedName.length() == 0)
            return false;

        if (_count == 0)
            loadDefaults();

        return setActiveByName(savedName.c_str());
    }

    // Populate with the 13 default crop profiles
    void loadDefaults();

private:
    CropProfile _profiles[MAX_CROPS];
    size_t      _count;
    size_t      _activeIndex;
};
