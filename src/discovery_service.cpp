#include "discovery_service.h"

namespace {
struct SensorEntityConfig {
    const char* name;
    const char* objectId;
    const char* uniqueId;
    const char* stateTopic;
    const char* unitOfMeasurement;
    const char* deviceClass;
    const char* stateClass;
};

struct SwitchEntityConfig {
    const char* name;
    const char* objectId;
    const char* uniqueId;
    const char* stateTopic;
    const char* commandTopic;
};

const SensorEntityConfig SENSOR_ENTITIES[] = {
    {"Air Temperature", "smartgarden_air_temp", "smartgarden_air_temp", "smartgarden/sensors/air_temp", "°C", "temperature", "measurement"},
    {"Air Humidity", "smartgarden_air_humidity", "smartgarden_air_humidity", "smartgarden/sensors/air_humidity", "%", "humidity", "measurement"},
    {"Soil Moisture", "smartgarden_soil_moisture", "smartgarden_soil_moisture", "smartgarden/sensors/soil_moisture", "%", "moisture", "measurement"},
    {"Soil Temperature", "smartgarden_soil_temp", "smartgarden_soil_temp", "smartgarden/sensors/soil_temp", "°C", "temperature", "measurement"},
    {"pH Value", "smartgarden_ph", "smartgarden_ph", "smartgarden/sensors/ph", "pH", nullptr, "measurement"},
    {"EC", "smartgarden_ec", "smartgarden_ec", "smartgarden/sensors/ec", "uS/cm", nullptr, "measurement"},
    {"Nitrogen", "smartgarden_nitrogen", "smartgarden_nitrogen", "smartgarden/sensors/nitrogen", "mg/kg", nullptr, "measurement"},
    {"Phosphorus", "smartgarden_phosphorus", "smartgarden_phosphorus", "smartgarden/sensors/phosphorus", "mg/kg", nullptr, "measurement"},
    {"Potassium", "smartgarden_potassium", "smartgarden_potassium", "smartgarden/sensors/potassium", "mg/kg", nullptr, "measurement"},
};

const SwitchEntityConfig SWITCH_ENTITIES[] = {
    {"Fan", "smartgarden_relay_1", "smartgarden_relay_1", "smartgarden/relay/1/state", "smartgarden/relay/1/set"},
    {"Heater", "smartgarden_relay_2", "smartgarden_relay_2", "smartgarden/relay/2/state", "smartgarden/relay/2/set"},
    {"Cooler", "smartgarden_relay_3", "smartgarden_relay_3", "smartgarden/relay/3/state", "smartgarden/relay/3/set"},
    {"Humidifier", "smartgarden_relay_4", "smartgarden_relay_4", "smartgarden/relay/4/state", "smartgarden/relay/4/set"},
    {"Dehumidifier", "smartgarden_relay_5", "smartgarden_relay_5", "smartgarden/relay/5/state", "smartgarden/relay/5/set"},
    {"Irrigation", "smartgarden_relay_6", "smartgarden_relay_6", "smartgarden/relay/6/state", "smartgarden/relay/6/set"},
    {"Relay7", "smartgarden_relay_7", "smartgarden_relay_7", "smartgarden/relay/7/state", "smartgarden/relay/7/set"},
    {"Relay8", "smartgarden_relay_8", "smartgarden_relay_8", "smartgarden/relay/8/state", "smartgarden/relay/8/set"},
};
}

DiscoveryService::DiscoveryService(PubSubClient& client,
                                   const char* deviceId,
                                   const char* deviceName,
                                   const char* manufacturer,
                                   const char* model,
                                   const char* availabilityTopic)
    : client(client),
      deviceId(deviceId),
      deviceName(deviceName),
      manufacturer(manufacturer),
      model(model),
      availabilityTopic(availabilityTopic) {}

bool DiscoveryService::begin(size_t relayCount) {
    Serial.println("[MQTT Discovery] Publishing Home Assistant discovery...");
    bool allPublished = true;

    for (const SensorEntityConfig& entity : SENSOR_ENTITIES) {
        allPublished = publishConfig("sensor",
                                     entity.objectId,
                                     buildSensorPayload(entity.name,
                                                        entity.objectId,
                                                        entity.uniqueId,
                                                        entity.stateTopic,
                                                        entity.unitOfMeasurement,
                                                        entity.deviceClass,
                                                        entity.stateClass)) && allPublished;
    }

    const size_t switchCount = relayCount < (sizeof(SWITCH_ENTITIES) / sizeof(SWITCH_ENTITIES[0]))
                                 ? relayCount
                                 : (sizeof(SWITCH_ENTITIES) / sizeof(SWITCH_ENTITIES[0]));

    for (size_t index = 0; index < switchCount; ++index) {
        const SwitchEntityConfig& entity = SWITCH_ENTITIES[index];
        allPublished = publishConfig("switch",
                                     entity.objectId,
                                     buildSwitchPayload(entity.name,
                                                        entity.objectId,
                                                        entity.uniqueId,
                                                        entity.stateTopic,
                                                        entity.commandTopic)) && allPublished;
    }

    Serial.printf("[MQTT Discovery] Discovery publish %s\n", allPublished ? "complete" : "incomplete");
    return allPublished;
}

