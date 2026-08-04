#pragma once

#include <Arduino.h>

extern bool charging;
extern int currentAmp;
extern int power;

void wallboxSetCurrent(int amps);
void wallboxStop();
String wallboxStatusJson();
