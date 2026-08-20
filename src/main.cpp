#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>

// Include all service headers
#include "sensor_manager.h"
#include "mqtt_service.h"
#include "network_service.h"
#include "climate_manager.h"
#include "irrigation_manager.h"
#include "zone_manager.h"
#include "relay_manager.h"
#include "data_logger.h"
#include "garden_profile.h"
#include "auto_control.h"
#include "config.h"

// ================= GLOBAL OBJECTS =================
// WiFi and MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Services
SensorManager sensorManager;
MQTTService mqttService("192.168.100.166", 1883);
NetworkService networkService("Le Danh", "123456789");
ClimateManager climateManager;
IrrigationManager irrigationManager;
ZoneManager zoneManager;
RelayManager relayManager;
DataLogger dataLogger;

// Configuration
SystemConfig systemConfig;

// ================= TIMING VARIABLES =================
unsigned long lastSensorRead = 0;
unsigned long lastControlUpdate = 0;
unsigned long lastPublish = 0;
unsigned long lastDataLog = 0;

// ================= SETUP FUNCTION =================
void setup()
{
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n========== SmartGarden System Starting ==========");
    
    // Load configuration
    Serial.println("[Setup] Loading configuration...");
    if (!loadConfig(systemConfig))
    {
        systemConfig = getDefaultConfig();
        saveConfig(systemConfig);
    }
    printConfig(systemConfig);
    
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
    climateManager.begin();
    
    // Initialize irrigation manager
    Serial.println("[Setup] Initializing irrigation manager...");
    irrigationManager.begin();
    
    // Initialize zone manager
    Serial.println("[Setup] Initializing zone manager...");
    zoneManager.begin();
    
    // Connect to WiFi
    Serial.printf("[Setup] Connecting to WiFi: %s\n", systemConfig.wifiSSID);
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
    Serial.printf("[Setup] Initializing MQTT: %s:%u\n", systemConfig.mqttBroker, systemConfig.mqttPort);
    mqttService.begin(systemConfig.mqttUsername, systemConfig.mqttPassword);
    
    // Connect to MQTT
    if (mqttService.connect())
    {
        Serial.println("[MQTT] ✓ Connected!");
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
        if (now - lastPublish > 5000) // Try reconnect every 5 seconds
        {
            mqttService.connect();
            lastPublish = now;
        }
    }
    mqttService.loop();
    client.loop(); // Ensure PubSubClient loop runs
    
    // ===== Read Sensors =====
    if (now - lastSensorRead >= systemConfig.sensorReadInterval)
    {
        lastSensorRead = now;
        
        SensorSnapshot snapshot;
        if (sensorManager.readSensors(snapshot))
        {
            Serial.println("\n========== SENSOR DATA ==========");
            Serial.printf("Air Temp     : %.1f °C\n", snapshot.airTemp);
            Serial.printf("Air Humidity : %.1f %%\n", snapshot.airHumidity);
            Serial.printf("Soil Moisture: %.1f %%\n", snapshot.soilMoisture);
            Serial.printf("Soil Temp    : %.1f °C\n", snapshot.soilTemp);
            Serial.printf("pH           : %.1f\n", snapshot.ph);
            Serial.printf("EC           : %u µS/cm\n", snapshot.ec);
            Serial.printf("Nitrogen     : %u mg/kg\n", snapshot.nitrogen);
            Serial.printf("Phosphorus   : %u mg/kg\n", snapshot.phosphorus);
            Serial.printf("Potassium    : %u mg/kg\n", snapshot.potassium);
            Serial.println("=================================\n");
            
            // Log sensor data
            dataLogger.logSensorData(snapshot);
            
            // Publish sensor data to MQTT
            if (mqttService.isConnected())
            {
                mqttService.publishSensorData(snapshot);
            }
        }
    }
    
    // ===== Auto Control =====
    if (now - lastControlUpdate >= systemConfig.climateControlInterval)
    {
        lastControlUpdate = now;
        
        // Get current sensor snapshot
        SensorSnapshot snapshot;
        if (sensorManager.readSensors(snapshot))
        {
            // Get active crop profile
            CropProfile activeProfile = CropProfileStore::getProfile(1); // Default to profile 1
            
            // Evaluate and generate control commands
            CommandQueue commands;
            AutoControlSystem::evaluateAndControl(activeProfile, snapshot, commands);
            
            // Execute relay commands
            for (uint8_t i = 0; i < commands.count; i++)
            {
                relayManager.setRelay(commands.commands[i].relayIndex, 
                                    commands.commands[i].state);
            }
        }
    }
    
    // ===== Climate Manager Loop =====
    climateManager.loop();
    
    // ===== Irrigation Manager Loop =====
    irrigationManager.loop();
    
    // ===== Zone Manager Loop =====
    zoneManager.loop();
    
    // ===== Periodic Status Publish =====
    if (now - lastPublish >= 30000) // Publish status every 30 seconds
    {
        lastPublish = now;
        
        if (mqttService.isConnected())
        {
            mqttService.publishStatus("online");
            relayManager.printStatus();
            dataLogger.printAllLogs();
        }
    }
    
    delay(10); // Small delay to prevent watchdog timeout
}
