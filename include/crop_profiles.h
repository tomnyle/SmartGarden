#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <cstring>
#include <strings.h>

static constexpr uint8_t MAX_CROP_PROFILES = 13;
static constexpr uint8_t MAX_RELAY_RULES = 8;
static constexpr uint8_t IRRIGATION_RELAY_INDEX = 0;
static constexpr uint8_t FAN_RELAY_INDEX = 1;
static constexpr uint8_t HEATER_RELAY_INDEX = 2;
static constexpr uint8_t COOLER_RELAY_INDEX = 3;

struct RangeValue
{
    float min;
    float max;

    bool contains(float value) const
    {
        return !isnan(value) && value >= min && value <= max;
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
    char key[24];
    char name[32];
    RangeValue temperature;
    RangeValue airHumidity;
    RangeValue soilHumidity;
    RangeValue ph;
    RangeValue ec;
    RangeValue nitrogen;
    RangeValue phosphorus;
    RangeValue potassium;
    RelayRule relayRules[MAX_RELAY_RULES];
    size_t relayRuleCount;
};

class CropProfileStore
{
public:
    CropProfileStore()
    {
        loadDefaults();
    }

    void loadDefaults()
    {
        profileCount = 0;

        addProfile("ginseng", "Sam Ngoc Linh", {15.0f, 22.0f}, {70.0f, 85.0f}, {70.0f, 80.0f}, {5.5f, 6.5f}, {1.2f, 2.0f}, {80.0f, 180.0f}, {30.0f, 90.0f}, {120.0f, 220.0f}, 15000UL, 900000UL);
        addProfile("salvia", "Tam That", {18.0f, 24.0f}, {65.0f, 80.0f}, {60.0f, 75.0f}, {6.0f, 7.0f}, {1.5f, 2.5f}, {70.0f, 160.0f}, {25.0f, 80.0f}, {110.0f, 210.0f}, 15000UL, 780000UL);
        addProfile("morinda", "Ba Kich", {22.0f, 28.0f}, {65.0f, 80.0f}, {65.0f, 78.0f}, {6.0f, 7.0f}, {2.0f, 3.0f}, {80.0f, 180.0f}, {30.0f, 90.0f}, {120.0f, 240.0f}, 20000UL, 720000UL);
        addProfile("lettuce", "Rau Cai Xoan", {18.0f, 24.0f}, {60.0f, 75.0f}, {45.0f, 65.0f}, {5.8f, 6.5f}, {1.0f, 1.8f}, {60.0f, 140.0f}, {20.0f, 70.0f}, {100.0f, 180.0f}, 12000UL, 660000UL);
        addProfile("microgreens", "Rau Mam", {15.0f, 22.0f}, {70.0f, 85.0f}, {70.0f, 85.0f}, {6.5f, 7.0f}, {1.0f, 2.0f}, {50.0f, 110.0f}, {20.0f, 60.0f}, {80.0f, 160.0f}, 10000UL, 600000UL);
        addProfile("tomato", "Ca Chua", {20.0f, 28.0f}, {60.0f, 80.0f}, {50.0f, 70.0f}, {5.5f, 6.8f}, {2.0f, 3.5f}, {100.0f, 220.0f}, {30.0f, 90.0f}, {180.0f, 320.0f}, 20000UL, 660000UL);
        addProfile("strawberry", "Dau Tay", {15.0f, 25.0f}, {65.0f, 80.0f}, {60.0f, 75.0f}, {5.5f, 6.8f}, {1.2f, 2.0f}, {80.0f, 160.0f}, {25.0f, 70.0f}, {140.0f, 240.0f}, 15000UL, 720000UL);
        addProfile("cucumber", "Dua Chuot", {22.0f, 30.0f}, {60.0f, 80.0f}, {50.0f, 65.0f}, {6.0f, 7.0f}, {2.5f, 4.0f}, {90.0f, 180.0f}, {25.0f, 70.0f}, {160.0f, 280.0f}, 18000UL, 600000UL);
        addProfile("chili", "Ot", {24.0f, 28.0f}, {60.0f, 75.0f}, {60.0f, 70.0f}, {6.0f, 6.8f}, {2.0f, 3.5f}, {90.0f, 180.0f}, {25.0f, 70.0f}, {150.0f, 260.0f}, 16000UL, 660000UL);
        addProfile("eggplant", "Ca Tim", {20.0f, 28.0f}, {60.0f, 80.0f}, {50.0f, 70.0f}, {5.5f, 6.5f}, {1.8f, 3.0f}, {90.0f, 180.0f}, {25.0f, 70.0f}, {150.0f, 260.0f}, 18000UL, 660000UL);
        addProfile("carrot", "Ca Rot", {15.0f, 20.0f}, {55.0f, 75.0f}, {65.0f, 75.0f}, {6.0f, 6.8f}, {1.5f, 2.5f}, {60.0f, 120.0f}, {20.0f, 60.0f}, {120.0f, 220.0f}, 12000UL, 900000UL);
        addProfile("onion", "Hanh Tay", {13.0f, 18.0f}, {55.0f, 75.0f}, {60.0f, 70.0f}, {6.0f, 7.5f}, {1.2f, 2.0f}, {50.0f, 110.0f}, {20.0f, 60.0f}, {100.0f, 180.0f}, 12000UL, 960000UL);
        addProfile("broccoli", "Sup Lo", {15.0f, 22.0f}, {60.0f, 80.0f}, {65.0f, 75.0f}, {6.0f, 7.5f}, {1.5f, 2.5f}, {70.0f, 140.0f}, {25.0f, 70.0f}, {130.0f, 220.0f}, 14000UL, 840000UL);

        activeIndex = profileCount > 0 ? 0 : -1;
    }

