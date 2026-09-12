#include "discovery_service.h"

#include "app_config.h"
#include "garden_profile.h"
#include "smartgarden_topics.h"

namespace {

void appendEscapedJsonString(String& out, const char* value)
{
    for (const unsigned char* ptr = reinterpret_cast<const unsigned char*>(value); *ptr != '\0'; ++ptr) {
        switch (*ptr) {
            case '\"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (*ptr < 0x20) {
                    char encoded[7];
                    snprintf(encoded, sizeof(encoded), "\\u%04x", *ptr);
                    out += encoded;
                } else {
                    out += static_cast<char>(*ptr);
                }
                break;
        }
    }
}

String jsonString(const char* value)
{
    String out = "\"";
    appendEscapedJsonString(out, value);
    out += "\"";
    return out;
}

String deviceBlock(const char* deviceId)
{
    String block = "\"device\":{\"identifiers\":[";
    block += jsonString(deviceId);
    block += "],\"name\":";
    block += jsonString(APP_NAME);
    block += ",\"manufacturer\":";
    block += jsonString("tomnyle");
    block += ",\"model\":";
    block += jsonString("ESP32 Smart Garden Controller");
    block += ",\"sw_version\":";
    block += jsonString(APP_VERSION);
    block += "}";
    return block;
}

String availabilityBlock(MQTTService& mqtt)
{
    return "\"availability_topic\":" + jsonString(mqtt.getStatusTopic().c_str()) +
           ",\"payload_available\":" + jsonString("online") +
           ",\"payload_not_available\":" + jsonString("offline");
}

String discoveryTopic(const char* domain, const char* deviceId, const char* objectId)
{
    return "homeassistant/" + String(domain) + "/" + deviceId + "/" + objectId + "/config";
}

String quotedList()
{
    CropProfileStore::initialize();
    uint8_t count = 0;
    const CropProfile* crops = CropProfileStore::getAllCrops(count);

    String options = "[";
    for (uint8_t i = 0; i < count; ++i) {
        if (i > 0) {
            options += ",";
        }
        options += "\"";
        appendEscapedJsonString(options, crops[i].name);
        options += "\"";
    }
    options += "]";
    return options;
}

} // namespace

DiscoveryService::DiscoveryService(MQTTService& mqttService)
    : mqtt(mqttService)
{
}

void DiscoveryService::begin()
{
    if (!mqtt.isConnected()) {
        return;
    }

    Serial.println("[Discovery] Publishing Home Assistant discovery");
    publishStatusEntity();
    publishFirmwareEntity();
    publishSensorEntities();
    publishRelayEntities();
    publishCropEntity();
    Serial.println("[Discovery] Discovery publish complete");
}

void DiscoveryService::publishStatusEntity()
{
    const char* deviceId = mqtt.getDeviceId();
    String payload = "{";
    payload += "\"name\":";
    payload += jsonString("ESP32 Status");
    payload += ",\"object_id\":";
    payload += jsonString((String(deviceId) + "_status").c_str());
    payload += ",\"unique_id\":";
    payload += jsonString((String(deviceId) + "_status").c_str());
    payload += ",\"state_topic\":";
    payload += jsonString(mqtt.getStatusTopic().c_str());
    payload += ",\"payload_on\":";
    payload += jsonString("online");
    payload += ",\"payload_off\":";
    payload += jsonString("offline");
    payload += ",\"device_class\":";
    payload += jsonString("connectivity");
    payload += ",";
    payload += availabilityBlock(mqtt);
    payload += ",";
    payload += deviceBlock(deviceId);
    payload += "}";

    mqtt.publishRaw(discoveryTopic("binary_sensor", deviceId, "status").c_str(), payload.c_str(), true);
}

void DiscoveryService::publishFirmwareEntity()
{
    const char* deviceId = mqtt.getDeviceId();
    String payload = "{";
    payload += "\"name\":";
    payload += jsonString("Firmware");
    payload += ",\"object_id\":";
    payload += jsonString((String(deviceId) + "_firmware").c_str());
    payload += ",\"unique_id\":";
    payload += jsonString((String(deviceId) + "_firmware").c_str());
    payload += ",\"state_topic\":";
    payload += jsonString(mqtt.getFirmwareTopic().c_str());
    payload += ",\"entity_category\":";
    payload += jsonString("diagnostic");
    payload += ",\"icon\":";
    payload += jsonString("mdi:chip");
    payload += ",";
    payload += availabilityBlock(mqtt);
    payload += ",";
    payload += deviceBlock(deviceId);
    payload += "}";

    mqtt.publishRaw(discoveryTopic("sensor", deviceId, "firmware").c_str(), payload.c_str(), true);
}

