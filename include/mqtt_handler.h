#pragma once

#include <PubSubClient.h>
#include "crop_profiles.h"

// ========================================
// MQTT helper functions used by smartgarden.ino
// These rely on a globally accessible `client` (PubSubClient)
// and `cropStore` (CropProfileStore) defined in the sketch.
// ========================================

// Forward declarations of globals from smartgarden.ino
extern PubSubClient     client;
extern CropProfileStore cropStore;

// Publish comma-separated list of all crop names
inline void publishCropList()
{
    String list = "";
    for (size_t i = 0; i < cropStore.count(); i++)
    {
        if (i > 0)
            list += ",";
        const CropProfile *p = cropStore.getAt(i);
        if (p)
            list += p->name;
    }
    client.publish("smartgarden/crop/list", list.c_str(), true);
    Serial.println("Published crop list: " + list);
}

// Publish the currently active crop profile as JSON
inline void publishCurrentCropConfig()
{
    const CropProfile *p = cropStore.getActive();
    if (!p)
        return;

    // Publish current crop name
    client.publish("smartgarden/crop/current", p->name, true);

    // Build a compact JSON object with all parameters
    String json = "{";
    json += "\"name\":\"" + String(p->name) + "\",";
    json += "\"temperature\":{\"min\":" + String(p->temperature.min, 1) +
            ",\"max\":" + String(p->temperature.max, 1) + "},";
    json += "\"airHumidity\":{\"min\":" + String(p->airHumidity.min, 1) +
            ",\"max\":" + String(p->airHumidity.max, 1) + "},";
    json += "\"soilHumidity\":{\"min\":" + String(p->soilHumidity.min, 1) +
            ",\"max\":" + String(p->soilHumidity.max, 1) + "},";
    json += "\"ph\":{\"min\":" + String(p->ph.min, 1) +
            ",\"max\":" + String(p->ph.max, 1) + "},";
    json += "\"ec\":{\"min\":" + String(p->ec.min, 1) +
            ",\"max\":" + String(p->ec.max, 1) + "},";
    json += "\"nitrogen\":{\"min\":" + String(p->nitrogen.min, 0) +
            ",\"max\":" + String(p->nitrogen.max, 0) + "},";
    json += "\"phosphorus\":{\"min\":" + String(p->phosphorus.min, 0) +
            ",\"max\":" + String(p->phosphorus.max, 0) + "},";
    json += "\"potassium\":{\"min\":" + String(p->potassium.min, 0) +
            ",\"max\":" + String(p->potassium.max, 0) + "}";
    json += "}";

    client.publish("smartgarden/crop/config", json.c_str(), true);
    Serial.println("Published crop config for: " + String(p->name));
}
