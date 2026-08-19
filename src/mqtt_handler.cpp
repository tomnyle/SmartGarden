#include "mqtt_handler.h"

#include "config.h"
#include "relay_manager.h"
#include "sensor_manager.h"

MqttHandler *MqttHandler::activeInstance = nullptr;

MqttHandler::MqttHandler()
    : mqttClient(wifiClient)
{
}

void MqttHandler::begin(RelayManager &relayManagerRef,
                        ClimateManager &climateManagerRef,
                        CropProfileStore &cropStoreRef,
                        SensorManager &sensorManagerRef)
{
    relayManager = &relayManagerRef;
    climateManager = &climateManagerRef;
    cropStore = &cropStoreRef;
    sensorManager = &sensorManagerRef;
    activeInstance = this;

    mqttClient.setServer(SMARTGARDEN_MQTT_HOST, SMARTGARDEN_MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);

    connectWiFi();
    ensureConnected();
}

void MqttHandler::loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        connectWiFi();
    }

    ensureConnected();

    if (mqttClient.connected())
    {
        mqttClient.loop();
    }
}

bool MqttHandler::isConnected() const
{
    return mqttClient.connected();
}

void MqttHandler::publishSensorData(const SensorSnapshot &snapshot)
{
    if (!mqttClient.connected())
    {
        return;
    }

    publishTopic("sensors/air_temp", String(snapshot.airTemp, 1));
    publishTopic("sensors/air_humidity", String(snapshot.airHumidity, 1));
    publishTopic("sensors/soil_moisture", String(snapshot.soilMoisture, 1));
    publishTopic("sensors/soil_temp", String(snapshot.soilTemp, 1));
    publishTopic("sensors/ph", String(snapshot.ph, 1));
    publishTopic("sensors/ec", String(snapshot.ec));
    publishTopic("sensors/nitrogen", String(snapshot.nitrogen));
    publishTopic("sensors/phosphorus", String(snapshot.phosphorus));
    publishTopic("sensors/potassium", String(snapshot.potassium));
}

void MqttHandler::publishCropList()
{
    if (!mqttClient.connected() || cropStore == nullptr)
    {
        return;
    }

    String payload;
    for (size_t i = 0; i < cropStore->getCount(); ++i)
    {
        if (i > 0)
        {
            payload += ",";
        }
        payload += cropStore->getProfile(i).key;
    }

    publishTopic("crop/list", payload);
}

void MqttHandler::publishCurrentCropConfig()
{
    if (!mqttClient.connected() || cropStore == nullptr)
    {
        return;
    }

    const CropProfile *active = cropStore->getActive();
    if (active == nullptr)
    {
        return;
    }

    publishTopic("crop/current", String(active->key));

    String json = "{";
    json += "\"key\":\"" + String(active->key) + "\",";
    json += "\"name\":\"" + String(active->name) + "\",";
    json += "\"temperature\":{\"min\":" + String(active->temperature.min, 1) + ",\"max\":" + String(active->temperature.max, 1) + "},";
    json += "\"airHumidity\":{\"min\":" + String(active->airHumidity.min, 1) + ",\"max\":" + String(active->airHumidity.max, 1) + "},";
    json += "\"soilHumidity\":{\"min\":" + String(active->soilHumidity.min, 1) + ",\"max\":" + String(active->soilHumidity.max, 1) + "},";
    json += "\"ph\":{\"min\":" + String(active->ph.min, 1) + ",\"max\":" + String(active->ph.max, 1) + "},";
    json += "\"ec\":{\"min\":" + String(active->ec.min, 1) + ",\"max\":" + String(active->ec.max, 1) + "}";
    json += "}";

    publishTopic("crop/config", json);
}

