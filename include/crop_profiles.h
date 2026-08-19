#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

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
    uint8_t relayIndex;
    uint32_t onDurationMs;
    uint32_t offDurationMs;
};

struct CropProfile
{
    const char *name;
    const char *displayName;
    const char *scientificName;
    RangeValue temperature;
    RangeValue airHumidity;
    RangeValue soilHumidity;
    RangeValue ph;
    RangeValue ec;
    RangeValue nitrogen;
    RangeValue phosphorus;
    RangeValue potassium;
    RelayRule relayRules[SMARTGARDEN_RELAY_COUNT];
    size_t relayRuleCount;
};

static_assert(SMARTGARDEN_RELAY_COUNT >= 3, "SmartGarden requires at least 3 relays for irrigation, fan, and light rules.");

static const uint8_t IRRIGATION_RELAY_INDEX = 0;
static const uint8_t CLIMATE_FAN_RELAY_INDEX = 1;
static const uint8_t GROW_LIGHT_RELAY_INDEX = 2;

class CropProfileStore
{
public:
    CropProfileStore() : _profileCount(0), _activeIndex(0) {}

    bool load()
    {
        loadDefaults();

        Preferences preferences;
        if (!preferences.begin("smartgarden", true))
        {
            return false;
        }

        String activeName = preferences.getString("active_crop", "");
        preferences.end();

        if (activeName.isEmpty())
        {
            return false;
        }

        return setActiveByName(activeName.c_str());
    }

    void save() const
    {
        const CropProfile *active = getActive();
        if (active == nullptr)
        {
            return;
        }

        Preferences preferences;
        if (!preferences.begin("smartgarden", false))
        {
            return;
        }

        preferences.putString("active_crop", active->name);
        preferences.end();
    }

    void loadDefaults()
    {
        _profileCount = 0;
        _activeIndex = 0;

        create(makeProfile("ginseng", "Sâm Ngọc Linh", "Panax notoginseng",
                           {15.0f, 22.0f}, {75.0f, 90.0f}, {70.0f, 80.0f}, {5.5f, 6.5f},
                           {1.2f, 2.0f}, {100.0f, 180.0f}, {40.0f, 90.0f}, {120.0f, 220.0f},
                           90UL * 1000UL, 45UL * 60UL * 1000UL));
        create(makeProfile("salvia", "Tam Thất", "Salvia miltiorrhiza",
                           {18.0f, 24.0f}, {65.0f, 85.0f}, {60.0f, 75.0f}, {6.0f, 7.0f},
                           {1.5f, 2.5f}, {120.0f, 220.0f}, {50.0f, 100.0f}, {150.0f, 260.0f},
                           90UL * 1000UL, 40UL * 60UL * 1000UL));
        create(makeProfile("morinda", "Ba Kích", "Morinda citrifolia",
                           {22.0f, 28.0f}, {65.0f, 85.0f}, {65.0f, 78.0f}, {6.0f, 7.0f},
                           {2.0f, 3.0f}, {150.0f, 260.0f}, {70.0f, 130.0f}, {180.0f, 320.0f},
                           2UL * 60UL * 1000UL, 35UL * 60UL * 1000UL));
        create(makeProfile("lettuce", "Rau Cải Xoăn", "Lactuca sativa",
                           {18.0f, 24.0f}, {60.0f, 80.0f}, {45.0f, 65.0f}, {5.8f, 6.5f},
                           {1.0f, 1.8f}, {90.0f, 170.0f}, {35.0f, 80.0f}, {100.0f, 200.0f},
                           60UL * 1000UL, 50UL * 60UL * 1000UL));
        create(makeProfile("tomato", "Cà Chua", "Solanum lycopersicum",
                           {20.0f, 28.0f}, {60.0f, 80.0f}, {50.0f, 70.0f}, {5.5f, 6.8f},
                           {2.0f, 3.5f}, {140.0f, 260.0f}, {60.0f, 120.0f}, {180.0f, 320.0f},
                           2UL * 60UL * 1000UL, 30UL * 60UL * 1000UL));
        create(makeProfile("strawberry", "Dâu Tây", "Fragaria vesca",
                           {15.0f, 25.0f}, {65.0f, 85.0f}, {60.0f, 75.0f}, {5.5f, 6.8f},
                           {1.2f, 2.0f}, {100.0f, 180.0f}, {40.0f, 90.0f}, {120.0f, 220.0f},
                           75UL * 1000UL, 45UL * 60UL * 1000UL));
        create(makeProfile("cucumber", "Dưa Chuột", "Cucumis sativus",
                           {22.0f, 30.0f}, {65.0f, 85.0f}, {50.0f, 65.0f}, {6.0f, 7.0f},
                           {2.5f, 4.0f}, {150.0f, 260.0f}, {70.0f, 130.0f}, {180.0f, 300.0f},
                           2UL * 60UL * 1000UL, 25UL * 60UL * 1000UL));
        create(makeProfile("chili", "Ớt", "Capsicum annuum",
                           {24.0f, 28.0f}, {60.0f, 80.0f}, {60.0f, 70.0f}, {6.0f, 6.8f},
                           {2.0f, 3.5f}, {140.0f, 260.0f}, {60.0f, 120.0f}, {180.0f, 320.0f},
                           90UL * 1000UL, 35UL * 60UL * 1000UL));
        create(makeProfile("carrot", "Cà Rốt", "Daucus carota",
                           {15.0f, 20.0f}, {55.0f, 75.0f}, {65.0f, 75.0f}, {6.0f, 6.8f},
                           {1.5f, 2.5f}, {110.0f, 200.0f}, {45.0f, 95.0f}, {130.0f, 240.0f},
                           60UL * 1000UL, 55UL * 60UL * 1000UL));
        create(makeProfile("onion", "Hành Tây", "Allium cepa",
                           {13.0f, 18.0f}, {55.0f, 75.0f}, {60.0f, 70.0f}, {6.0f, 7.5f},
                           {1.2f, 2.0f}, {90.0f, 170.0f}, {35.0f, 80.0f}, {110.0f, 200.0f},
                           60UL * 1000UL, 60UL * 60UL * 1000UL));
        create(makeProfile("eggplant", "Cà Tím", "Solanum melongena",
                           {20.0f, 28.0f}, {60.0f, 80.0f}, {50.0f, 70.0f}, {5.5f, 6.5f},
                           {1.8f, 3.0f}, {130.0f, 240.0f}, {55.0f, 110.0f}, {170.0f, 300.0f},
                           90UL * 1000UL, 35UL * 60UL * 1000UL));
        create(makeProfile("microgreens", "Rau Mầm", "Microgreens",
                           {15.0f, 22.0f}, {70.0f, 90.0f}, {70.0f, 85.0f}, {6.5f, 7.0f},
                           {1.0f, 2.0f}, {80.0f, 150.0f}, {30.0f, 70.0f}, {100.0f, 180.0f},
                           45UL * 1000UL, 35UL * 60UL * 1000UL));
        create(makeProfile("broccoli", "Súp Lơ", "Brassica oleracea",
                           {15.0f, 22.0f}, {60.0f, 80.0f}, {65.0f, 75.0f}, {6.0f, 7.5f},
                           {1.5f, 2.5f}, {110.0f, 210.0f}, {45.0f, 95.0f}, {140.0f, 250.0f},
                           75UL * 1000UL, 50UL * 60UL * 1000UL));
    }

