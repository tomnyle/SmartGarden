#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>

inline String buildSmartGardenTopic(const char* deviceId, const char* suffix)
{
    return String("smartgarden/") + String(deviceId ? deviceId : "unknown") + "/" + String(suffix ? suffix : "");
}

inline bool publishRetained(PubSubClient& client, const char* deviceId, const char* suffix, const char* payload)
{
    if (!client.connected() || !suffix || !payload) {
        return false;
    }

    String topic = buildSmartGardenTopic(deviceId, suffix);
    return client.publish(topic.c_str(), payload, true);
}

#endif
