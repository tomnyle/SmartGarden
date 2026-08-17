#pragma once

#include <Arduino.h>

#include "crop_profiles.h"

class MqttHandler
{
public:
    bool handleMessage(const String &topic, const String &payload, CropProfileStore &store, String &response)
    {
        if (topic.endsWith("/crop/select"))
        {
            response = store.setActiveByName(payload.c_str()) ? "ok" : "crop_not_found";
            if (response == "ok")
            {
                store.save();
            }
            return true;
        }

        if (topic.endsWith("/crop/delete"))
        {
            response = store.remove(payload.c_str()) ? "ok" : "crop_not_found";
            if (response == "ok")
            {
                store.save();
            }
            return true;
        }

        if (topic.endsWith("/crop/create") || topic.endsWith("/crop/update"))
        {
            CropProfile profile{};
            if (!parseProfile(payload, profile))
            {
                response = "invalid_payload";
                return true;
            }

            const bool isCreate = topic.endsWith("/crop/create");
            const bool ok = isCreate ? store.create(profile) : store.update(profile);
            response = ok ? "ok" : (isCreate ? "crop_exists" : "crop_not_found");
            if (ok)
            {
                store.save();
            }
            return true;
        }

        if (topic.endsWith("/crop/list"))
        {
            response = serializeProfiles(store);
            return true;
        }

        return false;
    }

private:
    static bool parseProfile(const String &payload, CropProfile &profile)
    {
        String fields[10];
        size_t fieldCount = 0;
        int start = 0;

        while (start <= payload.length() && fieldCount < 10)
        {
            int separator = payload.indexOf(',', start);
            if (separator < 0)
            {
                separator = payload.length();
            }
            fields[fieldCount++] = payload.substring(start, separator);
            start = separator + 1;
            if (separator >= payload.length())
            {
                break;
            }
        }

        if (fieldCount != 10)
        {
            return false;
        }

        fields[0].trim();
        if (fields[0].isEmpty())
        {
            return false;
        }

        strncpy(profile.name, fields[0].c_str(), sizeof(profile.name) - 1);
        profile.temperature = parseRange(fields[1]);
        profile.airHumidity = parseRange(fields[2]);
        profile.soilHumidity = parseRange(fields[3]);
        profile.ph = parseRange(fields[4]);
        profile.ec = parseRange(fields[5]);
        profile.nitrogen = parseRange(fields[6]);
        profile.phosphorus = parseRange(fields[7]);
        profile.potassium = parseRange(fields[8]);
        profile.irrigationDurationMs = static_cast<uint32_t>(fields[9].toInt());
        profile.relayRuleCount = 1;
        profile.relayRules[0] = {IRRIGATION_RELAY_INDEX, profile.irrigationDurationMs, 300000UL};
        return true;
    }

    static RangeValue parseRange(const String &field)
    {
        const int separator = field.indexOf('|');
        if (separator < 0)
        {
            const float value = field.toFloat();
            return {value, value};
        }

        return {
            field.substring(0, separator).toFloat(),
            field.substring(separator + 1).toFloat()};
    }

    static String serializeProfiles(const CropProfileStore &store)
    {
        String output;
        for (size_t i = 0; i < store.count(); ++i)
        {
            const CropProfile *profile = store.getByIndex(i);
            if (profile == nullptr)
            {
                continue;
            }

            output += profile->name;
            if (i + 1 < store.count())
            {
                output += ",";
            }
        }

        return output;
    }
};
