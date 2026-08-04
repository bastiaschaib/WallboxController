#include <Arduino.h>
#include "wallbox.h"
#include "config.h"

// Test values
bool charging = false;
int currentAmp = 0;
int power = 0;

void wallboxSetCurrent(int amps) {
  currentAmp = constrain(amps, MIN_AMPS, MAX_AMPS);
  charging = true;
  power = currentAmp * VOLTAGE;

  Serial.print("Charging current set: ");
  Serial.print(currentAmp);
  Serial.println(" A");
}

void wallboxStop() {
  charging = false;
  currentAmp = 0;
  power = 0;

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
  json += "}";
  return json;
}
