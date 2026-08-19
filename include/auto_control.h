#pragma once

#include <Arduino.h>
#include "crop_profiles.h"

struct SensorSnapshot
{
    float airTemp;
    float airHumidity;
    float soilMoisture;
    float soilTemp;
    float ph;
    uint16_t ec;
    uint16_t nitrogen;
    uint16_t phosphorus;
    uint16_t potassium;
    unsigned long timestamp;
};

struct RelayCommand
{
    uint8_t relayIndex;
    bool state;
    unsigned long timestamp;
};

class AutoControlSystem
{
public:
    static const size_t MAX_COMMANDS = 8;

    struct CommandQueue
    {
        RelayCommand commands[MAX_COMMANDS];
        size_t count;
    };

    static void evaluateAndControl(const CropProfile &profile, const SensorSnapshot &snapshot, CommandQueue &result)
    {
        result.count = 0;

        // Evaluate each sensor against profile ranges
        evaluateRange("temperature", profile.temperature, snapshot.airTemp, result);
        evaluateRange("airHumidity", profile.airHumidity, snapshot.airHumidity, result);
        evaluateRange("ph", profile.ph, snapshot.ph, result);
        evaluateRange("ec", profile.ec, snapshot.ec / 1000.0f, result);
        evaluateRange("nitrogen", profile.nitrogen, (float)snapshot.nitrogen, result);
        evaluateRange("phosphorus", profile.phosphorus, (float)snapshot.phosphorus, result);
        evaluateRange("potassium", profile.potassium, (float)snapshot.potassium, result);

        // Soil humidity priority control
        bool hasSoilPriorityCommand = false;
        if (snapshot.soilMoisture < profile.soilHumidity.min)
        {
            addCommand(result, IRRIGATION_RELAY_INDEX, true);
            hasSoilPriorityCommand = true;
        }
        else if (snapshot.soilMoisture > profile.soilHumidity.max)
        {
            addCommand(result, IRRIGATION_RELAY_INDEX, false);
            hasSoilPriorityCommand = true;
        }

        // Time-based relay rules (skip irrigation if soil priority is active)
        for (size_t i = 0; i < profile.relayRuleCount; ++i)
        {
            const RelayRule &rule = profile.relayRules[i];
            if (hasSoilPriorityCommand && rule.relayIndex == IRRIGATION_RELAY_INDEX)
            {
                continue;
            }

            const uint32_t cycle = rule.onDurationMs + rule.offDurationMs;
            if (cycle == 0)
                continue;

            const uint32_t elapsed = millis() % cycle;
            const bool shouldBeOn = elapsed < rule.onDurationMs;

            addCommand(result, rule.relayIndex, shouldBeOn);
        }
    }

private:
    static void evaluateRange(const char *name, const RangeValue &range, float value, CommandQueue &result)
    {
        if (!range.contains(value))
        {
            Serial.printf("[ALERT] %s out of range: %.1f (expected %.1f-%.1f)\n",
                          name, value, range.min, range.max);
        }
    }

    static void addCommand(CommandQueue &queue, uint8_t relayIndex, bool state)
    {
        if (queue.count >= MAX_COMMANDS)
            return;

        // Check if command already exists for this relay
        for (size_t i = 0; i < queue.count; ++i)
        {
            if (queue.commands[i].relayIndex == relayIndex)
            {
                queue.commands[i].state = state;
                return;
            }
        }

        // Add new command
        queue.commands[queue.count].relayIndex = relayIndex;
        queue.commands[queue.count].state = state;
        queue.commands[queue.count].timestamp = millis();
        queue.count++;
    }
};
