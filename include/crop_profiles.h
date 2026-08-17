#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <cstring>

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
    char name[24];
    RangeValue temperature;
    RangeValue airHumidity;
    RangeValue soilHumidity;
    RangeValue ph;
    RangeValue ec;
    RangeValue nitrogen;
    RangeValue phosphorus;
    RangeValue potassium;
    uint32_t irrigationDurationMs;
    RelayRule relayRules[MAX_RELAY_RULES];
    uint8_t relayRuleCount;
};

struct CropStorageSnapshot
{
    CropProfile profiles[MAX_CROP_PROFILES];
    size_t count;
    int8_t activeIndex;
};

class CropProfileStore
{
public:
    bool load()
    {
        Preferences prefs;
        if (!prefs.begin("crop_cfg", true))
        {
            return false;
        }

        const size_t expectedSize = sizeof(CropStorageSnapshot);
        if (prefs.getBytesLength("profiles") != expectedSize)
        {
            prefs.end();
            return false;
        }

        CropStorageSnapshot snapshot{};
        if (prefs.getBytes("profiles", &snapshot, expectedSize) != expectedSize)
        {
            prefs.end();
            return false;
        }

        prefs.end();

        if (snapshot.count > MAX_CROP_PROFILES)
        {
            return false;
        }

        count_ = snapshot.count;
        activeIndex_ = snapshot.activeIndex;

        for (size_t i = 0; i < count_; ++i)
        {
            profiles_[i] = snapshot.profiles[i];
            profiles_[i].name[sizeof(profiles_[i].name) - 1] = '\0';
        }

        if (activeIndex_ >= static_cast<int8_t>(count_))
        {
            activeIndex_ = count_ > 0 ? 0 : -1;
        }

        return true;
    }

    bool save() const
    {
        Preferences prefs;
        if (!prefs.begin("crop_cfg", false))
        {
            return false;
        }

        CropStorageSnapshot snapshot{};
        snapshot.count = count_;
        snapshot.activeIndex = activeIndex_;
        for (size_t i = 0; i < count_; ++i)
        {
            snapshot.profiles[i] = profiles_[i];
        }

        const size_t expectedSize = sizeof(CropStorageSnapshot);
        const bool ok = prefs.putBytes("profiles", &snapshot, expectedSize) == expectedSize;
        prefs.end();
        return ok;
    }

    void loadDefaults()
    {
        count_ = 0;
        activeIndex_ = -1;

        CropProfile lettuce = makeProfile(
            "lettuce",
            {18.0F, 24.0F},
            {55.0F, 80.0F},
            {45.0F, 65.0F},
            {5.8F, 6.5F},
            {1.0F, 1.8F},
            {120.0F, 180.0F},
            {40.0F, 70.0F},
            {140.0F, 220.0F},
            30000UL);

        lettuce.relayRules[0] = {IRRIGATION_RELAY_INDEX, 30000UL, 300000UL};
        lettuce.relayRuleCount = 1;

        CropProfile tomato = makeProfile(
            "tomato",
            {20.0F, 28.0F},
            {55.0F, 75.0F},
            {50.0F, 70.0F},
            {5.5F, 6.8F},
            {2.0F, 3.5F},
            {150.0F, 250.0F},
            {60.0F, 90.0F},
            {180.0F, 320.0F},
            45000UL);

        tomato.relayRules[0] = {IRRIGATION_RELAY_INDEX, 45000UL, 240000UL};
        tomato.relayRuleCount = 1;

        create(lettuce);
        create(tomato);
        setActiveByName("lettuce");
        save();
    }

    bool create(const CropProfile &profile)
    {
        if (count_ >= MAX_CROP_PROFILES || findIndex(profile.name) >= 0)
        {
            return false;
        }

        profiles_[count_++] = profile;

        if (activeIndex_ < 0)
        {
            activeIndex_ = 0;
        }

        return true;
    }

    bool update(const CropProfile &profile)
    {
        const int index = findIndex(profile.name);
        if (index < 0)
        {
            return false;
        }

        profiles_[static_cast<size_t>(index)] = profile;
        return true;
    }

    bool remove(const char *name)
    {
        const int index = findIndex(name);
        if (index < 0)
        {
            return false;
        }

        for (size_t i = static_cast<size_t>(index); i + 1 < count_; ++i)
        {
            profiles_[i] = profiles_[i + 1];
        }

        --count_;

        if (count_ == 0)
        {
            activeIndex_ = -1;
        }
        else if (activeIndex_ == static_cast<int8_t>(index))
        {
            if (activeIndex_ >= static_cast<int8_t>(count_))
            {
                activeIndex_ = static_cast<int8_t>(count_ - 1);
            }
        }
        else if (activeIndex_ > static_cast<int8_t>(index))
        {
            --activeIndex_;
        }

        return true;
    }

    bool setActiveByName(const char *name)
    {
        const int index = findIndex(name);
        if (index < 0)
        {
            return false;
        }

        activeIndex_ = static_cast<int8_t>(index);
        return true;
    }

    const CropProfile *getActive() const
    {
        if (activeIndex_ < 0 || activeIndex_ >= static_cast<int8_t>(count_))
        {
            return nullptr;
        }

        return &profiles_[static_cast<size_t>(activeIndex_)];
    }

    const CropProfile *getByIndex(size_t index) const
    {
        if (index >= count_)
        {
            return nullptr;
        }

        return &profiles_[index];
    }

    size_t count() const
    {
        return count_;
    }

private:
    CropProfile profiles_[MAX_CROP_PROFILES]{};
    size_t count_ = 0;
    int8_t activeIndex_ = -1;

    int findIndex(const char *name) const
    {
        if (name == nullptr)
        {
            return -1;
        }

        for (size_t i = 0; i < count_; ++i)
        {
            if (strncmp(name, profiles_[i].name, sizeof(profiles_[i].name)) == 0)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    static CropProfile makeProfile(
        const char *name,
        RangeValue temperature,
        RangeValue airHumidity,
        RangeValue soilHumidity,
        RangeValue ph,
        RangeValue ec,
        RangeValue nitrogen,
        RangeValue phosphorus,
        RangeValue potassium,
        uint32_t irrigationDurationMs)
    {
        CropProfile profile{};
        strncpy(profile.name, name, sizeof(profile.name) - 1);
        profile.temperature = temperature;
        profile.airHumidity = airHumidity;
        profile.soilHumidity = soilHumidity;
        profile.ph = ph;
        profile.ec = ec;
        profile.nitrogen = nitrogen;
        profile.phosphorus = phosphorus;
        profile.potassium = potassium;
        profile.irrigationDurationMs = irrigationDurationMs;
        profile.relayRuleCount = 0;
        return profile;
    }
};