void MqttHandler::publishRelayStates()
{
    if (!mqttClient.connected() || relayManager == nullptr)
    {
        return;
    }

    for (int i = 0; i < relayManager->getRelayCount(); ++i)
    {
        publishTopic("relay/" + String(i + 1) + "/state", relayManager->getRelayState(i) ? "ON" : "OFF");
    }

    if (climateManager != nullptr)
    {
        publishTopic("autocontrol/state", climateManager->isAutoControlEnabled() ? "ON" : "OFF");
    }
}

void MqttHandler::connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    Serial.print("Connecting WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(SMARTGARDEN_WIFI_SSID, SMARTGARDEN_WIFI_PASSWORD);

    for (int attempt = 0; attempt < 20 && WiFi.status() != WL_CONNECTED; ++attempt)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi connection not available");
    }
}

void MqttHandler::ensureConnected()
{
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected())
    {
        return;
    }

    const unsigned long now = millis();
    if (now - lastReconnectAttempt < 5000UL)
    {
        return;
    }

    lastReconnectAttempt = now;

    Serial.print("Connecting MQTT");
    if (mqttClient.connect(SMARTGARDEN_DEVICE_ID,
                           SMARTGARDEN_MQTT_USERNAME,
                           SMARTGARDEN_MQTT_PASSWORD))
    {
        Serial.println("...connected");
        subscribeTopics();
        publishCropList();
        publishCurrentCropConfig();
        publishRelayStates();
    }
    else
    {
        Serial.print("...failed with state ");
        Serial.println(mqttClient.state());
    }
}

void MqttHandler::subscribeTopics()
{
    publishTopic("device/name", SMARTGARDEN_DEVICE_NAME);
    mqttClient.subscribe((String(SMARTGARDEN_MQTT_ROOT_TOPIC) + "/crop/set").c_str());
    mqttClient.subscribe((String(SMARTGARDEN_MQTT_ROOT_TOPIC) + "/autocontrol/set").c_str());

    if (relayManager == nullptr)
    {
        return;
    }

    for (int i = 0; i < relayManager->getRelayCount(); ++i)
    {
        mqttClient.subscribe((String(SMARTGARDEN_MQTT_ROOT_TOPIC) + "/relay/" + String(i + 1) + "/set").c_str());
    }
}

void MqttHandler::onMessage(const String &topic, const String &payload)
{
    const String rootTopic = String(SMARTGARDEN_MQTT_ROOT_TOPIC);

    if (topic == rootTopic + "/crop/set" && cropStore != nullptr)
    {
        if (cropStore->setActiveByName(payload.c_str()))
        {
            cropStore->save();
            publishCurrentCropConfig();
        }
        return;
    }

    if (topic == rootTopic + "/autocontrol/set" && climateManager != nullptr)
    {
        const bool enabled = payload.equalsIgnoreCase("ON") || payload == "1" || payload.equalsIgnoreCase("TRUE");
        climateManager->setAutoControlEnabled(enabled);
        publishRelayStates();
        return;
    }

    const String relayPrefix = rootTopic + "/relay/";
    if (topic.startsWith(relayPrefix) && topic.endsWith("/set") && relayManager != nullptr)
    {
        const int relayNumber = topic.substring(relayPrefix.length(), topic.length() - 4).toInt();
        if (relayNumber >= 1 && relayNumber <= relayManager->getRelayCount())
        {
            const bool state = payload.equalsIgnoreCase("ON") || payload == "1" || payload.equalsIgnoreCase("TRUE");
            relayManager->setRelay(relayNumber - 1, state);
            publishRelayStates();
        }
    }
}

void MqttHandler::publishTopic(const String &suffix, const String &payload, bool retained)
{
    if (!mqttClient.connected())
    {
        return;
    }

    const String topic = String(SMARTGARDEN_MQTT_ROOT_TOPIC) + "/" + suffix;
    mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

void MqttHandler::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    if (activeInstance == nullptr)
    {
        return;
    }

    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; ++i)
    {
        message += static_cast<char>(payload[i]);
    }

    activeInstance->onMessage(String(topic), message);
}
