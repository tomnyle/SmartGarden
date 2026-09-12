#ifndef SMARTGARDEN_TOPICS_H
#define SMARTGARDEN_TOPICS_H

#include <stdint.h>

struct RelayEntityConfig {
    const char* objectId;
    const char* name;
    const char* icon;
};

enum SensorField {
    SENSOR_AIR_TEMPERATURE,
    SENSOR_AIR_HUMIDITY,
    SENSOR_SOIL_MOISTURE,
    SENSOR_SOIL_TEMPERATURE,
    SENSOR_PH,
    SENSOR_EC,
    SENSOR_NITROGEN,
    SENSOR_PHOSPHORUS,
    SENSOR_POTASSIUM
};

struct SensorEntityConfig {
    SensorField field;
    const char* objectId;
    const char* name;
    const char* deviceClass;
    const char* stateClass;
    const char* unit;
    const char* icon;
    bool integerValue;
};

static const RelayEntityConfig SMARTGARDEN_RELAYS[] = {
    {"fan", "Circulation Fan", "mdi:fan"},
    {"heater", "Heater", "mdi:radiator"},
    {"cooler", "Cooler", "mdi:snowflake"},
    {"humidifier", "Humidifier", "mdi:air-humidifier"},
    {"dehumidifier", "Dehumidifier", "mdi:water-off"},
    {"irrigation", "Irrigation", "mdi:watering-can"},
    {"relay7", "Relay 7", "mdi:toggle-switch"},
    {"relay8", "Relay 8", "mdi:toggle-switch"}
};

static const SensorEntityConfig SMARTGARDEN_SENSORS[] = {
    {SENSOR_AIR_TEMPERATURE, "air_temperature", "Air Temperature", "temperature", "measurement", "\xC2\xB0""C", "mdi:thermometer", false},
    {SENSOR_AIR_HUMIDITY, "air_humidity", "Air Humidity", "humidity", "measurement", "%", "mdi:water-percent", false},
    {SENSOR_SOIL_MOISTURE, "soil_moisture", "Soil Moisture", "moisture", "measurement", "%", "mdi:sprout", false},
    {SENSOR_SOIL_TEMPERATURE, "soil_temperature", "Soil Temperature", "temperature", "measurement", "\xC2\xB0""C", "mdi:thermometer-lines", false},
    {SENSOR_PH, "ph", "pH Value", nullptr, "measurement", nullptr, "mdi:ph", false},
    {SENSOR_EC, "ec", "EC", nullptr, "measurement", "uS/cm", "mdi:flash", true},
    {SENSOR_NITROGEN, "nitrogen", "Nitrogen", nullptr, "measurement", "mg/kg", "mdi:leaf", true},
    {SENSOR_PHOSPHORUS, "phosphorus", "Phosphorus", nullptr, "measurement", "mg/kg", "mdi:flask", true},
    {SENSOR_POTASSIUM, "potassium", "Potassium", nullptr, "measurement", "mg/kg", "mdi:periodic-table", true}
};

static const uint8_t SMARTGARDEN_RELAY_COUNT =
    sizeof(SMARTGARDEN_RELAYS) / sizeof(SMARTGARDEN_RELAYS[0]);

static const uint8_t SMARTGARDEN_SENSOR_COUNT =
    sizeof(SMARTGARDEN_SENSORS) / sizeof(SMARTGARDEN_SENSORS[0]);

#endif // SMARTGARDEN_TOPICS_H
