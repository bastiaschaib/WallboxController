#pragma once

#include <Arduino.h>

extern bool charging;
extern int currentAmp;
extern int power;
extern bool online;

void wallboxInit();
void wallboxPoll();
void wallboxSetCurrent(int amps);
void wallboxStop();
String wallboxStatusJson();
