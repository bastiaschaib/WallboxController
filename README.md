# WallboxController

ESP8266 Wi-Fi controller for a Heidelberg Energy Control wallbox — a web dashboard + REST API talking Modbus RTU directly over a TTL-to-RS485 module wired to the ESP8266 (no gateway or middleware needed).

> **Status:** live Modbus link — charging state, measured power, and current setpoint work. Control Pilot signaling, contactor switching, and current sensing are handled by the wallbox itself. See [Roadmap](#roadmap).

## Features

- Responsive dark/light web dashboard, served from LittleFS, installable as a home-screen app (icon + manifest)
- EN/DE language toggle (client-side, persisted in `localStorage`)
- REST API: live status, set max charging current, stop charging
- WiFi credentials kept out of source control (`secrets.h`, gitignored)
- Server-side clamping of requested current to a safe range
- Modbus polling serialized into a strict request/response chain (the RTU master only ever handles one transaction at a time), with a watchdog that resets the transport if nothing succeeds for a while
- WiFi modem sleep disabled — otherwise it corrupts bytes on the bit-banged RS485 link

## Project structure

```
WallboxController/
├── WallboxController.ino   # setup()/loop(), wires the modules together
├── wifi.cpp / wifi.h       # WiFi connection
├── webserver.cpp / webserver.h  # HTTP routes
├── wallbox.cpp / wallbox.h # Heidelberg register map, charging state + poll chain
├── modbus.cpp / modbus.h   # Modbus RTU transport wrapper (async reads/writes, reset)
├── config.h                # charging parameters + Modbus pin/timing settings
├── secrets.h.example       # template for credentials — copy to secrets.h
└── data/                   # served from LittleFS at runtime
    ├── index.html
    ├── style.css
    ├── script.js
    ├── manifest.json       # home-screen app metadata
    └── wallbox.png         # home-screen icon
```

## Hardware

- ESP8266 board (developed against a NodeMCU 1.0 / ESP-12E module)
- Heidelberg Energy Control wallbox with RS485 Modbus enabled, slave ID via DIP switches (default `1`)
- TTL-to-RS485 module (e.g. a MAX485 breakout), wired **directly** to the ESP8266 — not a networked Modbus TCP-to-RTU gateway
- **Power the module at 3.3V, not 5V** — the ESP8266's GPIOs aren't 5V-tolerant
- Bridge the module's `RE` and `DE` pins together (this module needs one GPIO to drive both)

| Module pin | ESP8266 pin | Purpose |
|---|---|---|
| `VCC` | `3V3` | Power |
| `GND` | `GND` | Common ground |
| `RO` | `D2` (GPIO4) | Module → ESP receive |
| `DI` | `D1` (GPIO5) | ESP → module transmit |
| `RE` + `DE` (bridged) | `D7` (GPIO13) | Direction control |

The module's `A`/`B` terminal then goes to the wallbox's RS485 `A`/`B` (a separate run, no GPIO signals — swap `A`/`B` if there's no communication). Link is fixed at **19200 baud, 8E1**, already configured in `modbus.cpp` (`SWSERIAL_8E1`).

## Getting started (Arduino IDE)

1. Install the ESP8266 board package (Boards Manager → "esp8266") and select your board under **Tools → Board**.
2. Install the `modbus-esp8266` library (Library Manager → "modbus-esp8266").
3. Copy `secrets.h.example` to `secrets.h` and fill in your WiFi credentials.
4. Wire the RS485 module per the table above; adjust `MODBUS_RTU_*` pins in `config.h` if you use different GPIOs.
5. Open `WallboxController.ino`, upload the sketch.
6. Upload the `data/` folder to the filesystem:
   - **IDE 1.8.x**: [ESP8266 LittleFS Data Upload](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) plugin → **Tools → ESP8266 LittleFS Data Upload**.
   - **IDE 2.x**: [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) plugin, run via Tools menu.
   - Reserve filesystem space under **Tools → Flash Size** (e.g. `4MB (FS:2MB OTA:~1019KB)`), and close the Serial Monitor before uploading.
7. Open the Serial Monitor (115200 baud) to confirm WiFi connects and note the IP address.
8. Visit that IP in a browser.

## API

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | Web dashboard |
| GET | `/status` | JSON status: `state` (`disconnected`\|`connected`\|`charging`\|`derating`\|`error`), `current` (setpoint), `power` (W), `energySincePowerOn`, `energyTotal` (Wh, see [Modbus details](#modbus-details)), `online` |
| GET | `/setcurrent?amps=<n>` | Set max charging current (clamped to `MIN_AMPS`–`MAX_AMPS` in `config.h`) |
| GET | `/stop` | Stop charging (writes max-current to 0; connector stays unlocked/ready) |

## Configuration

- `config.h` — `MIN_AMPS`, `MAX_AMPS`, and `MODBUS_*` pin/baud/slave/timing settings.
- `secrets.h` (not committed) — `WIFI_SSID`, `WIFI_PASSWORD`.

## Modbus details

`wallbox.cpp` runs a strict poll chain (state → power → session energy → total energy → setpoint re-write, one request at a time) over five Heidelberg registers (see the [official register table](https://www.amperfied.de/wp-content/uploads/2022/06/ModBus-Register-Tabelle.pdf)): input register 5 (charging state), 14 (power, reported as VA), 15+16 and 17+18 (session/lifetime energy, reported as VAh), and holding register 261 (max current, 0.1 A steps), re-written every `MODBUS_WATCHDOG_REFRESH_MS` to satisfy the wallbox's own Modbus watchdog (register 257). The wallbox reports apparent power/energy (VA/VAh); the firmware exposes these as W/Wh, assuming a power factor close to 1 (true for EV charging) — not precise enough for billing. If no transaction succeeds for `MODBUS_STUCK_RESET_MS`, the transport is torn down and reinitialized to clear a wedged link.

Register 5's raw value follows the IEC 61851 car state (A = no vehicle, B = vehicle plugged, C = vehicle plugged and requesting) plus a digit for whether the wallbox is currently authorizing current flow (1 = no, 2 = yes) — so `C2` (raw `7`) is the only value where current actually flows. The firmware collapses this into the `state` field returned by `/status`: `2`/`3` (A1/A2) → `disconnected`, `4`/`5`/`6` (B1/B2/C1) → `connected`, `7` (C2) → `charging`, `8` → `derating`, anything else (E/F/comms error) → `error`.

`modbus.cpp` is a thin async wrapper around [`modbus-esp8266`](https://github.com/emelianov/modbus-esp8266)'s `ModbusRTU` master (over `SoftwareSerial`, keeping the hardware UART free for USB debug); not Heidelberg-specific.

## Roadmap

Not yet implemented: per-phase current/voltage/temperature telemetry (registers 6-13), remote lock control (register 259), WiFi reconnect handling, persisted settings across reboots.

Out of scope (handled by the wallbox's own hardware): Control Pilot signaling, contactor switching, current/power measurement, RCD/overtemperature protection.

## License

No license specified yet.
