#include <Arduino.h>
#include "wifi.h"
#include "webserver.h"
#include "wallbox.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP started");

  wifiConnect();
  wallboxInit();
  webserverSetup();
}

void loop() {
  wallboxPoll();
  webserverHandle();
}
