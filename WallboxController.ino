#include <Arduino.h>
#include "wifi.h"
#include "webserver.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP started");

  wifiConnect();
  webserverSetup();
}

void loop() {
  webserverHandle();
}
