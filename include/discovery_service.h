#ifndef DISCOVERY_SERVICE_H
#define DISCOVERY_SERVICE_H

#include <Arduino.h>
#include <PubSubClient.h>

class DiscoveryService {
public:
    DiscoveryService(PubSubClient& client,
                     const char* deviceId,
                     const char* deviceName,
                     const char* manufacturer,
                     const char* model,
                     const char* availabilityTopic);

    bool begin(size_t relayCount);

private:
    PubSubClient& client;
    const char* deviceId;
    const char* deviceName;
    const char* manufacturer;
    const char* model;
    const char* availabilityTopic;

    bool publishConfig(const char* component, const char* objectId, const String& payload);
    String buildDeviceJson() const;
    String buildAvailabilityJson() const;
    String escapeJson(const char* value) const;
    String buildSensorPayload(const char* name,
                              const char* objectId,
                              const char* uniqueId,
                              const char* stateTopic,
                              const char* unitOfMeasurement,
                              const char* deviceClass,
                              const char* stateClass) const;
    String buildSwitchPayload(const char* name,
                              const char* objectId,
                              const char* uniqueId,
                              const char* stateTopic,
                              const char* commandTopic) const;
};

#endif
