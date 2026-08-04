# WallboxController

ESP8266 Wi-Fi wallbox controller — web UI + REST API for EV charging control.

A small responsive web dashboard, served directly from the ESP8266's flash filesystem, that lets you monitor charging status and set the maximum charging current from a phone or PC on the local network. The HTTP API is protected with Basic Auth.

> **Status:** software/UI scaffold. The backend (routes, state, auth) is fully implemented, but it currently operates on simulated charging state — it is **not yet wired to real charging hardware** (no Control Pilot signaling, relay/contactor control, or current sensing). See [Roadmap](#roadmap).

## Features

- Web dashboard (dark/light theme, responsive for mobile and desktop) served from LittleFS
- REST API: live status, set max charging current, stop charging
- HTTP Basic Auth on the page and all state-changing endpoints
- WiFi credentials and login credentials kept out of source control (`secrets.h`, gitignored)
- Server-side clamping of the requested current to a safe configured range

## Project structure

```
WallboxController/
├── WallboxController.ino   # setup()/loop(), wires the modules together
├── wifi.cpp / wifi.h        # WiFi connection
├── webserver.cpp / webserver.h   # HTTP routes, Basic Auth
├── wallbox.cpp / wallbox.h  # charging state + logic
├── config.h                  # charging parameters (min/max amps, voltage)
├── secrets.h.example         # template for credentials — copy to secrets.h
├── secrets.h                 # WiFi + web UI credentials (gitignored, not committed)
└── data/                     # served from LittleFS at runtime
    ├── index.html
    ├── style.css
    └── script.js
```

## Hardware

- ESP8266 board (developed against a NodeMCU 1.0 / ESP-12E module)
- No additional wiring required yet — see [Roadmap](#roadmap) for what's needed to actually control a charger

## Getting started (Arduino IDE)

1. Install the ESP8266 board package (Boards Manager → search "esp8266") and select your board under **Tools → Board**.
2. Copy `secrets.h.example` to `secrets.h` and fill in your WiFi SSID/password and the web UI login (`AUTH_USER` / `AUTH_PASSWORD`).
3. Open `WallboxController.ino` in the Arduino IDE.
4. Upload the sketch (**Upload** button).
5. Upload the `data/` folder to the filesystem:
   - **Arduino IDE 1.8.x**: install the [ESP8266 LittleFS Data Upload](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) plugin (drop the `.jar` into `<sketchbook>/tools/ESP8266LittleFS/tool/`), then **Tools → ESP8266 LittleFS Data Upload**.
   - **Arduino IDE 2.x**: install the [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) plugin, then use its upload command from the Tools menu.
   - Make sure **Tools → Flash Size** reserves space for a filesystem (e.g. `4MB (FS:2MB OTA:~1019KB)`), and close the Serial Monitor before uploading (it needs the port free).
6. Open the Serial Monitor at 115200 baud to confirm the WiFi connects and note the printed IP address.
7. Visit that IP in a browser — you'll be prompted for the `AUTH_USER` / `AUTH_PASSWORD` you set in `secrets.h`.

## API

All endpoints require HTTP Basic Auth.

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | Web dashboard |
| GET | `/status` | JSON: `{ "charging": bool, "current": int, "power": int }` |
| GET | `/setcurrent?amps=<n>` | Set max charging current (clamped to `MIN_AMPS`–`MAX_AMPS` in `config.h`, currently 6–16 A) |
| GET | `/stop` | Stop charging |

## Configuration

- `config.h` — `MIN_AMPS`, `MAX_AMPS`, `VOLTAGE` (used to compute displayed power).
- `secrets.h` (not committed) — `WIFI_SSID`, `WIFI_PASSWORD`, `AUTH_USER`, `AUTH_PASSWORD`.

## Roadmap

Not yet implemented — required for this to control a real charger:

- Control Pilot (CP) PWM signaling and state reading (IEC 61851-1 / J1772 / Type 2)
- Contactor/relay switching gated by a proper charging state machine
- Real current/power measurement (CT clamp or shunt) instead of the current computed placeholder
- RCD/ground-fault and overtemperature monitoring
- WiFi reconnect handling and a watchdog
- Persisted settings across reboots

## License

No license specified yet.