void DiscoveryService::publishSensorEntities()
{
    const char* deviceId = mqtt.getDeviceId();
    for (uint8_t i = 0; i < SMARTGARDEN_SENSOR_COUNT; ++i) {
        const SensorEntityConfig& sensor = SMARTGARDEN_SENSORS[i];
        String payload =
            "{"
            "\"name\":" + jsonString(sensor.name) + ","
            "\"object_id\":" + jsonString((String(deviceId) + "_" + sensor.objectId).c_str()) + ","
            "\"unique_id\":" + jsonString((String(deviceId) + "_" + sensor.objectId).c_str()) + ","
            "\"state_topic\":" + jsonString(mqtt.getSensorTopic(sensor.objectId).c_str());

        if (sensor.deviceClass != nullptr) {
            payload += ",\"device_class\":";
            payload += jsonString(sensor.deviceClass);
        }
        if (sensor.stateClass != nullptr) {
            payload += ",\"state_class\":";
            payload += jsonString(sensor.stateClass);
        }
        if (sensor.unit != nullptr) {
            payload += ",\"unit_of_measurement\":";
            payload += jsonString(sensor.unit);
        }
        if (sensor.icon != nullptr) {
            payload += ",\"icon\":";
            payload += jsonString(sensor.icon);
        }

        payload += ",";
        payload += availabilityBlock(mqtt);
        payload += ",";
        payload += deviceBlock(deviceId);
        payload += "}";

        mqtt.publishRaw(discoveryTopic("sensor", deviceId, sensor.objectId).c_str(), payload.c_str(), true);
    }
}

void DiscoveryService::publishRelayEntities()
{
    const char* deviceId = mqtt.getDeviceId();
    for (uint8_t i = 0; i < SMARTGARDEN_RELAY_COUNT; ++i) {
        const RelayEntityConfig& relay = SMARTGARDEN_RELAYS[i];
        String payload = "{";
        payload += "\"name\":";
        payload += jsonString(relay.name);
        payload += ",\"object_id\":";
        payload += jsonString((String(deviceId) + "_" + relay.objectId).c_str());
        payload += ",\"unique_id\":";
        payload += jsonString((String(deviceId) + "_" + relay.objectId).c_str());
        payload += ",\"command_topic\":";
        payload += jsonString(mqtt.getRelayCommandTopic(i).c_str());
        payload += ",\"state_topic\":";
        payload += jsonString(mqtt.getRelayStateTopic(i).c_str());
        payload += ",\"payload_on\":";
        payload += jsonString("ON");
        payload += ",\"payload_off\":";
        payload += jsonString("OFF");
        payload += ",\"icon\":";
        payload += jsonString(relay.icon);
        payload += ",";
        payload += availabilityBlock(mqtt);
        payload += ",";
        payload += deviceBlock(deviceId);
        payload += "}";

        mqtt.publishRaw(discoveryTopic("switch", deviceId, relay.objectId).c_str(), payload.c_str(), true);
    }
}

void DiscoveryService::publishCropEntity()
{
    const char* deviceId = mqtt.getDeviceId();
    String payload = "{";
    payload += "\"name\":";
    payload += jsonString("Crop Profile");
    payload += ",\"object_id\":";
    payload += jsonString((String(deviceId) + "_crop").c_str());
    payload += ",\"unique_id\":";
    payload += jsonString((String(deviceId) + "_crop").c_str());
    payload += ",\"command_topic\":";
    payload += jsonString(mqtt.getCropSelectTopic().c_str());
    payload += ",\"state_topic\":";
    payload += jsonString(mqtt.getCropCurrentTopic().c_str());
    payload += ",\"options\":";
    payload += quotedList();
    payload += ",\"icon\":";
    payload += jsonString("mdi:sprout");
    payload += ",";
    payload += availabilityBlock(mqtt);
    payload += ",";
    payload += deviceBlock(deviceId);
    payload += "}";

    mqtt.publishRaw(discoveryTopic("select", deviceId, "crop").c_str(), payload.c_str(), true);
}
