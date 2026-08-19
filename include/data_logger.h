#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

class DataLogger
{
private:
    static const size_t MAX_LOGS = 100;
    struct LogEntry
    {
        unsigned long timestamp;
        float airTemp;
        float airHumidity;
        float soilMoisture;
        float ph;
        uint16_t ec;
        char cropName[24];
    };

    LogEntry logs[MAX_LOGS];
    size_t logIndex = 0;
    size_t logCount = 0;

public:
    void logSensorData(float airTemp, float airHumidity, float soilMoisture, float ph, uint16_t ec, const char* cropName)
    {
        logs[logIndex].timestamp = millis();
        logs[logIndex].airTemp = airTemp;
        logs[logIndex].airHumidity = airHumidity;
        logs[logIndex].soilMoisture = soilMoisture;
        logs[logIndex].ph = ph;
        logs[logIndex].ec = ec;
        strncpy(logs[logIndex].cropName, cropName, sizeof(logs[logIndex].cropName) - 1);

        logIndex = (logIndex + 1) % MAX_LOGS;
        if (logCount < MAX_LOGS)
            logCount++;
    }

    void publishLogs(PubSubClient& client)
    {
        for (size_t i = 0; i < logCount; i++)
        {
            LogEntry& entry = logs[i];
            String json = "{";
            json += "\"timestamp\":" + String(entry.timestamp) + ",";
            json += "\"crop\":\"" + String(entry.cropName) + "\",";
            json += "\"airTemp\":" + String(entry.airTemp, 1) + ",";
            json += "\"airHumidity\":" + String(entry.airHumidity, 1) + ",";
            json += "\"soilMoisture\":" + String(entry.soilMoisture, 1) + ",";
            json += "\"ph\":" + String(entry.ph, 1) + ",";
            json += "\"ec\":" + String(entry.ec);
            json += "}";

            client.publish("smartgarden/logs", json.c_str(), false);
        }
    }

    size_t getLogCount() const
    {
        return logCount;
    }
};
