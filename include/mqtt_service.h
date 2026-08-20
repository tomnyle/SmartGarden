#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stdint.h>
#include <Arduino.h>
#include "sensor_manager.h"
#include "garden_profile.h"

typedef void (*RelayCommandCallback)(const char* relayName, bool state);
typedef void (*CropSelectCallback)(const char* cropName);

class MQTTService {
public:
    MQTTService(const char* broker, int port);
    
    void begin(const char* username, const char* password);
    
    // Connection management
    bool connect();
    bool isConnected() const;
    void loop();  // Must be called frequently
    
    // Publishing functions
    bool publish(const char* topic, const char* payload);
    bool publishSensorData(const SensorSnapshot& snapshot);
    bool publishRelayState(const char* relayName, bool state);
    bool publishCropList();
    bool publishCurrentCrop(const char* cropName);
    bool publishStatus(bool online);
    bool publishUptime(uint32_t uptimeSeconds);
    bool publishDiscovery();

    void setRelayCommandCallback(RelayCommandCallback callback);
    void setCropSelectCallback(CropSelectCallback callback);
    
    // Get device ID
    const char* getDeviceId() const { return deviceId; }
    
private:
    const char* mqttBroker;
    int mqttPort;
    char deviceId[64];
    char mqttUsername[64];
    char mqttPassword[64];
    bool connected;
    unsigned long lastPublishTime;
    unsigned long publishInterval;
    RelayCommandCallback relayCallback;
    CropSelectCallback cropCallback;

    bool publishDiscoverySensor(const char* objectId, const char* name,
                                const char* stateTopic, const char* unit,
                                const char* deviceClass);
    bool publishDiscoverySwitch(const char* relayName, const char* displayName);
    bool publishDiscoverySelect();
    
    // Callback for received messages
    void onMessageReceived(char* topic, byte* payload, unsigned int length);
    friend void mqttMessageCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_SERVICE_H