    bool load()
    {
        preferences.begin("smartgarden", true);
        String storedKey = preferences.getString("activeCrop", "");
        preferences.end();

        if (storedKey.isEmpty())
        {
            return false;
        }

        return setActiveByName(storedKey.c_str());
    }

    void save()
    {
        const CropProfile *active = getActive();
        if (active == nullptr)
        {
            return;
        }

        preferences.begin("smartgarden", false);
        preferences.putString("activeCrop", active->key);
        preferences.end();
    }

    bool setActiveByName(const char *name)
    {
        if (name == nullptr)
        {
            return false;
        }

        for (size_t i = 0; i < profileCount; ++i)
        {
            if (matches(profiles[i].key, name) || matches(profiles[i].name, name))
            {
                activeIndex = static_cast<int>(i);
                return true;
            }
        }

        return false;
    }

    CropProfile *getActive()
    {
        if (activeIndex < 0 || static_cast<size_t>(activeIndex) >= profileCount)
        {
            return nullptr;
        }

        return &profiles[activeIndex];
    }

    const CropProfile *getActive() const
    {
        if (activeIndex < 0 || static_cast<size_t>(activeIndex) >= profileCount)
        {
            return nullptr;
        }

        return &profiles[activeIndex];
    }

    const CropProfile &getProfile(size_t index) const
    {
        return profiles[index];
    }

    size_t getCount() const
    {
        return profileCount;
    }

private:
    CropProfile profiles[MAX_CROP_PROFILES] = {};
    size_t profileCount = 0;
    int activeIndex = -1;
    Preferences preferences;

    static bool matches(const char *left, const char *right)
    {
        return strcasecmp(left, right) == 0;
    }

    static void copyText(char *destination, size_t size, const char *value)
    {
        if (size == 0)
        {
            return;
        }

        strncpy(destination, value, size - 1);
        destination[size - 1] = '\0';
    }

    static CropProfile makeProfile(const char *key,
                                   const char *name,
                                   RangeValue temperature,
                                   RangeValue airHumidity,
                                   RangeValue soilHumidity,
                                   RangeValue ph,
                                   RangeValue ec,
                                   RangeValue nitrogen,
                                   RangeValue phosphorus,
                                   RangeValue potassium,
                                   uint32_t irrigationOnMs,
                                   uint32_t irrigationOffMs)
    {
        CropProfile profile = {};
        copyText(profile.key, sizeof(profile.key), key);
        copyText(profile.name, sizeof(profile.name), name);
        profile.temperature = temperature;
        profile.airHumidity = airHumidity;
        profile.soilHumidity = soilHumidity;
        profile.ph = ph;
        profile.ec = ec;
        profile.nitrogen = nitrogen;
        profile.phosphorus = phosphorus;
        profile.potassium = potassium;
        profile.relayRules[0] = {IRRIGATION_RELAY_INDEX, irrigationOnMs, irrigationOffMs};
        profile.relayRuleCount = 1;
        return profile;
    }

    void addProfile(const char *key,
                    const char *name,
                    RangeValue temperature,
                    RangeValue airHumidity,
                    RangeValue soilHumidity,
                    RangeValue ph,
                    RangeValue ec,
                    RangeValue nitrogen,
                    RangeValue phosphorus,
                    RangeValue potassium,
                    uint32_t irrigationOnMs,
                    uint32_t irrigationOffMs)
    {
        if (profileCount >= MAX_CROP_PROFILES)
        {
            return;
        }

        profiles[profileCount++] = makeProfile(key,
                                               name,
                                               temperature,
                                               airHumidity,
                                               soilHumidity,
                                               ph,
                                               ec,
                                               nitrogen,
                                               phosphorus,
                                               potassium,
                                               irrigationOnMs,
                                               irrigationOffMs);
    }
};
