#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include "app_config.h"
#include "sensor_manager.h"
#include "mqtt_service.h"
#include "network_service.h"
#include "climate_manager.h"
#include "irrigation_manager.h"
#include "zone_manager.h"
#include "relay_manager.h"
#include "data_logger.h"
#include "garden_profile.h"

// ================= GLOBAL OBJECTS =================
WiFiClient espClient;
PubSubClient pubsubClient(espClient);

// DHT Sensor
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

SensorManager sensorManager;
MQTTService mqttService(MQTT_BROKER, MQTT_PORT);
NetworkService networkService(WIFI_SSID, WIFI_PASSWORD);
ClimateManager climateManager;
IrrigationManager irrigationManager;
ZoneManager zoneManager;
RelayManager relayManager;
DataLogger dataLogger;

// Current crop profile
const CropProfile* currentCrop = nullptr;
unsigned long systemStartTime = 0;

// ================= TIMING VARIABLES =================
unsigned long lastSensorRead = 0;
unsigned long lastControlUpdate = 0;
unsigned long lastPublish = 0;
unsigned long lastDataLog = 0;
unsigned long lastRelayStatusPublish = 0;
unsigned long lastDiscoveryPublish = 0;

// ================= CALLBACK FUNCTIONS =================
void relayCommandCallback(uint8_t relayIndex, bool state)
{
    Serial.printf("[App] Relay %u command: %s\n", relayIndex, state ? "ON" : "OFF");
    relayManager.setRelay(relayIndex, state);
    delay(100);
    mqttService.publishRelayStatus(relayIndex, relayManager.getRelayState(relayIndex));
}

void cropSelectCallback(const char* cropName)
{
    Serial.printf("[App] Crop selected: %s\n", cropName);
    
    const CropProfile* crop = CropProfileStore::getCropByName(cropName);
    if (crop)
    {
        currentCrop = crop;
        climateManager.setCurrentProfile(crop);
        mqttService.publishCurrentCrop(crop);
        Serial.printf("[App] ✓ Crop set to: %s\n", crop->name);
    }
    else
    {
        Serial.printf("[App] ✗ Crop not found: %s\n", cropName);
    }
}

// ================= SETUP FUNCTION =================
void setup()
{
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n========== SmartGarden System Starting ==========");
    Serial.printf("[App] Version: %s\n", APP_VERSION);
    Serial.printf("[App] Device ID: %s\n", "smartgarden");
    
    // Record system start time
    systemStartTime = millis();
    
    // Initialize DHT sensor
    Serial.println("[Setup] Initializing DHT22 sensor...");
    dht.begin();
    
    // Initialize relay manager
    Serial.println("[Setup] Initializing relay manager...");
    relayManager.begin();
    
    // Initialize sensor manager
    Serial.println("[Setup] Initializing sensor manager...");
    sensorManager.begin();
    
    // Initialize data logger
    Serial.println("[Setup] Initializing data logger...");
    dataLogger.begin();
    
    // Initialize climate manager
    Serial.println("[Setup] Initializing climate manager...");
    climateManager.begin(&relayManager);
    
    // Initialize crop profiles
    Serial.println("[Setup] Initializing crop profiles...");
    CropProfileStore::initialize();
    currentCrop = CropProfileStore::getCropById(1); // Default to Sâm (crop ID 1)
    climateManager.setCurrentProfile(currentCrop);
    
    // Initialize irrigation manager
    Serial.println("[Setup] Initializing irrigation manager...");
    irrigationManager.begin(&relayManager);
    
    // Initialize zone manager
    Serial.println("[Setup] Initializing zone manager...");
    zoneManager.begin(&relayManager);
    
    // Connect to WiFi
    Serial.printf("[Setup] Connecting to WiFi: %s\n", WIFI_SSID);
    networkService.begin();
    
    int attempts = 0;
    while (!networkService.isConnected() && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
        networkService.loop();
    }
    Serial.println();
    
    if (networkService.isConnected())
    {
        Serial.println("[WiFi] ✓ Connected!");
    }
    else
    {
        Serial.println("[WiFi] ✗ Failed to connect (will retry in loop)");
    }
    
    // Setup MQTT
    Serial.printf("[Setup] Initializing MQTT: %s:%u\n", MQTT_BROKER, MQTT_PORT);
    mqttService.setClient(&pubsubClient);
    mqttService.begin(MQTT_USERNAME, MQTT_PASSWORD);
    
    // Set callbacks for MQTT commands
    mqttService.setRelayCommandCallback(relayCommandCallback);
    mqttService.setCropSelectCallback(cropSelectCallback);
    
    // Connect to MQTT
    if (mqttService.connect())
    {
        Serial.println("[MQTT] ✓ Connected!");
        mqttService.publishCurrentCrop(currentCrop);
    }
    else
    {
        Serial.println("[MQTT] ✗ Failed to connect (will retry in loop)");
    }
    
    Serial.println("========== Setup Complete ==========\n");
    
    // Initialize timing variables
    lastSensorRead = millis();
    lastControlUpdate = millis();
    lastPublish = millis();
    lastDataLog = millis();
    lastRelayStatusPublish = millis();
    lastDiscoveryPublish = millis();
}

