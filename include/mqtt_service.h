#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stdint.h>
#include <Arduino.h>
#include "sensor_manager.h"
#include "garden_profile.h"

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
    bool publishCropList();
    bool publishCropConfig(const CropProfile& profile);
    bool publishStatus(const char* status);
    
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
    
    // Callback for received messages
    void onMessageReceived(char* topic, byte* payload, unsigned int length);
    friend void mqttMessageCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_SERVICE_H
