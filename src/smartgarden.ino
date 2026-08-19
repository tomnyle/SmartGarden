#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>

#include "config.h"
#include "crop_profiles.h"
#include "auto_control.h"
#include "mqtt_handler.h"

// ================= WIFI =================
WiFiClient espClient;
PubSubClient client(espClient);

// ================= DHT22 =================
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ================= RS485 =================
#define RXD2 16
#define TXD2 17
#define MAX485_RE_DE 4
HardwareSerial RS485Serial(2);
ModbusMaster node;

// ================= RELAY =================
#define RELAY_COUNT 8
int relayPins[RELAY_COUNT] = {5, 18, 19, 27, 32, 33, 25, 26};
bool relayState[RELAY_COUNT] = {false};

// ================= CROP STORE =================
CropProfileStore cropStore;

// ================= RS485 =================
void preTransmission()
{
    digitalWrite(MAX485_RE_DE, HIGH);
}

void postTransmission()
{
    digitalWrite(MAX485_RE_DE, LOW);
}

// ================= WIFI =================
void setup_wifi()
{
    Serial.print("Connecting WiFi");
    WiFi.begin(SMARTGARDEN_WIFI_SSID, SMARTGARDEN_WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("Failed to connect to WiFi");
    }
}

// ================= RELAY =================
void setRelay(int index, bool state)
{
    if (index < 0 || index >= RELAY_COUNT)
        return;

    relayState[index] = state;
    digitalWrite(relayPins[index], state ? LOW : HIGH);

    String stateTopic = "smartgarden/relay/" + String(index + 1) + "/state";
    client.publish(stateTopic.c_str(), state ? "ON" : "OFF", true);

    Serial.print("Relay ");
    Serial.print(index + 1);
    Serial.print(" -> ");
    Serial.println(state ? "ON" : "OFF");
}

// ================= MQTT CALLBACK =================
void callback(char *topic, byte *payload, unsigned int length)
{
    String msg = "";
    for (unsigned int i = 0; i < length; i++)
    {
        msg += (char)payload[i];
    }

    Serial.print("MQTT RX: ");
    Serial.print(topic);
    Serial.print(" = ");
    Serial.println(msg);

    // Handle crop selection
    if (String(topic) == "smartgarden/crop/set")
    {
        if (cropStore.setActiveByName(msg.c_str()))
        {
            cropStore.save();
            Serial.println("Crop changed to: " + msg);
            publishCurrentCropConfig();
        }
    }

    // Handle relay control
    for (int i = 0; i < RELAY_COUNT; i++)
    {
        String cmdTopic = "smartgarden/relay/" + String(i + 1) + "/set";
        if (String(topic) == cmdTopic)
        {
            if (msg == "ON")
            {
                setRelay(i, true);
            }
            else if (msg == "OFF")
            {
                setRelay(i, false);
            }
        }
    }
}

// ================= MQTT =================
void reconnect()
{
    while (!client.connected())
    {
        Serial.print("MQTT...");

        if (client.connect(SMARTGARDEN_DEVICE_ID,
                           SMARTGARDEN_MQTT_USERNAME,
                           SMARTGARDEN_MQTT_PASSWORD))
        {
            Serial.println("CONNECTED");

            // Subscribe to topics
            client.subscribe("smartgarden/crop/set");
            for (int i = 0; i < RELAY_COUNT; i++)
            {
                String topic = "smartgarden/relay/" + String(i + 1) + "/set";
                client.subscribe(topic.c_str());
            }

            // Publish initial states
            publishCropList();
            publishCurrentCropConfig();

            for (int i = 0; i < RELAY_COUNT; i++)
            {
                setRelay(i, relayState[i]);
            }
        }
        else
        {
            Serial.print("FAILED=");
            Serial.println(client.state());
            delay(3000);
        }
    }
}

// ================= SETUP =================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=== SmartGarden Starting ===");

    // Initialize relays
    for (int i = 0; i < RELAY_COUNT; i++)
    {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH);
        relayState[i] = false;
    }

    // Initialize DHT
    dht.begin();

    // Initialize RS485
    pinMode(MAX485_RE_DE, OUTPUT);
    digitalWrite(MAX485_RE_DE, LOW);

    RS485Serial.begin(4800, SERIAL_8N1, RXD2, TXD2);
    node.begin(1, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    // Initialize Crop Store
    if (!cropStore.load())
    {
        Serial.println("Loading default crop profiles...");
        cropStore.loadDefaults();
    }

    // Connect WiFi
    setup_wifi();

    // Setup MQTT
    client.setServer(SMARTGARDEN_MQTT_HOST, SMARTGARDEN_MQTT_PORT);
    client.setBufferSize(512);
    client.setCallback(callback);
    reconnect();

    Serial.println("=== SmartGarden Ready ===\n");
}

