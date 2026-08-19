#ifndef PINS_H
#define PINS_H

// DHT22 Sensor Pin
#define DHT_PIN 4
#define DHT_TYPE DHT22

// RS485 Communication Pins (Serial2)
#define RS485_RX 16
#define RS485_TX 17
#define RS485_DE 18  // Direction Enable pin

// Relay Control Pins (8 relays)
#define RELAY_1_PIN 32
#define RELAY_2_PIN 33
#define RELAY_3_PIN 25
#define RELAY_4_PIN 26
#define RELAY_5_PIN 27
#define RELAY_6_PIN 14
#define RELAY_7_PIN 12
#define RELAY_8_PIN 13

// Relay array
const uint8_t RELAY_PINS[8] = {
    RELAY_1_PIN, RELAY_2_PIN, RELAY_3_PIN, RELAY_4_PIN,
    RELAY_5_PIN, RELAY_6_PIN, RELAY_7_PIN, RELAY_8_PIN
};

// Serial pins (for debugging)
#define SERIAL_RX 3
#define SERIAL_TX 1  // GPIO1 on ESP32

// Status LED (optional)
#define STATUS_LED_PIN 2

#endif // PINS_H
