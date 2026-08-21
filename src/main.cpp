#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n========== SmartGarden Test ===========");
    Serial.println("[App] Serial is working!");
}

void loop()
{
    Serial.println("[Loop] Running...");
    delay(1000);
}
