# WallboxController

ESP8266 Wi-Fi wallbox controller — web UI + REST API for EV charging control, talking Modbus RTU directly to a Heidelberg Energy Control wallbox over a TTL-to-RS485 module wired straight to the ESP8266's GPIOs (no Node-RED, gateway box, or other middleware required).

A small responsive web dashboard, served directly from the ESP8266's flash filesystem, that lets you monitor charging status and set the maximum charging current from a phone or PC on the local network. The HTTP API is protected with Basic Auth.

> **Status:** the web UI/API talk to the real wallbox over Modbus (charging state, measured power, and the current setpoint are live). Control Pilot signaling, relay/contactor control, and current sensing are handled entirely by the wallbox itself — this firmware only reads/writes its Modbus registers. See [Roadmap](#roadmap) for what's still open.

## Features

- Web dashboard (dark/light theme, responsive for mobile and desktop) served from LittleFS
- REST API: live status (from the wallbox's own Modbus registers), set max charging current, stop charging
- HTTP Basic Auth on the page and all state-changing endpoints
- WiFi credentials and login credentials kept out of source control (`secrets.h`, gitignored)
- Server-side clamping of the requested current to a safe configured range
- Non-blocking Modbus RTU polling with a periodic watchdog-safe re-send of the current setpoint

## Project structure

```
WallboxController/
├── WallboxController.ino   # setup()/loop(), wires the modules together
├── wifi.cpp / wifi.h        # WiFi connection
├── webserver.cpp / webserver.h   # HTTP routes, Basic Auth
├── wallbox.cpp / wallbox.h  # Heidelberg register map, charging state + logic
├── modbus.cpp / modbus.h    # Modbus RTU transport wrapper (RS485 serial link, async reads/writes)
├── config.h                  # charging parameters + Modbus RTU pin/timing settings
├── secrets.h.example         # template for credentials — copy to secrets.h
├── secrets.h                 # WiFi + web UI credentials (gitignored, not committed)
└── data/                     # served from LittleFS at runtime
    ├── index.html
    ├── style.css
    └── script.js
```

## Hardware

- ESP8266 board (developed against a NodeMCU 1.0 / ESP-12E module)
- A Heidelberg Energy Control wallbox with its RS485 Modbus interface enabled, slave ID set via its DIP switches (default `1`)
- A TTL-to-RS485 module (e.g. the common MAX485-based breakout: 2-pin `A`/`B` screw terminal, 5-pin header `VCC`/`GND`/`RO`/`RE`/`DE`/`DI`), wired directly to the ESP8266 — **not** a networked Modbus TCP-to-RTU gateway
- **Power the module at 3.3V, not 5V.** The ESP8266's GPIOs are not 5V-tolerant; a 5V-powered module's `RO` output would drive ~5V logic into the ESP8266's RX pin. These modules run fine at 3.3V at the slow 19200 baud used here.
- Bridge the module's `RE` and `DE` pins together with a short wire (this module doesn't auto-sense transmit/receive direction — one GPIO drives both at once)

Wiring, module pin → ESP8266 pin (see `MODBUS_RTU_*` constants in `config.h`):

| Module pin | ESP8266 pin | Purpose |
|---|---|---|
| `VCC` | `3V3` | Power |
| `GND` | `GND` | Common ground |
| `RO` | `D2` (GPIO4) | Module → ESP receive |
| `DI` | `D1` (GPIO5) | ESP → module transmit |
| `RE` + `DE` (bridged) | `D7` (GPIO13) | Direction control |

The module's `A`/`B` screw terminal then goes to the wallbox's RS485 `A`/`B` terminals — this is a separate run from the ESP8266↔module wiring above, and carries no GPIO signals (if there's no communication at all, try swapping `A`/`B`).

The wallbox's RTU line is fixed at **19200 baud, 8 data bits, 1 stop bit, even parity** (confirmed against a working Node-RED setup on the same wallbox) — the firmware's `SoftwareSerial` is already configured for this (`SWSERIAL_8E1`), nothing to change there.

## Getting started (Arduino IDE)

1. Install the ESP8266 board package (Boards Manager → search "esp8266") and select your board under **Tools → Board**.
2. Install the `modbus-esp8266` library (Library Manager → search "modbus-esp8266", by Andre Sarmento Barbosa / Alexander Emelianov).
3. Copy `secrets.h.example` to `secrets.h` and fill in your WiFi SSID/password and the web UI login (`AUTH_USER` / `AUTH_PASSWORD`).
4. Wire the TTL-to-RS485 module per the [Hardware](#hardware) table above. If you use different GPIOs, update `MODBUS_RTU_RX_PIN` / `MODBUS_RTU_TX_PIN` / `MODBUS_RTU_DE_RE_PIN` in `config.h` to match.
5. Open `WallboxController.ino` in the Arduino IDE.
6. Upload the sketch (**Upload** button).
7. Upload the `data/` folder to the filesystem:
   - **Arduino IDE 1.8.x**: install the [ESP8266 LittleFS Data Upload](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) plugin (drop the `.jar` into `<sketchbook>/tools/ESP8266LittleFS/tool/`), then **Tools → ESP8266 LittleFS Data Upload**.
   - **Arduino IDE 2.x**: install the [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) plugin, then use its upload command from the Tools menu.
   - Make sure **Tools → Flash Size** reserves space for a filesystem (e.g. `4MB (FS:2MB OTA:~1019KB)`), and close the Serial Monitor before uploading (it needs the port free).
8. Open the Serial Monitor at 115200 baud to confirm WiFi connects, note the printed IP address, and watch for Modbus read logging.
9. Visit that IP in a browser — you'll be prompted for the `AUTH_USER` / `AUTH_PASSWORD` you set in `secrets.h`.

## API

All endpoints require HTTP Basic Auth.

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | Web dashboard |
| GET | `/status` | JSON: `{ "charging": bool, "current": int, "power": int, "energySincePowerOn": int, "energyTotal": int, "online": bool }` — `current` is the commanded setpoint, `power` is the wallbox's own measured value (register 14), the two energy fields are VAh (apparent energy, not true Wh — see [Modbus details](#modbus-details)) from registers 15/16 and 17/18, `online` reflects Modbus reachability |
| GET | `/setcurrent?amps=<n>` | Set max charging current (clamped to `MIN_AMPS`–`MAX_AMPS` in `config.h`, currently 6–16 A); writes the wallbox's max-current register |
| GET | `/stop` | Stop charging (writes max-current register to 0; the connector stays unlocked/ready) |

## Configuration

- `config.h` — `MIN_AMPS`, `MAX_AMPS`, `VOLTAGE`; and the Modbus settings: `MODBUS_RTU_RX_PIN`, `MODBUS_RTU_TX_PIN`, `MODBUS_RTU_DE_RE_PIN`, `MODBUS_BAUD`, `MODBUS_SLAVE_ID`, `MODBUS_POLL_INTERVAL_MS`, `MODBUS_WATCHDOG_REFRESH_MS`.
- `secrets.h` (not committed) — `WIFI_SSID`, `WIFI_PASSWORD`, `AUTH_USER`, `AUTH_PASSWORD`.

## Modbus details

`wallbox.cpp` talks to five Heidelberg Energy Control registers (see the [official register table](https://www.amperfied.de/wp-content/uploads/2022/06/ModBus-Register-Tabelle.pdf) for the full map):

- **Input register 5** (charging state) — polled every `MODBUS_POLL_INTERVAL_MS`; only state `7` (C2: vehicle plugged, requesting, wallbox allows) counts as `charging`.
- **Input register 14** (power, VA) — polled alongside register 5.
- **Input registers 15+16** (energy since power-on, VAh) and **17+18** (energy since installation, VAh) — each a 32-bit value split across two registers (high word first), read via `modbusReadInputReg32()`. These are apparent energy (VAh), not true energy (Wh) — consistent with register 14 also being apparent power (VA), not real power — so don't use them for anything billing-related. Note register 15/16 resets whenever the wallbox itself resets, so it tracks the current session at best, not a true lifetime total (use 17/18 for that).
- **Holding register 261** (max current, 0.1 A steps) — written immediately on `/setcurrent` and `/stop`, and re-written every `MODBUS_WATCHDOG_REFRESH_MS` regardless of user action. The wallbox has its own Modbus watchdog (register 257, default 15000 ms) that falls back to a configured Failsafe Current if no holding register is written in time — the periodic re-send keeps charging stable without requiring the watchdog timeout to be changed.

`modbus.cpp` is a thin async transport wrapper around the [`modbus-esp8266`](https://github.com/emelianov/modbus-esp8266) library's `ModbusRTU` master (over a `SoftwareSerial` link, so the hardware UART stays free for the USB debug console); it is not Heidelberg-specific and could be reused for other Modbus RTU targets on the same bus.

## Roadmap

Not yet implemented:

- Per-phase current, voltage, and PCB temperature telemetry (registers 6-13 are documented but not currently polled — see [Modbus details](#modbus-details))
- Remote lock control (register 259) as an alternative/addition to the current 0A "stop"
- WiFi reconnect handling
- Persisted settings across reboots

Out of scope for this firmware (handled entirely by the wallbox's own hardware): Control Pilot signaling, contactor switching, current/power measurement, RCD/ground-fault and overtemperature protection.

## License

No license specified yet.
