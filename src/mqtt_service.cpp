#include "mqtt_service.h"
#include "garden_profile.h"
#include <PubSubClient.h>
#include <WiFi.h>

// Declare as extern - defined in smartgarden.ino
extern WiFiClient espClient;
extern PubSubClient client;

MQTTService::MQTTService(const char* broker, int port)
    : mqttBroker(broker), mqttPort(port), connected(false),
      lastPublishTime(0), publishInterval(5000)
{
    strncpy(deviceId, "SmartGarden_", sizeof(deviceId) - 1);
    strncat(deviceId, String(ESP.getEfuseMac()).c_str(), sizeof(deviceId) - strlen(deviceId) - 1);
}

void MQTTService::begin(const char* username, const char* password)
{
    client.setServer(mqttBroker, mqttPort);
    client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->onMessageReceived(topic, payload, length);
    });
    
    strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
    strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);
    
    Serial.println("[MQTTService] Initialized");
}

bool MQTTService::connect()
{
    if (connected)
    {
        return true;
    }
    
    Serial.printf("[MQTTService] Connecting to %s:%d...\n", mqttBroker, mqttPort);
    
    if (!client.connected())
    {
        if (client.connect(deviceId, mqttUsername, mqttPassword))
        {
            Serial.println("[MQTTService] Connected to MQTT broker");
            connected = true;
            
            // Subscribe to control topics
            String topic = String("garden/") + String(deviceId) + "/control/#";
            client.subscribe(topic.c_str());
            
            // Publish online status
            publishStatus("online");
            
            return true;
        }
        else
        {
            Serial.printf("[MQTTService] Connection failed, code=%d\n", client.state());
            connected = false;
            return false;
        }
    }
    
    return true;
}

bool MQTTService::isConnected() const
{
    return client.connected();
}

void MQTTService::loop()
{
    if (!client.connected())
    {
        if (millis() % 5000 == 0) // Attempt reconnect every 5 seconds
        {
            connect();
        }
    }
    else
    {
        client.loop();
    }
}

bool MQTTService::publish(const char* topic, const char* payload)
{
    if (!client.connected())
    {
        return false;
    }
    
    String fullTopic = String("garden/") + String(deviceId) + "/" + String(topic);
    return client.publish(fullTopic.c_str(), payload);
}

bool MQTTService::publishSensorData(const SensorSnapshot& snapshot)
{
    if (!client.connected())
    {
        return false;
    }
    
    unsigned long now = millis();
    if (now - lastPublishTime < publishInterval)
    {
        return false; // Not time to publish yet
    }
    lastPublishTime = now;
    
    // Create JSON payload
    char payload[512];
    snprintf(payload, sizeof(payload),
            "{\"airTemp\":%.1f,\"airHumidity\":%.1f,\"soilMoisture\":%.1f,"
            "\"soilTemp\":%.1f,\"ph\":%.1f,\"ec\":%u,\"nitrogen\":%u,"
            "\"phosphorus\":%u,\"potassium\":%u}",
            snapshot.airTemp, snapshot.airHumidity, snapshot.soilMoisture,
            snapshot.soilTemp, snapshot.ph, snapshot.ec, snapshot.nitrogen,
            snapshot.phosphorus, snapshot.potassium);
    
    bool result = publish("sensors", payload);
    if (result)
    {
        Serial.println("[MQTTService] Sensor data published");
    }
    return result;
}

bool MQTTService::publishCropList()
{
    if (!client.connected())
    {
        return false;
    }
    
    // Get crop list from CropProfileStore
    char payload[1024];
    snprintf(payload, sizeof(payload),
            "{\"crops\":[{\"id\":1,\"name\":\"Sâm\"},{\"id\":2,\"name\":\"Cà chua\"},{\"id\":3,\"name\":\"Dâu tây\"}"
            ",{\"id\":4,\"name\":\"Rau mầm\"},{\"id\":5,\"name\":\"Cải kale\"},{\"id\":6,\"name\":\"Bánh chua\"}"
            ",{\"id\":7,\"name\":\"Thơm\"},{\"id\":8,\"name\":\"Xà lách\"},{\"id\":9,\"name\":\"Ớt\"}"
            ",{\"id\":10,\"name\":\"Cúc họa mi\"},{\"id\":11,\"name\":\"Chanh\"},{\"id\":12,\"name\":\"Bạc hà\"}"
            ",{\"id\":13,\"name\":\"Tỏi\"}]}");
    
    bool result = publish("crops/list", payload);
    if (result)
    {
        Serial.println("[MQTTService] Crop list published");
    }
    return result;
}

bool MQTTService::publishCropConfig(const CropProfile& profile)
{
    if (!client.connected())
    {
        return false;
    }
    
    // Create JSON with crop configuration
    char payload[512];
    snprintf(payload, sizeof(payload),
            "{\"name\":\"%s\",\"tempMin\":%.1f,\"tempMax\":%.1f,"
            "\"humidMin\":%.1f,\"humidMax\":%.1f,\"soilMin\":%.1f,\"soilMax\":%.1f}",
            profile.name, profile.temperature.min, profile.temperature.max,
            profile.airHumidity.min, profile.airHumidity.max,
            profile.soilHumidity.min, profile.soilHumidity.max);
    
    bool result = publish("crops/current", payload);
    if (result)
    {
        Serial.printf("[MQTTService] Crop config published: %s\n", profile.name);
    }
    return result;
}

bool MQTTService::publishStatus(const char* status)
{
    return publish("status", status);
}

void MQTTService::onMessageReceived(char* topic, byte* payload, unsigned int length)
{
    // Null-terminate payload
    char message[256];
    if (length >= sizeof(message))
    {
        length = sizeof(message) - 1;
    }
    strncpy(message, (char*)payload, length);
    message[length] = '\0';
    
    Serial.printf("[MQTTService] Message received on %s: %s\n", topic, message);
    
    // Parse control commands here
    // Example: garden/SmartGarden_xxx/control/fan -> ON/OFF
}