// ================= LOOP =================
unsigned long lastSensorRead = 0;
unsigned long lastControlUpdate = 0;

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();
    }

    if (!client.connected())
    {
        reconnect();
    }

    client.loop();

    unsigned long now = millis();

    // Read sensors and control
    if (now - lastSensorRead >= SENSOR_UPDATE_MS)
    {
        lastSensorRead = now;

        // Read DHT
        float airTemp = dht.readTemperature();
        float airHum = dht.readHumidity();

        // Read Modbus
        float moisture = 0;
        float soilTemp = 0;
        float ph = 0;
        uint16_t ec = 0;
        uint16_t n = 0;
        uint16_t p = 0;
        uint16_t k = 0;

        uint8_t result = node.readHoldingRegisters(0x0000, 40);
        if (result == node.ku8MBSuccess)
        {
            moisture = node.getResponseBuffer(0) / 10.0;
            soilTemp = node.getResponseBuffer(1) / 10.0;
            ph = node.getResponseBuffer(3) / 10.0;
            n = node.getResponseBuffer(4);
            p = node.getResponseBuffer(5);
            k = node.getResponseBuffer(6);
            ec = node.getResponseBuffer(9);
        }
        else
        {
            Serial.print("MODBUS ERROR = ");
            Serial.println(result);
        }

        // Get active crop profile
        const CropProfile *activeCrop = cropStore.getActive();
        if (activeCrop != nullptr)
        {
            // Create sensor snapshot
            SensorSnapshot snapshot;
            snapshot.airTemp = airTemp;
            snapshot.airHumidity = airHum;
            snapshot.soilMoisture = moisture;
            snapshot.soilTemp = soilTemp;
            snapshot.ph = ph;
            snapshot.ec = ec;
            snapshot.nitrogen = n;
            snapshot.phosphorus = p;
            snapshot.potassium = k;
            snapshot.timestamp = now;

            // Evaluate and control
            AutoControlSystem::CommandQueue commands;
            AutoControlSystem::evaluateAndControl(*activeCrop, snapshot, commands);

            // Apply relay commands
            for (size_t i = 0; i < commands.count; ++i)
            {
                const RelayCommand &cmd = commands.commands[i];
                setRelay(cmd.relayIndex, cmd.state);
            }
        }

        // Publish sensor data
        client.publish("smartgarden/sensors/air_temp", String(airTemp, 1).c_str(), true);
        client.publish("smartgarden/sensors/air_humidity", String(airHum, 1).c_str(), true);
        client.publish("smartgarden/sensors/soil_moisture", String(moisture, 1).c_str(), true);
        client.publish("smartgarden/sensors/soil_temp", String(soilTemp, 1).c_str(), true);
        client.publish("smartgarden/sensors/ph", String(ph, 1).c_str(), true);
        client.publish("smartgarden/sensors/ec", String(ec).c_str(), true);
        client.publish("smartgarden/sensors/nitrogen", String(n).c_str(), true);
        client.publish("smartgarden/sensors/phosphorus", String(p).c_str(), true);
        client.publish("smartgarden/sensors/potassium", String(k).c_str(), true);

        // Serial output
        Serial.println("\n===== GARDEN DATA =====");
        Serial.printf("Active Crop: %s\n", activeCrop ? activeCrop->name : "None");
        Serial.printf("Air Temp : %.1f C\n", airTemp);
        Serial.printf("Humidity : %.1f %%\n", airHum);
        Serial.printf("Moisture : %.1f %%\n", moisture);
        Serial.printf("SoilTemp : %.1f C\n", soilTemp);
        Serial.printf("PH       : %.1f\n", ph);
        Serial.printf("EC       : %u uS/cm\n", ec);
        Serial.printf("N        : %u mg/kg\n", n);
        Serial.printf("P        : %u mg/kg\n", p);
        Serial.printf("K        : %u mg/kg\n", k);
        Serial.println("=======================");
    }
}
