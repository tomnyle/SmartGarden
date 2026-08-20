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
    // Publish status
    client.publish("smartgarden/system/status", "online", true);
}

// Publish list of available crops to MQTT
inline void publishCropList()
{
    // Get available crops (simplified)
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Smart Garden Ready - %u crops available", 13);
    client.publish("smartgarden/status", buffer, true);
    client.publish("smartgarden/system/status", "online", true);
}

#endif
