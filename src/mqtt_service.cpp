#include "mqtt_service.h"

MqttService *MqttService::_instance = nullptr;

MqttService::MqttService(const char *host, uint16_t port,
                         const char *username, const char *password,
                         const char *clientId)
    : _port(port), _relayCount(8), _userCallback(nullptr)
{
    strncpy(_host,     host,     sizeof(_host) - 1);
    strncpy(_username, username, sizeof(_username) - 1);
    strncpy(_password, password, sizeof(_password) - 1);
    strncpy(_clientId, clientId, sizeof(_clientId) - 1);
    _host[sizeof(_host) - 1]         = '\0';
    _username[sizeof(_username) - 1] = '\0';
    _password[sizeof(_password) - 1] = '\0';
    _clientId[sizeof(_clientId) - 1] = '\0';

    _instance = this;
}

void MqttService::begin(WiFiClient &wifiClient)
{
    _client.setClient(wifiClient);
    _client.setServer(_host, _port);
    _client.setCallback(_internalCallback);
    Serial.printf("[MqttService] Configured broker %s:%u\n", _host, _port);
}

bool MqttService::connect()
{
    Serial.print("[MqttService] Connecting...");

    bool ok = _client.connect(_clientId,
                              strlen(_username) ? _username : nullptr,
                              strlen(_password) ? _password : nullptr);
    if (ok)
    {
        Serial.println(" CONNECTED");
        subscribe(_relayCount);
    }
    else
    {
        Serial.printf(" FAILED (state=%d)\n", _client.state());
    }
    return ok;
}

bool MqttService::isConnected() const
{
    return _client.connected();
}

void MqttService::subscribe(int relayCount)
{
    _relayCount = relayCount;
    _client.subscribe("smartgarden/crop/set");

    for (int i = 1; i <= relayCount; i++)
    {
        String t = "smartgarden/relay/" + String(i) + "/set";
        _client.subscribe(t.c_str());
    }
    Serial.println("[MqttService] Subscribed to topics");
}

void MqttService::publish(const SensorSnapshot &s)
{
    _client.publish("smartgarden/sensors/air_temp",     String(s.airTemp, 1).c_str(),     true);
    _client.publish("smartgarden/sensors/air_humidity", String(s.airHumidity, 1).c_str(), true);
    _client.publish("smartgarden/sensors/soil_moisture",String(s.soilMoisture, 1).c_str(),true);
    _client.publish("smartgarden/sensors/soil_temp",    String(s.soilTemp, 1).c_str(),    true);
    _client.publish("smartgarden/sensors/ph",           String(s.ph, 1).c_str(),          true);
    _client.publish("smartgarden/sensors/ec",           String(s.ec).c_str(),             true);
    _client.publish("smartgarden/sensors/nitrogen",     String(s.nitrogen).c_str(),       true);
    _client.publish("smartgarden/sensors/phosphorus",   String(s.phosphorus).c_str(),     true);
    _client.publish("smartgarden/sensors/potassium",    String(s.potassium).c_str(),      true);
}

void MqttService::publishCropList(const CropProfileStore &store)
{
    String list = "";
    for (size_t i = 0; i < store.count(); i++)
    {
        if (i > 0) list += ",";
        const CropProfile *p = store.getAt(i);
        if (p) list += p->name;
    }
    _client.publish("smartgarden/crop/list", list.c_str(), true);
    Serial.println("[MqttService] Crop list published: " + list);
}

void MqttService::publishCropConfig(const CropProfile *p)
{
    if (!p) return;

    _client.publish("smartgarden/crop/current", p->name, true);

    String json = "{";
    json += "\"name\":\"" + String(p->name) + "\",";
    json += "\"temperature\":{\"min\":" + String(p->temperature.min, 1) +
            ",\"max\":" + String(p->temperature.max, 1) + "},";
    json += "\"airHumidity\":{\"min\":" + String(p->airHumidity.min, 1) +
            ",\"max\":" + String(p->airHumidity.max, 1) + "},";
    json += "\"soilHumidity\":{\"min\":" + String(p->soilHumidity.min, 1) +
            ",\"max\":" + String(p->soilHumidity.max, 1) + "},";
    json += "\"ph\":{\"min\":" + String(p->ph.min, 1) +
            ",\"max\":" + String(p->ph.max, 1) + "},";
    json += "\"ec\":{\"min\":" + String(p->ec.min, 1) +
            ",\"max\":" + String(p->ec.max, 1) + "}";
    json += "}";

    _client.publish("smartgarden/crop/config", json.c_str(), true);
    Serial.printf("[MqttService] Crop config published for: %s\n", p->name);
}

void MqttService::publishRelayState(int relayIndex, bool state)
{
    String topic = "smartgarden/relay/" + String(relayIndex + 1) + "/state";
    _client.publish(topic.c_str(), state ? "ON" : "OFF", true);
}

void MqttService::setCallback(MessageCallback cb)
{
    _userCallback = cb;
}

void MqttService::loop()
{
    if (!_client.connected())
        connect();
    _client.loop();
}

void MqttService::_internalCallback(char *topic, uint8_t *payload, unsigned int length)
{
    if (_instance && _instance->_userCallback)
        _instance->_userCallback(topic, payload, length);
}
