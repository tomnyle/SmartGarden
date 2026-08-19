#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#include "crop_profiles.h"

extern PubSubClient client;
extern CropProfileStore cropStore;

#define SMARTGARDEN_TOPIC_CROP_LIST SMARTGARDEN_MQTT_ROOT_TOPIC "/crop/list"
#define SMARTGARDEN_TOPIC_CROP_CURRENT SMARTGARDEN_MQTT_ROOT_TOPIC "/crop/current"
#define SMARTGARDEN_TOPIC_CROP_CONFIG SMARTGARDEN_MQTT_ROOT_TOPIC "/crop/config"

inline void publishCropList()
{
    String payload;
    for (size_t i = 0; i < cropStore.size(); ++i)
    {
        const CropProfile *profile = cropStore.getAt(i);
        if (profile == nullptr)
        {
            continue;
        }

        if (!payload.isEmpty())
        {
            payload += ",";
        }
        payload += profile->name;
    }

    client.publish(SMARTGARDEN_TOPIC_CROP_LIST, payload.c_str(), true);
}

inline void publishCurrentCropConfig()
{
    const CropProfile *profile = cropStore.getActive();
    if (profile == nullptr)
    {
        return;
    }

    client.publish(SMARTGARDEN_TOPIC_CROP_CURRENT, profile->name, true);

    String payload = "{";
    payload += "\"name\":\"" + String(profile->name) + "\",";
    payload += "\"displayName\":\"" + String(profile->displayName) + "\",";
    payload += "\"scientificName\":\"" + String(profile->scientificName) + "\",";
    payload += "\"temperature\":{\"min\":" + String(profile->temperature.min, 1) + ",\"max\":" + String(profile->temperature.max, 1) + "},";
    payload += "\"airHumidity\":{\"min\":" + String(profile->airHumidity.min, 1) + ",\"max\":" + String(profile->airHumidity.max, 1) + "},";
    payload += "\"soilHumidity\":{\"min\":" + String(profile->soilHumidity.min, 1) + ",\"max\":" + String(profile->soilHumidity.max, 1) + "},";
    payload += "\"ph\":{\"min\":" + String(profile->ph.min, 1) + ",\"max\":" + String(profile->ph.max, 1) + "},";
    payload += "\"ec\":{\"min\":" + String(profile->ec.min, 1) + ",\"max\":" + String(profile->ec.max, 1) + "},";
    payload += "\"nitrogen\":{\"min\":" + String(profile->nitrogen.min, 0) + ",\"max\":" + String(profile->nitrogen.max, 0) + "},";
    payload += "\"phosphorus\":{\"min\":" + String(profile->phosphorus.min, 0) + ",\"max\":" + String(profile->phosphorus.max, 0) + "},";
    payload += "\"potassium\":{\"min\":" + String(profile->potassium.min, 0) + ",\"max\":" + String(profile->potassium.max, 0) + "}";
    payload += "}";

    client.publish(SMARTGARDEN_TOPIC_CROP_CONFIG, payload.c_str(), true);
}
