#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "webserver.h"
#include "wallbox.h"

ESP8266WebServer server(80);

void webserverSetup() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed - did you upload the data/ folder?");
  }

  // Web page (from data/index.html)
  server.on("/", []() {
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      server.send(500, "text/plain", "index.html not found on LittleFS - upload the data/ folder");
      return;
    }
    server.streamFile(f, "text/html");
    f.close();
  });

  // Static assets (no sensitive data). Cached client-side so a phone
  // launching the home-screen app doesn't re-fetch everything from the
  // ESP8266's slow WiFi/LittleFS on every open. CSS/JS get a shorter TTL
  // since they're still actively changing; the icon/manifest barely change.
  server.serveStatic("/style.css", LittleFS, "/style.css", "public, max-age=86400");
  server.serveStatic("/script.js", LittleFS, "/script.js", "public, max-age=86400");
  server.serveStatic("/wallbox.png", LittleFS, "/wallbox.png", "public, max-age=604800");
  server.serveStatic("/manifest.json", LittleFS, "/manifest.json", "public, max-age=604800");

  // Status API
  server.on("/status", []() {
    server.send(200, "application/json", wallboxStatusJson());
  });

  // Set charging current
  server.on("/setcurrent", []() {
    if (server.hasArg("amps")) {
      wallboxSetCurrent(server.arg("amps").toInt());
    }
    server.send(200, "text/plain", "OK");
  });

  // Stop
  server.on("/stop", []() {
    wallboxStop();
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Web server started");
}

void webserverHandle() {
  server.handleClient();
}
