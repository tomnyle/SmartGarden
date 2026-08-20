#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "crop_profiles.h"

extern PubSubClient client;
extern CropProfileStore cropStore;

// Publish current active crop configuration to MQTT
void publishCurrentCropConfig()
{
    const CropProfile *activeCrop = cropStore.getActive();
    if (activeCrop == nullptr)
    {
        client.publish("smartgarden/crop/current", "None", true);
        return;
    }

    // Publish crop name
    client.publish("smartgarden/crop/current", activeCrop->name, true);

    // Publish crop parameters
    char buffer[32];
    
    snprintf(buffer, sizeof(buffer), "%.1f", activeCrop->optimalTemp);
    client.publish("smartgarden/crop/optimal_temp", buffer, true);

    snprintf(buffer, sizeof(buffer), "%.1f", activeCrop->optimalHumidity);
    client.publish("smartgarden/crop/optimal_humidity", buffer, true);

    snprintf(buffer, sizeof(buffer), "%.1f", activeCrop->optimalMoisture);
    client.publish("smartgarden/crop/optimal_moisture", buffer, true);

    snprintf(buffer, sizeof(buffer), "%.1f", activeCrop->optimalPH);
    client.publish("smartgarden/crop/optimal_ph", buffer, true);

    snprintf(buffer, sizeof(buffer), "%u", activeCrop->optimalEC);
    client.publish("smartgarden/crop/optimal_ec", buffer, true);
}

// Publish list of available crops to MQTT
void publishCropList()
{
    // Get available crops (simplified - publish count)
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Smart Garden Ready - %u crops available",
             cropStore.getCount());
    client.publish("smartgarden/status", buffer, true);
    client.publish("smartgarden/system/status", "online", true);
}

#endif
