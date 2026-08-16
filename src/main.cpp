#include <Arduino.h>

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("      SMART GARDEN");
    Serial.println("==============================");
    Serial.println("ESP32 started successfully");
}

void loop()
{
    static unsigned long lastPrint = 0;

    if (millis() - lastPrint >= 5000)
    {
        lastPrint = millis();

        Serial.println("Smart Garden is running...");
    }
}