#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stdint.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include "sensor_manager.h"
#include "garden_profile.h"
#include "relay_manager.h"

typedef void (*RelayCommandCallback)(uint8_t relayIndex, bool state);
typedef void (*CropSelectCallback)(const char* cropName);

class MQTTService {
public:
    MQTTService(const char* broker, int port);
    
    void begin(const char* username, const char* password);
    void setClient(PubSubClient* client);
    
    // Connection management
    bool connect();
    bool isConnected() const;
    void loop();  // Must be called frequently
    
    // Callback setters
    void setRelayCommandCallback(RelayCommandCallback callback);
    void setCropSelectCallback(CropSelectCallback callback);
    
    // Publishing functions
    bool publish(const char* topic, const char* payload);
    bool publishSensorData(const SensorSnapshot& snapshot);
    bool publishRelayStatus(uint8_t relayIndex, bool state);
    bool publishAllRelayStatus(const RelayManager* relayMgr);
    bool publishCropList();
    bool publishCurrentCrop(const CropProfile* profile);
    bool publishStatus(const char* status);
    bool publishUptime(unsigned long uptime);
    
    // Home Assistant MQTT Discovery
    void publishDiscoveryMessages();
    
    // Get device ID
    const char* getDeviceId() const { return deviceId; }
    
private:
    PubSubClient* client;
    const char* mqttBroker;
    int mqttPort;
    char deviceId[64];
    char mqttUsername[64];
    char mqttPassword[64];
    bool connected;
    unsigned long lastPublishTime;
    unsigned long publishInterval;
    unsigned long lastDiscoveryTime;
    unsigned long lastRetryTime;
    unsigned long retryInterval;
    
    // Callbacks
    RelayCommandCallback relayCallback;
    CropSelectCallback cropCallback;
    
    // Callback for received messages
    void onMessageReceived(char* topic, byte* payload, unsigned int length);
    friend void mqttMessageCallback(char* topic, byte* payload, unsigned int length);
    
    // Helper functions
    void subscribeToTopics();
    void handleRelayCommand(const char* relayName, const char* payload);
    void handleCropSelect(const char* payload);
};

#endif // MQTT_SERVICE_H