// ================= LOOP FUNCTION =================
void loop()
{
    unsigned long now = millis();
    
    // ===== Network Management =====
    if (!networkService.isConnected())
    {
        networkService.begin();
    }
    networkService.loop();
    
    // ===== MQTT Management =====
    if (!mqttService.isConnected())
    {
        if (now - lastPublish > MQTT_RECONNECT_INTERVAL)
        {
            mqttService.connect();
            lastPublish = now;
        }
    }
    mqttService.loop();
    
    // ===== Republish Discovery Messages Every 5 minutes =====
    if (now - lastDiscoveryPublish > 300000) // 5 minutes
    {
        lastDiscoveryPublish = now;
        if (mqttService.isConnected())
        {
            Serial.println("[App] Republishing discovery messages...");
            mqttService.publishDiscoveryMessages();
        }
    }
    
    // ===== Read Sensors =====
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL)
    {
        lastSensorRead = now;
        
        if (sensorManager.readSensors())
        {
            SensorSnapshot snapshot = sensorManager.getSnapshot();
            
            Serial.println("\n========== SENSOR DATA ==========");
            Serial.printf("Temperature : %.1f °C\n", snapshot.airTemp);
            Serial.printf("Humidity    : %.1f %%\n", snapshot.airHumidity);
            Serial.printf("Soil Moisture: %.1f %%\n", snapshot.soilMoisture);
            Serial.printf("Soil Temp   : %.1f °C\n", snapshot.soilTemp);
            Serial.printf("pH          : %.1f\n", snapshot.ph);
            Serial.printf("EC          : %u µS/cm\n", snapshot.ec);
            Serial.println("=================================");
            Serial.println();
            
            // Log sensor data
            dataLogger.logSensorData(snapshot);
            
            // Publish sensor data to MQTT
            if (mqttService.isConnected())
            {
                mqttService.publishSensorData(snapshot);
            }
            
            // Auto climate control based on crop profile
            if (currentCrop)
            {
                climateManager.control(snapshot);
            }
        }
    }
    
    // ===== Irrigation Manager Loop =====
    irrigationManager.loop();
    
    // ===== Publish Relay Status =====
    if (now - lastRelayStatusPublish >= 10000) // Every 10 seconds
    {
        lastRelayStatusPublish = now;
        
        if (mqttService.isConnected())
        {
            mqttService.publishAllRelayStatus(&relayManager);
        }
    }
    
    // ===== Periodic Status Publish =====
    if (now - lastPublish >= PUBLISH_STATUS_INTERVAL)
    {
        lastPublish = now;
        
        if (mqttService.isConnected())
        {
            mqttService.publishStatus("online");
            mqttService.publishUptime(now - systemStartTime);
            
            Serial.println("\n========== RELAY STATUS ==========");
            relayManager.printStatus();
            Serial.println("==================================");
            Serial.println();
        }
    }
    
    delay(10); // Small delay to prevent watchdog timeout
}