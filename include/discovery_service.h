#ifndef DISCOVERY_SERVICE_H
#define DISCOVERY_SERVICE_H

#include "mqtt_service.h"

class DiscoveryService {
public:
    explicit DiscoveryService(MQTTService& mqttService);

    void begin();

private:
    MQTTService& mqtt;

    void publishStatusEntity();
    void publishFirmwareEntity();
    void publishSensorEntities();
    void publishRelayEntities();
    void publishCropEntity();
};

#endif // DISCOVERY_SERVICE_H