bool DiscoveryService::publishConfig(const char* component, const char* objectId, const String& payload) {
    const String topic = String("homeassistant/") + component + "/" + objectId + "/config";
    const size_t requiredSize = payload.length() + topic.length() + 16;
    if (requiredSize > client.getBufferSize()) {
        Serial.printf("[MQTT Discovery] FAILED %s (payload %u > buffer %u)\n",
                      objectId,
                      static_cast<unsigned int>(requiredSize),
                      static_cast<unsigned int>(client.getBufferSize()));
        return false;
    }

    const bool published = client.publish(topic.c_str(), payload.c_str(), true);

    Serial.printf("[MQTT Discovery] %s %s\n",
                  published ? "OK" : "FAILED",
                  objectId);

    return published;
}

String DiscoveryService::buildDeviceJson() const {
    String payload;
    payload.reserve(192);
    payload += "\"device\":{";
    payload += "\"identifiers\":[\"";
    payload += escapeJson(deviceId);
    payload += "\"],";
    payload += "\"name\":\"";
    payload += escapeJson(deviceName);
    payload += "\",";
    payload += "\"manufacturer\":\"";
    payload += escapeJson(manufacturer);
    payload += "\",";
    payload += "\"model\":\"";
    payload += escapeJson(model);
    payload += "\"";
    payload += "}";
    return payload;
}

String DiscoveryService::buildAvailabilityJson() const {
    String payload;
    payload.reserve(128);
    payload += "\"availability_topic\":\"";
    payload += escapeJson(availabilityTopic);
    payload += "\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
    return payload;
}

String DiscoveryService::escapeJson(const char* value) const {
    String escaped;
    if (value == nullptr) {
        return escaped;
    }

    escaped.reserve(strlen(value) + 8);
    while (*value != '\0') {
        switch (*value) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += *value;
                break;
        }
        ++value;
    }

    return escaped;
}

String DiscoveryService::buildSensorPayload(const char* name,
                                            const char* objectId,
                                            const char* uniqueId,
                                            const char* stateTopic,
                                            const char* unitOfMeasurement,
                                            const char* deviceClass,
                                            const char* stateClass) const {
    String payload;
    payload.reserve(512);
    payload += "{";
    payload += "\"name\":\"";
    payload += escapeJson(name);
    payload += "\",";
    payload += "\"object_id\":\"";
    payload += escapeJson(objectId);
    payload += "\",";
    payload += "\"unique_id\":\"";
    payload += escapeJson(uniqueId);
    payload += "\",";
    payload += "\"state_topic\":\"";
    payload += escapeJson(stateTopic);
    payload += "\",";
    if (unitOfMeasurement && unitOfMeasurement[0] != '\0') {
        payload += "\"unit_of_measurement\":\"";
        payload += escapeJson(unitOfMeasurement);
        payload += "\",";
    }
    if (deviceClass && deviceClass[0] != '\0') {
        payload += "\"device_class\":\"";
        payload += escapeJson(deviceClass);
        payload += "\",";
    }
    if (stateClass && stateClass[0] != '\0') {
        payload += "\"state_class\":\"";
        payload += escapeJson(stateClass);
        payload += "\",";
    }
    payload += buildAvailabilityJson();
    payload += ",";
    payload += buildDeviceJson();
    payload += "}";
    return payload;
}

String DiscoveryService::buildSwitchPayload(const char* name,
                                            const char* objectId,
                                            const char* uniqueId,
                                            const char* stateTopic,
                                            const char* commandTopic) const {
    String payload;
    payload.reserve(512);
    payload += "{";
    payload += "\"name\":\"";
    payload += escapeJson(name);
    payload += "\",";
    payload += "\"object_id\":\"";
    payload += escapeJson(objectId);
    payload += "\",";
    payload += "\"unique_id\":\"";
    payload += escapeJson(uniqueId);
    payload += "\",";
    payload += "\"state_topic\":\"";
    payload += escapeJson(stateTopic);
    payload += "\",";
    payload += "\"command_topic\":\"";
    payload += escapeJson(commandTopic);
    payload += "\",";
    payload += "\"device_class\":\"switch\",";
    payload += "\"payload_on\":\"ON\",";
    payload += "\"payload_off\":\"OFF\",";
    payload += buildAvailabilityJson();
    payload += ",";
    payload += buildDeviceJson();
    payload += "}";
    return payload;
}
