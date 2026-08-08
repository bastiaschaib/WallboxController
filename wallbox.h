#pragma once

#include <Arduino.h>

extern bool charging;
extern int currentAmp;
extern int power;
extern bool online;
extern uint32_t energySincePowerOn; // VAh, resets whenever the wallbox itself resets
extern uint32_t energyTotal;        // VAh, since installation

void wallboxInit();
void wallboxPoll();
void wallboxSetCurrent(int amps);
void wallboxStop();
String wallboxStatusJson();
