#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ModbusMaster.h>

// ================= FORWARD DECLARATIONS =================
void publishRelayDiscovery();
void publishSensorDiscovery();

// ================= WIFI =================
const char* ssid = "Le Danh";
const char* password = "123456789";

// ================= MQTT =================
const char* mqtt_server = "192.168.100.166";
const int mqtt_port = 1883;
const char* mqtt_user = "homer";
const char* mqtt_password = "Danh@@@1992";

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
bool relayState[RELAY_COUNT];

// ================= RS485 CONTROL =================
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
  Serial.println();
  Serial.print("[WiFi] Connecting to SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  
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
    Serial.println("[WiFi] ✓ Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("[WiFi] Signal: %d dBm\n", WiFi.RSSI());
  }
  else
  {
    Serial.println("[WiFi] ✗ Connection Failed!");
  }
}

// ================= RELAY CONTROL =================
void setRelay(int index, bool state)
{
  if (index < 0 || index >= RELAY_COUNT) return;
  
  relayState[index] = state;
  digitalWrite(relayPins[index], state ? LOW : HIGH);  // Relay active LOW

  String stateTopic = "smartfarm/garden/relay/" + String(index + 1) + "/state";
  client.publish(stateTopic.c_str(), state ? "ON" : "OFF", true);

  Serial.printf("[Relay] Relay %d -> %s\n", index + 1, state ? "ON" : "OFF");
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length)
{
  String msg = "";
  for (unsigned int i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }

  Serial.print("[MQTT RX] ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(msg);

  for (int i = 0; i < RELAY_COUNT; i++)
  {
    String cmdTopic = "smartfarm/garden/relay/" + String(i + 1) + "/set";
    if (String(topic) == cmdTopic)
    {
      if (msg == "ON")
        setRelay(i, true);
      else if (msg == "OFF")
        setRelay(i, false);
    }
  }
}

// ================= MQTT RECONNECT =================
void reconnect()
{
  int attempts = 0;
  while (!client.connected() && attempts < 5)
  {
    Serial.print("[MQTT] Connecting to broker... ");

    if (client.connect("ESP32_SmartGarden", mqtt_user, mqtt_password))
    {
      Serial.println("✓ Connected!");

      // Subscribe to relay commands
      for (int i = 0; i < RELAY_COUNT; i++)
      {
        String topic = "smartfarm/garden/relay/" + String(i + 1) + "/set";
        client.subscribe(topic.c_str());
      }

      // Publish relay discovery and states
      publishRelayDiscovery();
      publishSensorDiscovery();
      
      return;
    }
    else
    {
      Serial.print("✗ Failed (code=");
      Serial.print(client.state());
      Serial.println(")");
      attempts++;
      delay(3000);
    }
  }
}

// ================= RELAY DISCOVERY =================
void publishRelayDiscovery()
{
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    String topic = "homeassistant/switch/garden_relay" + String(i + 1) + "/config";
    String payload = 
      "{" 
        "\"name\":\"Relay " + String(i + 1) + "\","
        "\"uniq_id\":\"relay_" + String(i + 1) + "\","
        "\"cmd_t\":\"smartfarm/garden/relay/" + String(i + 1) + "/set\","
        "\"stat_t\":\"smartfarm/garden/relay/" + String(i + 1) + "/state\","
        "\"pl_on\":\"ON\","
        "\"pl_off\":\"OFF\","
        "\"ret\":true"
      "}";

    client.publish(topic.c_str(), payload.c_str(), true);
    delay(100);
  }
  Serial.println("[MQTT] Relay discovery published");
}

// ================= SENSOR DISCOVERY =================
void publishSensorDiscovery()
{
  struct Sensor {
    const char* name;
    const char* id;
    const char* topic;
    const char* unit;
  };

  Sensor sensors[] = {
    {"Air Temperature", "air_temp", "smartfarm/garden/air_temp", "°C"},
    {"Air Humidity", "air_humidity", "smartfarm/garden/air_humidity", "%"},
    {"Soil Moisture", "moisture", "smartfarm/garden/moisture", "%"},
    {"Soil Temperature", "soil_temp", "smartfarm/garden/soil_temp", "°C"},
    {"pH", "ph", "smartfarm/garden/ph", "pH"},
    {"EC", "ec", "smartfarm/garden/conductivity", "µS/cm"},
    {"Nitrogen", "n", "smartfarm/garden/n", "mg/kg"},
    {"Phosphorus", "p", "smartfarm/garden/p", "mg/kg"},
    {"Potassium", "k", "smartfarm/garden/k", "mg/kg"},
    {"Salinity", "salinity", "smartfarm/garden/salinity", "ppt"},
    {"TDS", "tds", "smartfarm/garden/tds", "ppm"}
  };

  for (auto sensor : sensors)
  {
    String configTopic = "homeassistant/sensor/" + String(sensor.id) + "/config";
    String payload = 
      "{" 
        "\"name\":\"" + String(sensor.name) + "\","
        "\"uniq_id\":\"" + String(sensor.id) + "\","
        "\"stat_t\":\"" + String(sensor.topic) + "\","
        "\"unit_of_meas\":\"" + String(sensor.unit) + "\""
      "}";
    
    client.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }
  Serial.println("[MQTT] Sensor discovery published");
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("   SMART GARDEN v1.0 - Home Assistant");
  Serial.println("======================================");
  Serial.println();

  // Initialize Relays
  Serial.println("[Setup] Initializing relays...");
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
    relayState[i] = false;
  }
  Serial.println("[Setup] ✓ Relays initialized");

  // Initialize DHT22
  Serial.println("[Setup] Initializing DHT22...");
  dht.begin();
  Serial.println("[Setup] ✓ DHT22 initialized");

  // Initialize RS485
  Serial.println("[Setup] Initializing RS485 Modbus...");
  pinMode(MAX485_RE_DE, OUTPUT);
  digitalWrite(MAX485_RE_DE, LOW);
  RS485Serial.begin(4800, SERIAL_8N1, RXD2, TXD2);
  node.begin(1, RS485Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  Serial.println("[Setup] ✓ RS485 initialized");

  // Connect WiFi
  setup_wifi();

  // Setup MQTT
  Serial.println("[Setup] Initializing MQTT...");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  Serial.println();
  Serial.println("======================================");
  Serial.println("   SETUP COMPLETE - Starting loop");
  Serial.println("======================================");
  Serial.println();
}

