#include <Arduino.h>
#include "wallbox.h"
#include "config.h"
#include "modbus.h"

// Heidelberg Energy Control Modbus register map, from the manufacturer's
// official "ModBus-Register-Tabelle" (Heidelberg/Amperfied, Feb 2021).
namespace {
  constexpr uint16_t REG_CHARGING_STATE = 5;   // input reg, FC04: 2=A1,3=A2,4=B1,5=B2,6=C1,7=C2,8=derating,9=E,10=F,11=err
  constexpr uint16_t REG_POWER = 14;           // input reg, FC04, sum of L1-L3, in VA
  constexpr uint16_t REG_ENERGY_POWERON = 15;  // input regs 15+16, FC04, VAh since last power-on, high word first
  constexpr uint16_t REG_ENERGY_TOTAL = 17;    // input regs 17+18, FC04, VAh since installation, high word first
  constexpr uint16_t REG_MAX_CURRENT = 261;    // holding reg, FC03/FC06, 0.1A steps, 0 or 60-160

  // Only state 7 (C2 = vehicle plugged, requesting, wallbox allows) means current is actually flowing.
  constexpr uint16_t STATE_C2_CHARGING = 7;
}

bool charging = false;
int currentAmp = 0;
int power = 0;
bool online = false;
uint32_t energySincePowerOn = 0;
uint32_t energyTotal = 0;

static int targetAmp = 0; // last commanded setpoint; re-sent periodically to hold the wallbox's Modbus watchdog open

static unsigned long lastPollMs = 0;
static unsigned long lastWatchdogRefreshMs = 0;

static void onChargingStateResult(bool success, uint16_t value) {
  online = success;
  if (success) {
    charging = (value == STATE_C2_CHARGING);
  }
  Serial.print("[modbus] read state reg ");
  Serial.print(REG_CHARGING_STATE);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
}

static void onPowerResult(bool success, uint16_t value) {
  online = success;
  if (success) {
    power = value;
  }
  Serial.print("[modbus] read power reg ");
  Serial.print(REG_POWER);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
}

static void onEnergyPowerOnResult(bool success, uint32_t value) {
  if (success) energySincePowerOn = value;
  Serial.print("[modbus] read energy-since-poweron regs ");
  Serial.print(REG_ENERGY_POWERON);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
}

static void onEnergyTotalResult(bool success, uint32_t value) {
  if (success) energyTotal = value;
  Serial.print("[modbus] read energy-total regs ");
  Serial.print(REG_ENERGY_TOTAL);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
}

static void writeCurrentSetpoint() {
  uint16_t deciamps = targetAmp > 0 ? (uint16_t)(targetAmp * 10) : 0;
  modbusWriteHoldingReg(REG_MAX_CURRENT, deciamps, [](bool success, uint16_t value) {
    online = success;
    Serial.print("[modbus] write current reg ");
    Serial.print(REG_MAX_CURRENT);
    Serial.print(" = ");
    Serial.print(value);
    Serial.println(success ? " OK" : " FAILED");
  });
}

void wallboxInit() {
  modbusInit();
}

void wallboxPoll() {
  modbusLoop();

  unsigned long now = millis();

  if (now - lastPollMs >= MODBUS_POLL_INTERVAL_MS) {
    lastPollMs = now;
    modbusReadInputReg(REG_CHARGING_STATE, onChargingStateResult);
    modbusReadInputReg(REG_POWER, onPowerResult);
    modbusReadInputReg32(REG_ENERGY_POWERON, onEnergyPowerOnResult);
    modbusReadInputReg32(REG_ENERGY_TOTAL, onEnergyTotalResult);
  }

  if (now - lastWatchdogRefreshMs >= MODBUS_WATCHDOG_REFRESH_MS) {
    lastWatchdogRefreshMs = now;
    writeCurrentSetpoint();
  }
}

void wallboxSetCurrent(int amps) {
  currentAmp = constrain(amps, MIN_AMPS, MAX_AMPS);
  targetAmp = currentAmp;
  writeCurrentSetpoint();

  Serial.print("Charging current set: ");
  Serial.print(currentAmp);
  Serial.println(" A");
}

void wallboxStop() {
  currentAmp = 0;
  targetAmp = 0;
  charging = false;
  writeCurrentSetpoint();

  Serial.println("Charging stopped");
}

String wallboxStatusJson() {
  String json = "{";
  json += "\"charging\":";
  json += charging ? "true" : "false";
  json += ",\"current\":";
  json += currentAmp;
  json += ",\"power\":";
  json += power;
  json += ",\"energySincePowerOn\":";
  json += energySincePowerOn;
  json += ",\"energyTotal\":";
  json += energyTotal;
  json += ",\"online\":";
  json += online ? "true" : "false";
  json += "}";
  return json;
}