    bool create(const CropProfile &profile)
    {
        if (_profileCount >= SMARTGARDEN_CROP_CAPACITY)
        {
            return false;
        }

        _profiles[_profileCount++] = profile;
        return true;
    }

    bool setActiveByName(const char *name)
    {
        if (name == nullptr)
        {
            return false;
        }

        for (size_t i = 0; i < _profileCount; ++i)
        {
            if (equalsIgnoreCase(_profiles[i].name, name) ||
                equalsIgnoreCase(_profiles[i].displayName, name))
            {
                _activeIndex = i;
                return true;
            }
        }

        return false;
    }

    const CropProfile *getActive() const
    {
        if (_profileCount == 0 || _activeIndex >= _profileCount)
        {
            return nullptr;
        }

        return &_profiles[_activeIndex];
    }

    const CropProfile *getAt(size_t index) const
    {
        if (index >= _profileCount)
        {
            return nullptr;
        }

        return &_profiles[index];
    }

    size_t size() const
    {
        return _profileCount;
    }

private:
    CropProfile _profiles[SMARTGARDEN_CROP_CAPACITY];
    size_t _profileCount;
    size_t _activeIndex;

    static bool equalsIgnoreCase(const char *left, const char *right)
    {
        if (left == nullptr || right == nullptr)
        {
            return false;
        }

        return strcasecmp(left, right) == 0;
    }

    static CropProfile makeProfile(const char *name,
                                   const char *displayName,
                                   const char *scientificName,
                                   RangeValue temperature,
                                   RangeValue airHumidity,
                                   RangeValue soilHumidity,
                                   RangeValue ph,
                                   RangeValue ec,
                                   RangeValue nitrogen,
                                   RangeValue phosphorus,
                                   RangeValue potassium,
                                   uint32_t irrigationOnDurationMs,
                                   uint32_t irrigationOffDurationMs)
    {
        CropProfile profile = {};
        profile.name = name;
        profile.displayName = displayName;
        profile.scientificName = scientificName;
        profile.temperature = temperature;
        profile.airHumidity = airHumidity;
        profile.soilHumidity = soilHumidity;
        profile.ph = ph;
        profile.ec = ec;
        profile.nitrogen = nitrogen;
        profile.phosphorus = phosphorus;
        profile.potassium = potassium;
        profile.relayRules[0] = {IRRIGATION_RELAY_INDEX, irrigationOnDurationMs, irrigationOffDurationMs};
        profile.relayRules[1] = {CLIMATE_FAN_RELAY_INDEX, 10UL * 60UL * 1000UL, 20UL * 60UL * 1000UL};
        profile.relayRules[2] = {GROW_LIGHT_RELAY_INDEX, 12UL * 60UL * 60UL * 1000UL, 12UL * 60UL * 60UL * 1000UL};
        profile.relayRuleCount = 3;
        return profile;
    }
};
