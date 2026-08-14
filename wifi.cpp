#include <ESP8266WiFi.h>
#include "wifi.h"
#include "secrets.h"

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  // Modem sleep power-cycles the radio and introduces interrupt-latency spikes
  // long enough to corrupt bytes on the bit-banged SoftwareSerial RS485 link,
  // which otherwise silently drops nearly every Modbus response.
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
