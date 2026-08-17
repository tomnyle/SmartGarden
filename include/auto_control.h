#pragma once

#include <Arduino.h>

#include "config.h"
#include "crop_profiles.h"
#include "pins.h"

struct SensorSnapshot
{
    float temperature;
    float airHumidity;
    float soilHumidity;
    float ph;
    float ec;
    float nitrogen;
    float phosphorus;
    float potassium;
    unsigned long capturedAtMs;
};

struct RelayCommand
{
    uint8_t relayIndex;
    bool turnOn;
};

struct AlertMessage
{
    const char *metric;
    float value;
    RangeValue expected;
};

struct AutoControlResult
{
    RelayCommand commands[MAX_RELAY_RULES];
    size_t commandCount;
    AlertMessage alerts[MAX_ALERT_MESSAGES];
    size_t alertCount;
};

class AutoControlEngine
{
public:
    AutoControlResult evaluate(const CropProfile &profile, const SensorSnapshot &snapshot, const bool relayStates[RELAY_COUNT]) const
    {
        AutoControlResult result{};
        result.commandCount = 0;
        result.alertCount = 0;

        evaluateRange("temperature", profile.temperature, snapshot.temperature, result);
        evaluateRange("air_humidity", profile.airHumidity, snapshot.airHumidity, result);
        evaluateRange("soil_humidity", profile.soilHumidity, snapshot.soilHumidity, result);
        evaluateRange("ph", profile.ph, snapshot.ph, result);
        evaluateRange("ec", profile.ec, snapshot.ec, result);
        evaluateRange("nitrogen", profile.nitrogen, snapshot.nitrogen, result);
        evaluateRange("phosphorus", profile.phosphorus, snapshot.phosphorus, result);
        evaluateRange("potassium", profile.potassium, snapshot.potassium, result);

        if (snapshot.soilHumidity < profile.soilHumidity.min)
        {
            addCommand(result, IRRIGATION_RELAY_INDEX, true);
        }
        else if (snapshot.soilHumidity > profile.soilHumidity.max)
        {
            addCommand(result, IRRIGATION_RELAY_INDEX, false);
        }

        for (size_t i = 0; i < profile.relayRuleCount; ++i)
        {
            const RelayRule &rule = profile.relayRules[i];
            const uint32_t cycle = rule.onDurationMs + rule.offDurationMs;
            if (cycle == 0)
            {
                continue;
            }

            const uint32_t cyclePosition = static_cast<uint32_t>(snapshot.capturedAtMs % cycle);
            const bool shouldBeOn = cyclePosition < rule.onDurationMs;
            if (rule.relayIndex < RELAY_COUNT && relayStates[rule.relayIndex] != shouldBeOn)
            {
                addCommand(result, rule.relayIndex, shouldBeOn);
            }
        }

        return result;
    }

private:
    static void evaluateRange(const char *metric, const RangeValue &range, float value, AutoControlResult &result)
    {
        if (!range.contains(value) && result.alertCount < MAX_ALERT_MESSAGES)
        {
            result.alerts[result.alertCount++] = {metric, value, range};
        }
    }

    static void addCommand(AutoControlResult &result, uint8_t relayIndex, bool turnOn)
    {
        for (size_t i = 0; i < result.commandCount; ++i)
        {
            if (result.commands[i].relayIndex == relayIndex)
            {
                result.commands[i].turnOn = turnOn;
                return;
            }
        }

        if (result.commandCount < MAX_RELAY_RULES)
        {
            result.commands[result.commandCount++] = {relayIndex, turnOn};
        }
    }
};
