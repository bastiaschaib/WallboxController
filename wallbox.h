#pragma once

#include <Arduino.h>

enum class ChargingState { Disconnected, Connected, Charging, Derating, Error };

extern ChargingState state;
extern int currentAmp;
extern int power;
extern bool online;
extern uint32_t energySincePowerOn; // Wh, resets whenever the wallbox itself resets
extern uint32_t energyTotal;        // Wh, since installation

void wallboxInit();
void wallboxPoll();
void wallboxSetCurrent(int amps);
void wallboxStop();
String wallboxStatusJson();