// ================= LOOP =================
unsigned long lastSend = 0;
unsigned long lastStatus = 0;

void loop()
{
  unsigned long now = millis();

  // WiFi reconnect check
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] Reconnecting...");
    setup_wifi();
  }

  // MQTT reconnect check
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();

  // Read sensors and publish every 10 seconds
  if (now - lastSend >= 10000)
  {
    lastSend = now;

    // Read DHT22
    float airTemp = dht.readTemperature();
    float airHum = dht.readHumidity();

    // Initialize Modbus sensor values
    float moisture = 0;
    float soilTemp = 0;
    float ph = 0;
    uint16_t ec = 0;
    uint16_t n = 0;
    uint16_t p = 0;
    uint16_t k = 0;
    uint16_t salinity = 0;
    uint16_t tds = 0;

    // Read Modbus registers
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
      salinity = node.getResponseBuffer(35);
      tds = node.getResponseBuffer(36);
    }
    else
    {
      Serial.printf("[Modbus] Error code: %d\n", result);
    }

    // Publish to MQTT
    if (client.connected())
    {
      client.publish("smartfarm/garden/air_temp", String(airTemp, 1).c_str(), true);
      client.publish("smartfarm/garden/air_humidity", String(airHum, 1).c_str(), true);
      client.publish("smartfarm/garden/moisture", String(moisture, 1).c_str(), true);
      client.publish("smartfarm/garden/soil_temp", String(soilTemp, 1).c_str(), true);
      client.publish("smartfarm/garden/ph", String(ph, 1).c_str(), true);
      client.publish("smartfarm/garden/conductivity", String(ec).c_str(), true);
      client.publish("smartfarm/garden/n", String(n).c_str(), true);
      client.publish("smartfarm/garden/p", String(p).c_str(), true);
      client.publish("smartfarm/garden/k", String(k).c_str(), true);
      client.publish("smartfarm/garden/salinity", String(salinity / 10.0, 1).c_str(), true);
      client.publish("smartfarm/garden/tds", String(tds).c_str(), true);
    }

    // Print to serial monitor
    Serial.println();
    Serial.println("========== SENSOR DATA ==========");
    Serial.printf("Air Temp  : %.1f °C\n", airTemp);
    Serial.printf("Air Humidity : %.1f %%\n", airHum);
    Serial.printf("Soil Moisture: %.1f %%\n", moisture);
    Serial.printf("Soil Temp : %.1f °C\n", soilTemp);
    Serial.printf("pH        : %.1f\n", ph);
    Serial.printf("EC        : %u µS/cm\n", ec);
    Serial.printf("N         : %u mg/kg\n", n);
    Serial.printf("P         : %u mg/kg\n", p);
    Serial.printf("K         : %u mg/kg\n", k);
    Serial.printf("Salinity  : %.1f ppt\n", salinity / 10.0);
    Serial.printf("TDS       : %u ppm\n", tds);
    Serial.println("================================");
  }

  // Print system status every 30 seconds
  if (now - lastStatus >= 30000)
  {
    lastStatus = now;

    Serial.println();
    Serial.println("========== SYSTEM STATUS ==========");
    Serial.printf("WiFi  : %s", WiFi.status() == WL_CONNECTED ? "✓ Connected" : "✗ Disconnected");
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf(" (%d dBm)", WiFi.RSSI());
      Serial.printf(" - IP: %s", WiFi.localIP().toString().c_str());
    }
    Serial.println();

    Serial.printf("MQTT  : %s\n", client.connected() ? "✓ Connected" : "✗ Disconnected");

    Serial.print("Relays: ");
    for (int i = 0; i < RELAY_COUNT; i++)
    {
      Serial.print(relayState[i] ? "[ON] " : "[OFF] ");
    }
    Serial.println();
    Serial.println("==================================");
  }

  delay(10);
}
