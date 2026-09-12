#include "discovery_service.h"

#include "app_config.h"
#include "garden_profile.h"

namespace {

constexpr uint8_t RELAY_COUNT = 8;

const char* const RELAY_OBJECT_IDS[RELAY_COUNT] = {
    "fan",
    "heater",
    "cooler",
    "humidifier",
    "dehumidifier",
    "irrigation",
    "relay7",
    "relay8"
};

const char* const RELAY_NAMES[RELAY_COUNT] = {
    "Circulation Fan",
    "Heater",
    "Cooler",
    "Humidifier",
    "Dehumidifier",
    "Irrigation",
    "Relay 7",
    "Relay 8"
};

const char* const RELAY_ICONS[RELAY_COUNT] = {
    "mdi:fan",
    "mdi:radiator",
    "mdi:snowflake",
    "mdi:air-humidifier",
    "mdi:water-off",
    "mdi:watering-can",
    "mdi:toggle-switch",
    "mdi:toggle-switch"
};

String deviceBlock(const char* deviceId)
{
    String block = "\"dev\":{\"ids\":[\"";
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
        options += crops[i].name;
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
    struct SensorConfig {
        const char* objectId;
        const char* name;
        const char* deviceClass;
        const char* stateClass;
        const char* unit;
        const char* icon;
    };

    const SensorConfig sensors[] = {
        {"air_temperature", "Air Temperature", "temperature", "measurement", "\xC2\xB0""C", "mdi:thermometer"},
        {"air_humidity", "Air Humidity", "humidity", "measurement", "%", "mdi:water-percent"},
        {"soil_moisture", "Soil Moisture", "moisture", "measurement", "%", "mdi:sprout"},
        {"soil_temperature", "Soil Temperature", "temperature", "measurement", "\xC2\xB0""C", "mdi:thermometer-lines"},
        {"ph", "pH Value", nullptr, "measurement", nullptr, "mdi:ph"},
        {"ec", "EC", nullptr, "measurement", "uS/cm", "mdi:flash"},
        {"nitrogen", "Nitrogen", nullptr, "measurement", "mg/kg", "mdi:leaf"},
        {"phosphorus", "Phosphorus", nullptr, "measurement", "mg/kg", "mdi:flask"},
        {"potassium", "Potassium", nullptr, "measurement", "mg/kg", "mdi:periodic-table"}
    };

    const char* deviceId = mqtt.getDeviceId();
    for (const auto& sensor : sensors) {
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
    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        String payload =
            "{"
            "\"name\":\"" + String(RELAY_NAMES[i]) + "\","
            "\"object_id\":\"" + String(deviceId) + "_" + RELAY_OBJECT_IDS[i] + "\","
            "\"unique_id\":\"" + String(deviceId) + "_" + RELAY_OBJECT_IDS[i] + "\","
            "\"command_topic\":\"" + mqtt.getRelayCommandTopic(i) + "\","
            "\"state_topic\":\"" + mqtt.getRelayStateTopic(i) + "\","
            "\"payload_on\":\"ON\","
            "\"payload_off\":\"OFF\","
            "\"icon\":\"" + String(RELAY_ICONS[i]) + "\","
            + availabilityBlock(mqtt) + ","
            + deviceBlock(deviceId) +
            "}";

        mqtt.publishRaw(discoveryTopic("switch", deviceId, RELAY_OBJECT_IDS[i]).c_str(), payload.c_str(), true);
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
