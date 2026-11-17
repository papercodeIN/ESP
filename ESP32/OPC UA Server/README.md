# ESP32 OPC UA Server

A complete, self-contained **OPC UA server** for the classic **ESP32
(ESP32 DevKit)**, built with **PlatformIO only**. The board exposes its
process picture on `opc.tcp://<ip>:4840`, appears in UaExpert, Prosys, TIA
Portal and any other OPC UA client, and ships with a built-in web interface
for setup and live monitoring — no recompiling to change Wi-Fi or the port.

The firmware uses the [open62541](https://www.open62541.org/) reference
implementation of OPC UA (IEC 62541), vendored as an amalgamated single
file and patched for ESP32 — see
[lib/open62541/README.md](lib/open62541/README.md) for the exact changes.

---

## Features

- **OPC UA server** (binary protocol) on TCP 4840, security policy `None`
- **Namespace 1** process tags with **string identifiers** (`ns=1;s=Temperature`)
- Read-only tags: device name, chip temperature, uptime, free heap,
  Wi-Fi RSSI, simulated sine 0…100 %
- Writable tags: **Setpoint** (persisted in NVS), **RelayOutput** (GPIO2)
- `RebootDevice` **method** callable from any OPC UA client
- Minimal namespace 0, sized for ESP32 RAM (max 4 sessions / 8 channels)
- **Web configuration UI**: Wi-Fi, static IP, device name, TCP port,
  endpoint host override
- **Live tag page**: read every tag, write Setpoint / RelayOutput from the
  browser — writes use the same state the OPC UA clients see
- **Setup access point**: if no Wi-Fi is configured or the network cannot
  be reached, the device opens `esp32-opcua-XXXX` and serves the UI at
  <http://192.168.4.1>
- Settings stored in NVS (flash), survive power cycles, factory reset
- mDNS: `http://esp32-opcua.local` and an advertised `opcua-tcp` service

## Hardware

| Item | Value |
| --- | --- |
| Board | Generic ESP32 DevKit (WROOM-32), 4 MB flash, 520 KB RAM |
| Transport | OPC UA binary over Wi-Fi TCP |
| OPC UA port | 4840 (configurable) |
| Relay output | GPIO2 (onboard LED on most DevKits) |
| Button input | GPIO0 (BOOT button, active low) |

### Why the classic ESP32?

open62541's server stack needs roughly 100–150 KB of heap alongside the
Wi-Fi stack and the web server. The classic ESP32 (520 KB SRAM, dual-core)
has the headroom; the ESP32-C3 boards (400 KB, single core) work but are
tight, and the ESP8266 / WEMOS D1 Mini (80 KB) cannot run OPC UA at all.

## Getting started

These steps take a brand-new board from bare metal to a visible OPC UA
server. If you have already flashed the firmware once, skip to step 5.

### 1. What you need

- An ESP32 DevKit board and a USB **data** cable (not charge-only)
- Python 3.8 or newer on Windows, macOS or Linux
- A 2.4 GHz Wi-Fi network (name and password) — the ESP32 does not support
  5 GHz
- A computer on the same network to open the web UI and run OPC UA clients

### 2. Get the code

Clone or download this repository, then open a terminal in the
`ESP32/OPC UA Server` folder.

### 3. Install VS Code and the PlatformIO IDE extension

1. Install [Visual Studio Code](https://code.visualstudio.com/) if you do
   not have it yet, and [Python 3.8+](https://www.python.org/downloads/)
   (on Windows, tick **Add python.exe to PATH** in the Python installer).
2. In VS Code open the **Extensions** view (`Ctrl+Shift+X`), search for
   **PlatformIO IDE** (publisher: `PlatformIO`) and install it.
3. **File → Open Folder…** and select the `OPC UA Server` folder.
   PlatformIO detects `platformio.ini` and imports the project
   automatically.

### 4. Build and flash in VS Code

The PlatformIO sidebar lists **PROJECT TASKS → env:esp32dev**:

| Task | What it does |
| --- | --- |
| **Build** | compiles the firmware — works without a board |
| **Upload** | flashes the board over USB |
| **Monitor** | live serial log; 115200 baud is taken from `platformio.ini` |

Notes:

- The **first build** compiles the vendored open62541 stack (a single
  ~14 MB C file) — allow a few minutes; later builds skip it unless the
  library changes. The project pins `platform = espressif32@6.12.0`
  (Arduino core 2.0.17) so builds are reproducible.
- The firmware uses the `huge_app` partition table (3 MB app) because the
  OPC UA stack exceeds the default 1.2 MB app slot.
- If **Upload** cannot find a port, hold the **BOOT** button while plugging
  in the USB cable and start Upload again.

#### Command-line alternative

```bash
python -m platformio run                        # build (works without a board)
python -m platformio run -t upload              # flash over USB
python -m platformio device monitor -b 115200   # live serial log
```

### 5. First boot: configure Wi-Fi from the setup access point

1. Power the board. With no Wi-Fi stored it starts a setup access point.
2. Join the Wi-Fi network `esp32-opcua-XXXX` with the password
   `opcua1234` (both configurable in `src/opcua_config.h`).
3. Open <http://192.168.4.1>.
4. **Network** tab: enter your SSID and password, optionally a static IP,
   then **Save and restart**.
5. After the restart the device joins your network. The **Status** tab
   shows the OPC UA endpoint URL, or use `http://esp32-opcua.local`
   (mDNS).
6. **OPC UA** tab: set the device name and TCP port if needed.

The setup access point is also started automatically when the stored
network cannot be reached within 25 seconds of boot, so a wrong password
never bricks the device. Even in setup-AP mode the OPC UA server runs on
`opc.tcp://192.168.4.1:4840`, so you can exercise it without a router.

### 6. Verify it works

- **UaExpert / Prosys / TIA Portal**: add the endpoint shown on the
  Status tab (`opc.tcp://<ip>:4840`), security policy **None**, anonymous
  login, and browse namespace 1.
- **No OPC UA tools installed?** Run the bundled smoke test from the same
  computer (Python only, no dependencies):
  `python tools/opcua_test.py --host <device-ip>` — it performs the full
  client handshake and reads/writes every tag.
- The web **Tags** tab shows live values and lets you write Setpoint /
  RelayOutput without any OPC UA client at all.

## Web interface

| Endpoint | Purpose |
| --- | --- |
| `GET /` | configuration and monitoring UI |
| `GET /api/status` | Wi-Fi, IP, RSSI, uptime, heap, endpoint URL |
| `GET /api/settings` | stored configuration as JSON |
| `POST /api/settings` | update configuration (form encoded) |
| `GET /api/tags` | all tags with live values |
| `POST /api/tags/write` | write a writable tag |
| `POST /api/reboot` | restart |
| `POST /api/factory-reset` | erase settings and restart into setup mode |

The UI works entirely offline; the page is embedded in the firmware flash.

## OPC UA address space

All tags live in **namespace 1** with **string identifiers**, organized
under the Objects folder. Node ids are therefore e.g. `ns=1;s=Temperature`.

| Node id (ns=1;s=…) | Type | Access | Description |
| --- | --- | --- | --- |
| `DeviceName` | String | read | configured device name |
| `Temperature` | Double | read | ESP32 chip temperature (°C) |
| `Uptime` | Double | read | uptime (s) |
| `FreeHeap` | Double | read | free heap (bytes) |
| `WiFiRSSI` | Double | read | Wi-Fi signal (dBm) |
| `Sine` | Double | read | generated sine 0…100 % (60 s period) |
| `Setpoint` | Double | **write** | persisted in NVS across reboots |
| `RelayOutput` | Boolean | **write** | GPIO2 / onboard LED |
| `ButtonInput` | Boolean | read | BOOT button (GPIO0, active low) |
| `RebootDevice` | Method | call | restarts the device |

Namespace 0 is open62541's **minimal** variant — enough for clients to
navigate Objects and read the standard `Server` clock
(`ns=0;i=2258`). Type definitions exist for all builtin types, so clients
can decode every tag.

## Configuration notes

- `src/opcua_config.h` seeds the defaults on first boot and after a
  factory reset. After that, everything is configured in the web UI.
- **Endpoint host override** (OPC UA tab): by default the endpoint URL in
  GetEndpoints advertises the device IP (`opc.tcp://<ip>:4840`). If your
  client environment expects a host name, set it there (e.g.
  `esp32-opcua.local`).
- Security policy is **None** only — no certificates. OPC UA
  `Basic256Sha256` requires mbedTLS + certificate provisioning, which the
  vendored build has disabled to fit the ESP32; treat the link like you
  would Modbus TCP (trusted / isolated network).
- Adding tags: extend `k_tags[]` and the value functions in
  `src/opcua_server.cpp`.

## Command-line smoke test

`tools/opcua_test.py` needs only the Python standard library:

```bash
python tools/opcua_test.py --host 192.168.1.50
python tools/opcua_test.py --host 192.168.1.50 --skip-write
```

It checks HEL/ACK, OpenSecureChannel (policy None), GetEndpoints,
CreateSession, anonymous ActivateSession, reads every tag plus the
standard server clock, writes and reads back `Setpoint`, and closes the
session cleanly.

## Project layout

```
platformio.ini          PlatformIO environment (esp32dev, huge_app)
lib/open62541/          open62541 v1.4.22 amalgamation, patched for ESP32
src/
  pio_entry.cpp         setup()/loop()
  app.cpp               Wi-Fi, setup AP, mDNS, server lifecycle
  esp32_compat.c        libc shims (gethostname, signal) ESP-IDF lacks
  opcua_config.h        compile-time defaults (first boot only)
  opcua_settings.h/.cpp persistent settings in NVS
  opcua_server.h/.cpp   OPC UA server, tags, data source callbacks
  opcua_web.h/.cpp      HTTP API and configuration handlers
  web_page.h            embedded single-page UI
tools/opcua_test.py     dependency-free OPC UA binary test client
```

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No setup AP | Serial log at 115200 baud; board powered and flashed |
| Web UI unreachable | Correct IP on the Status tab, port 80 free, same subnet |
| Client cannot connect | Security policy must be `None`; same subnet; port 4840 not blocked |
| Tags missing in browse | Only namespace 1 has process tags; minimal ns0 has no full type tree |
| Write rejected | Tag not writable (only `Setpoint` / `RelayOutput` are) |
| Out-of-memory in log | Lower `OPC_MAX_SESSIONS` or `OPC_TCP_BUFFER_SIZE` in `src/opcua_config.h` |
