#pragma once

#include <Arduino.h>

// Charging parameters
constexpr int MIN_AMPS = 6;
constexpr int MAX_AMPS = 16;
constexpr int VOLTAGE = 230;

// Modbus RTU over a TTL-to-RS485 module, wired directly to the ESP8266
// (module RO/DI/RE+DE -> ESP8266 GPIOs; module A/B -> wallbox RS485 A/B).
// Module RE and DE pins must be bridged together and driven by MODBUS_RTU_DE_RE_PIN.
constexpr int MODBUS_RTU_RX_PIN = D2;    // GPIO4, module RO -> here
constexpr int MODBUS_RTU_TX_PIN = D1;    // GPIO5, module DI <- here
constexpr int MODBUS_RTU_DE_RE_PIN = D7; // GPIO13, module RE+DE (bridged) <- here
constexpr unsigned long MODBUS_BAUD = 19200; // matches the wallbox's fixed RTU line rate (19200 8E1)

constexpr uint8_t MODBUS_SLAVE_ID = 1; // wallbox's DIP-switched RS485 address

// Modbus request/refresh timing
constexpr unsigned long MODBUS_POLL_INTERVAL_MS = 1000;      // how often to read charging state + power
constexpr unsigned long MODBUS_WATCHDOG_REFRESH_MS = 5000;   // re-write current setpoint to keep the wallbox's Modbus watchdog (register 257, default 15000ms) from expiring
