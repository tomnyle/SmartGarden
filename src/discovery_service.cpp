#include "discovery_service.h"

#include "app_config.h"
#include "garden_profile.h"
#include "smartgarden_topics.h"

namespace {

String deviceBlock(const char* deviceId)
{
    String block = "\"device\":{\"identifiers\":[\"";
    block += deviceId;
    block += "\"],\"name\":\"";
    block += APP_NAME;
    block += "\",\"manufacturer\":\"tomnyle\",\"model\":\"ESP32 Smart Garden Controller\",\"sw_version\":\"";
    block += APP_VERSION;
    block += "\"}";
    return block;
}

String availabilityBlock(MQTTService& mqtt)
{
    return "\"availability_topic\":\"" + mqtt.getStatusTopic() +
           "\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
}

String discoveryTopic(const char* domain, const char* deviceId, const char* objectId)
{
    return "homeassistant/" + String(domain) + "/" + deviceId + "/" + objectId + "/config";
}

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
                out += static_cast<char>(*ptr);
                break;
        }
    }
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
    String payload =
        "{"
        "\"name\":\"ESP32 Status\","
        "\"object_id\":\"" + String(deviceId) + "_status\","
        "\"unique_id\":\"" + String(deviceId) + "_status\","
        "\"state_topic\":\"" + mqtt.getStatusTopic() + "\","
        "\"payload_on\":\"online\","
        "\"payload_off\":\"offline\","
        "\"device_class\":\"connectivity\","
        + availabilityBlock(mqtt) + ","
        + deviceBlock(deviceId) +
        "}";

    mqtt.publishRaw(discoveryTopic("binary_sensor", deviceId, "status").c_str(), payload.c_str(), true);
}

void DiscoveryService::publishFirmwareEntity()
{
    const char* deviceId = mqtt.getDeviceId();
    String payload =
        "{"
        "\"name\":\"Firmware\","
        "\"object_id\":\"" + String(deviceId) + "_firmware\","
        "\"unique_id\":\"" + String(deviceId) + "_firmware\","
        "\"state_topic\":\"" + mqtt.getFirmwareTopic() + "\","
        "\"entity_category\":\"diagnostic\","
        "\"icon\":\"mdi:chip\","
        + availabilityBlock(mqtt) + ","
        + deviceBlock(deviceId) +
        "}";

    mqtt.publishRaw(discoveryTopic("sensor", deviceId, "firmware").c_str(), payload.c_str(), true);
}

void DiscoveryService::publishSensorEntities()
{
    const char* deviceId = mqtt.getDeviceId();
    for (uint8_t i = 0; i < SMARTGARDEN_SENSOR_COUNT; ++i) {
        const SensorEntityConfig& sensor = SMARTGARDEN_SENSORS[i];
        String payload =
            "{"
            "\"name\":\"" + String(sensor.name) + "\","
            "\"object_id\":\"" + String(deviceId) + "_" + sensor.objectId + "\","
            "\"unique_id\":\"" + String(deviceId) + "_" + sensor.objectId + "\","
            "\"state_topic\":\"" + mqtt.getSensorTopic(sensor.objectId) + "\"";

        if (sensor.deviceClass != nullptr) {
            payload += ",\"device_class\":\"";
            payload += sensor.deviceClass;
            payload += "\"";
        }
        if (sensor.stateClass != nullptr) {
            payload += ",\"state_class\":\"";
            payload += sensor.stateClass;
            payload += "\"";
        }
        if (sensor.unit != nullptr) {
            payload += ",\"unit_of_measurement\":\"";
            payload += sensor.unit;
            payload += "\"";
        }
        if (sensor.icon != nullptr) {
            payload += ",\"icon\":\"";
            payload += sensor.icon;
            payload += "\"";
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
        String payload =
            "{"
            "\"name\":\"" + String(relay.name) + "\","
            "\"object_id\":\"" + String(deviceId) + "_" + relay.objectId + "\","
            "\"unique_id\":\"" + String(deviceId) + "_" + relay.objectId + "\","
            "\"command_topic\":\"" + mqtt.getRelayCommandTopic(i) + "\","
            "\"state_topic\":\"" + mqtt.getRelayStateTopic(i) + "\","
            "\"payload_on\":\"ON\","
            "\"payload_off\":\"OFF\","
            "\"icon\":\"" + String(relay.icon) + "\","
            + availabilityBlock(mqtt) + ","
            + deviceBlock(deviceId) +
            "}";

        mqtt.publishRaw(discoveryTopic("switch", deviceId, relay.objectId).c_str(), payload.c_str(), true);
    }
}

void DiscoveryService::publishCropEntity()
{
    const char* deviceId = mqtt.getDeviceId();
    String payload =
        "{"
        "\"name\":\"Crop Profile\","
        "\"object_id\":\"" + String(deviceId) + "_crop\","
        "\"unique_id\":\"" + String(deviceId) + "_crop\","
        "\"command_topic\":\"" + mqtt.getCropSelectTopic() + "\","
        "\"state_topic\":\"" + mqtt.getCropCurrentTopic() + "\","
        "\"options\":" + quotedList() + ","
        "\"icon\":\"mdi:sprout\","
        + availabilityBlock(mqtt) + ","
        + deviceBlock(deviceId) +
        "}";

    mqtt.publishRaw(discoveryTopic("select", deviceId, "crop").c_str(), payload.c_str(), true);
}
