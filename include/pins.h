#pragma once

// ========================================
// Smart Garden - Pin Configuration
// ========================================

// ---------- RS485 / MAX485 ----------
#define PIN_RS485_RX       16
#define PIN_RS485_TX       17
#define PIN_RS485_RE_DE     4

// ---------- DHT22 ----------
#define PIN_DHT22          15

// ---------- I2C ----------
#define PIN_I2C_SDA        21
#define PIN_I2C_SCL        22

// ---------- Relay ----------
#define RELAY_COUNT         8

static const uint8_t RELAY_PINS[RELAY_COUNT] = {
    5, 18, 19, 27, 32, 33, 25, 26
};

// Relay module active LOW
#define RELAY_ACTIVE_LOW    true

// ---------- Status LED ----------
#define PIN_STATUS_LED       2