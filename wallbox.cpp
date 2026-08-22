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

  // The wallbox reports these as apparent power/energy (VA/VAh); we expose
  // them as W/Wh since EV charging is resistive/DC-rectified with a power
  // factor close to 1, making VA and W practically interchangeable here.

  // Raw charging-state register values. The car letter (A/B/C) says whether/how
  // a vehicle is plugged in; the digit (1/2) says whether the wallbox is
  // currently authorizing current flow - so C2 is the only value where
  // current actually flows. 9-11 (E/F/comms error) are lumped into Error.
  constexpr uint16_t STATE_A1 = 2;
  constexpr uint16_t STATE_A2 = 3;
  constexpr uint16_t STATE_B1 = 4;
  constexpr uint16_t STATE_B2 = 5;
  constexpr uint16_t STATE_C1 = 6;
  constexpr uint16_t STATE_C2 = 7;
  constexpr uint16_t STATE_DERATING = 8;

  ChargingState rawStateToChargingState(uint16_t value) {
    switch (value) {
      case STATE_A1:
      case STATE_A2:
        return ChargingState::Disconnected;
      case STATE_B1:
      case STATE_B2:
      case STATE_C1:
        return ChargingState::Connected;
      case STATE_C2:
        return ChargingState::Charging;
      case STATE_DERATING:
        return ChargingState::Derating;
      default:
        return ChargingState::Error;
    }
  }

  const char* chargingStateToString(ChargingState s) {
    switch (s) {
      case ChargingState::Disconnected: return "disconnected";
      case ChargingState::Connected: return "connected";
      case ChargingState::Charging: return "charging";
      case ChargingState::Derating: return "derating";
      case ChargingState::Error: return "error";
    }
    return "error";
  }

  // The RTU master only ever has one transaction in flight, so a poll cycle
  // has to be a strict chain rather than firing all reads at once - anything
  // fired while the previous one is still outstanding gets silently dropped
  // before it even reaches the wire.
  enum class PollStep { Idle, State, Power, EnergyPowerOn, EnergyTotal, Write };
  PollStep pollStep = PollStep::Idle;
  bool writeDueThisCycle = false;
  bool chainWriteInFlight = false; // guards against an unrelated manual write advancing the chain

  // A completed step must not issue the next request from inside its own
  // callback: the library invokes our callback before it resets its
  // "transaction in flight" state, so a send from in there is silently
  // dropped as busy. Instead just flag it and let wallboxPoll() advance the
  // chain on its next tick, once the library has actually gone idle.
  bool advancePending = false;

  constexpr unsigned long MODBUS_STUCK_RESET_MS = 10000; // no successful transaction for this long -> reset transport
  unsigned long lastSuccessMs = 0;

  void startNextStep();

  void noteResult(bool success) {
    if (success) lastSuccessMs = millis();
  }
}

ChargingState state = ChargingState::Disconnected;
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
    state = rawStateToChargingState(value);
  }
  noteResult(success);
  Serial.print("[modbus] read state reg ");
  Serial.print(REG_CHARGING_STATE);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
  advancePending = true;
}

static void onPowerResult(bool success, uint16_t value) {
  online = success;
  if (success) {
    power = value;
  }
  noteResult(success);
  Serial.print("[modbus] read power reg ");
  Serial.print(REG_POWER);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
  advancePending = true;
}

static void onEnergyPowerOnResult(bool success, uint32_t value) {
  if (success) energySincePowerOn = value;
  noteResult(success);
  Serial.print("[modbus] read energy-since-poweron regs ");
  Serial.print(REG_ENERGY_POWERON);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
  advancePending = true;
}

static void onEnergyTotalResult(bool success, uint32_t value) {
  if (success) energyTotal = value;
  noteResult(success);
  Serial.print("[modbus] read energy-total regs ");
  Serial.print(REG_ENERGY_TOTAL);
  Serial.print(": ");
  Serial.println(success ? String(value) : "FAILED");
  advancePending = true;
}

static void onWriteCurrentResult(bool success, uint16_t value) {
  online = success;
  noteResult(success);
  Serial.print("[modbus] write current reg ");
  Serial.print(REG_MAX_CURRENT);
  Serial.print(" = ");
  Serial.print(value);
  Serial.println(success ? " OK" : " FAILED");

  // Only advance the poll chain if this write was the one the chain itself
  // issued - a manual wallboxSetCurrent()/wallboxStop() write completing
  // must not push a mid-chain read state forward out of turn.
  if (chainWriteInFlight) {
    chainWriteInFlight = false;
    advancePending = true;
  }
}

static void writeCurrentSetpoint() {
  uint16_t deciamps = targetAmp > 0 ? (uint16_t)(targetAmp * 10) : 0;
  modbusWriteHoldingReg(REG_MAX_CURRENT, deciamps, onWriteCurrentResult);
}

namespace {
  void startNextStep() {
    switch (pollStep) {
      case PollStep::State:
        pollStep = PollStep::Power;
        modbusReadInputReg(REG_POWER, onPowerResult);
        break;
      case PollStep::Power:
        pollStep = PollStep::EnergyPowerOn;
        modbusReadInputReg32(REG_ENERGY_POWERON, onEnergyPowerOnResult);
        break;
      case PollStep::EnergyPowerOn:
        pollStep = PollStep::EnergyTotal;
        modbusReadInputReg32(REG_ENERGY_TOTAL, onEnergyTotalResult);
        break;
      case PollStep::EnergyTotal:
        if (writeDueThisCycle) {
          pollStep = PollStep::Write;
          chainWriteInFlight = true;
          writeCurrentSetpoint();
        } else {
          pollStep = PollStep::Idle;
        }
        break;
      case PollStep::Write:
      case PollStep::Idle:
        pollStep = PollStep::Idle;
        break;
    }
  }
}

void wallboxInit() {
  modbusInit();
  lastSuccessMs = millis(); // start the stuck-link countdown from boot, not just after the first success
}

void wallboxPoll() {
  modbusLoop();

  if (advancePending) {
    advancePending = false;
    startNextStep();
  }

  unsigned long now = millis();

  if (pollStep == PollStep::Idle && now - lastPollMs >= MODBUS_POLL_INTERVAL_MS) {
    lastPollMs = now;
    writeDueThisCycle = (now - lastWatchdogRefreshMs >= MODBUS_WATCHDOG_REFRESH_MS);
    if (writeDueThisCycle) lastWatchdogRefreshMs = now;
    pollStep = PollStep::State;
    modbusReadInputReg(REG_CHARGING_STATE, onChargingStateResult);
  }

  if (now - lastSuccessMs >= MODBUS_STUCK_RESET_MS) {
    Serial.println("[modbus] no successful transaction in a while, resetting link");
    modbusReset();
    pollStep = PollStep::Idle;
    chainWriteInFlight = false;
    lastSuccessMs = now; // avoid immediately re-triggering before the next cycle gets a chance
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
  writeCurrentSetpoint();
  // `state` is left alone here - whether the wallbox is now Connected or
  // Disconnected depends on the actual car, which only the next poll knows.

  Serial.println("Charging stopped");
}

String wallboxStatusJson() {
  String json = "{";
  json += "\"state\":\"";
  json += chargingStateToString(state);
  json += "\",\"current\":";
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
