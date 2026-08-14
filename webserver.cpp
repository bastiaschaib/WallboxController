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

  // Static assets (no sensitive data). CSS/JS are still actively changing
  // and there's no cache-busting (no build step, no versioned filenames),
  // so caching them client-side risks a phone silently running a stale
  // script.js after an update - not worth the saved round trip on a local
  // network. The icon/manifest barely change, so those stay cached.
  server.serveStatic("/style.css", LittleFS, "/style.css", "no-store");
  server.serveStatic("/script.js", LittleFS, "/script.js", "no-store");
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
