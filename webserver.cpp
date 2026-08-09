#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "webserver.h"
#include "wallbox.h"
#include "secrets.h"

ESP8266WebServer server(80);

static bool checkAuth() {
  if (!server.authenticate(AUTH_USER, AUTH_PASSWORD)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void webserverSetup() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed - did you upload the data/ folder?");
  }

  // Web page (from data/index.html, auth-protected)
  server.on("/", []() {
    if (!checkAuth()) return;

    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      server.send(500, "text/plain", "index.html not found on LittleFS - upload the data/ folder");
      return;
    }
    server.streamFile(f, "text/html");
    f.close();
  });

  // Static assets (no sensitive data)
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.serveStatic("/wallbox.png", LittleFS, "/wallbox.png");
  server.serveStatic("/manifest.json", LittleFS, "/manifest.json");

  // Status API
  server.on("/status", []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", wallboxStatusJson());
  });

  // Set charging current
  server.on("/setcurrent", []() {
    if (!checkAuth()) return;

    if (server.hasArg("amps")) {
      wallboxSetCurrent(server.arg("amps").toInt());
    }
    server.send(200, "text/plain", "OK");
  });

  // Stop
  server.on("/stop", []() {
    if (!checkAuth()) return;

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
