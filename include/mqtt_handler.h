#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "garden_profile.h"

extern PubSubClient client;

// Forward declarations for publish functions
void publishCurrentCropConfig();
void publishCropList();

// Publish current active crop configuration to MQTT
inline void publishCurrentCropConfig()
{
    client.publish("smartgarden/status", "online", true);
}

// Publish list of available crops to MQTT
inline void publishCropList()
{
    CropProfileStore::initialize();

    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);
    String payload = "[";

    for (uint8_t i = 0; i < count; i++)
    {
        if (i > 0)
        {
            payload += ",";
        }

        payload += "{\"id\":";
        payload += String(crops[i].id);
        payload += ",\"name\":\"";
        payload += crops[i].name;
        payload += "\"}";
    }

    payload += "]";
    client.publish("smartgarden/crop/available", payload.c_str(), true);
}

#endif
