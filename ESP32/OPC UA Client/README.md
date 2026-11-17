# ESP32 OPC UA Client

A complete, self-contained **OPC UA client** for the classic **ESP32
(ESP32 DevKit)**, built with **PlatformIO only**. The board connects to any
OPC UA server (PLC, SCADA, another ESP32 running the companion
*ESP32 OPC UA Server* firmware), polls the tags you configure, and serves a
built-in web interface for setup and live monitoring — no recompiling to
change Wi-Fi, the server URL or the tag list.

The firmware uses the [open62541](https://www.open62541.org/) reference
implementation of OPC UA (IEC 62541), vendored as an amalgamated single
file and patched for ESP32 — see
[lib/open62541/README.md](lib/open62541/README.md) for the exact changes.

---

## Features

- **OPC UA client** (binary protocol), security policy `None`
- Connects to **any OPC UA server** — endpoint URL, optional username /
  password and poll interval configured in the web UI
- **8 configurable tag slots**: numeric ids (`ns=0;i=2258`) or string ids
  (`ns=1;s=Temperature`), each with a display name
- **Batched reads**: one Read service call per interval (all tags in a
  single request), 250 ms … 1 h configurable
- **Reconnect with backoff**: connection failures retry every 10 s and the
  status is shown live
- Value decoding for Boolean, all integer widths, Float/Double, String,
  DateTime and array types; status codes per tag
- **Web configuration UI**: Wi-Fi, static IP, server connection, tag table
- **Live tag page** with per-tag value, status and source timestamp
- **Setup access point** `esp32-opcua-client-XXXX` when Wi-Fi is not
  configured or unreachable; UI at <http://192.168.4.1>
- Settings stored in NVS (flash), survive power cycles, factory reset
- mDNS: `http://esp32-opcua-client.local`

## Hardware

| Item | Value |
| --- | --- |
| Board | Generic ESP32 DevKit (WROOM-32), 4 MB flash, 520 KB RAM |
| Transport | OPC UA binary over Wi-Fi TCP |
| Target port | typically 4840 (whatever the server uses) |
| Web UI | port 80 |

### Why the classic ESP32?

The open62541 client stack plus the Wi-Fi stack and web server need the
520 KB SRAM of the classic ESP32; ESP32-C3 boards work but are tight, and
the ESP8266 / WEMOS D1 Mini (80 KB SRAM) cannot run OPC UA at all.

## Getting started

These steps take a brand-new board from bare metal to reading tags. If you
have already flashed the firmware once, skip to step 5.

### 1. What you need

- An ESP32 DevKit board and a USB **data** cable (not charge-only)
- Python 3.8 or newer on Windows, macOS or Linux
- A 2.4 GHz Wi-Fi network (name and password)
- An OPC UA server to talk to (see *Testing options* below)
- A computer on the same network to open the web UI

### 2. Get the code

Clone or download this repository, then open a terminal in the
`ESP32/OPC UA Client` folder.

### 3. Install VS Code and the PlatformIO IDE extension

1. Install [Visual Studio Code](https://code.visualstudio.com/) if you do
   not have it yet, and [Python 3.8+](https://www.python.org/downloads/)
   (on Windows, tick **Add python.exe to PATH** in the Python installer).
2. In VS Code open the **Extensions** view (`Ctrl+Shift+X`), search for
   **PlatformIO IDE** (publisher: `PlatformIO`) and install it.
3. **File → Open Folder…** and select the `OPC UA Client` folder.

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
2. Join the Wi-Fi network `esp32-opcua-client-XXXX` with the password
   `opcua1234` (configurable in `src/opcua_config.h`).
3. Open <http://192.168.4.1>.
4. **Network** tab: enter your SSID and password, optionally a static IP,
   then **Save and restart**.
5. **Connection** tab: enter the server endpoint URL, e.g.
   `opc.tcp://192.168.1.50:4840`, and **Save and reconnect**.
6. **Tags** tab: enable the slots you want and fill in the node ids.

The setup access point is also started automatically when the stored
network cannot be reached within 25 seconds of boot.

### 6. Verify it works

- The **Status** tab shows the client state (`Connected`), successful /
  failed read counters and the last error, if any.
- The **Tags** tab shows the live value of every enabled slot, refreshed
  every 2 s.

## Testing options (pick any)

| Option | What you need |
| --- | --- |
| **Companion firmware** | Flash the sibling `ESP32/OPC UA Server` project onto a second ESP32 and point the client at it. The default tag slots (`ns=1;s=Temperature`, `ns=1;s=Setpoint`, `ns=0;i=2258`) match it out of the box. |
| **Reference simulator** | Run the bundled `tools/opcua_sim_server.py` on any computer (Python only, no dependencies): `python tools/opcua_sim_server.py --port 4840`, then point the client at `opc.tcp://<pc-ip>:4840`. |
| **Real OPC UA server** | Any PLC/SCADA with OPC UA: set the endpoint URL and the node ids your server publishes (find them in UaExpert). |

### The reference simulator

`tools/opcua_sim_server.py` implements the client side of the protocol in
pure Python: HEL/ACK, OpenSecureChannel (policy None), GetEndpoints,
CreateSession, ActivateSession (anonymous or username), Read and Write. It
simulates `DeviceName`, `Temperature` (sine), `Uptime`, `Sine`,
`Setpoint` (writable), `RelayOutput` (writable) and the standard server
clock `ns=0;i=2258`. It is a bench tool — not a hardened server.

## Web interface

| Endpoint | Purpose |
| --- | --- |
| `GET /` | configuration and monitoring UI |
| `GET /api/status` | Wi-Fi, IP, uptime, heap, client state, counters |
| `GET /api/settings` | stored configuration as JSON |
| `POST /api/settings` | update Wi-Fi configuration (form encoded) |
| `POST /api/connection` | update server URL / credentials / poll interval and reconnect |
| `GET /api/tags` | tag slots with live values |
| `POST /api/tags` | save the tag slot table (form encoded) |
| `POST /api/connect` | (re)connect to the configured server |
| `POST /api/disconnect` | close the session |
| `POST /api/reboot` | restart |
| `POST /api/factory-reset` | erase settings and restart into setup mode |

The UI works entirely offline; the page is embedded in the firmware flash.

## Client behaviour notes

- Reads are **polling** (no subscriptions): every poll interval the
  firmware sends one batched Read request for all enabled tags. This keeps
  memory predictable and suits SCADA-style monitoring.
- Security policy is **None** only. Without message encryption a
  username/password travels unencrypted inside ActivateSession — use it
  only on trusted networks.
- The service calls run in the Arduino loop, so a slow server briefly
  delays the web UI (bounded by the 3 s client timeout). Poll intervals
  below 250 ms are clamped.
- Connection-level failures (closed channel, timeout, communication error)
  trigger a 10 s backoff and automatic reconnect; `Connect / reconnect` in
  the UI forces an immediate attempt.

## Project layout

```
platformio.ini          PlatformIO environment (esp32dev, huge_app)
lib/open62541/          open62541 v1.4.22 amalgamation, patched for ESP32
src/
  pio_entry.cpp         setup()/loop()
  app.cpp               Wi-Fi, setup AP, mDNS, client lifecycle
  esp32_compat.c        libc shims (gethostname, signal) ESP-IDF lacks
  opcua_config.h        compile-time defaults (first boot only)
  opcua_settings.h/.cpp persistent settings + tag slots in NVS
  opcua_client.h/.cpp   OPC UA client, session state machine, batch reads
  opcua_web.h/.cpp      HTTP API and configuration handlers
  web_page.h            embedded single-page UI
tools/opcua_sim_server.py  dependency-free OPC UA reference simulator
```

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No setup AP | Serial log at 115200 baud; board powered and flashed |
| Web UI unreachable | Correct IP on the Status tab, port 80 free, same subnet |
| State Error on connect | Server URL correct (`opc.tcp://host:port`)? Server reachable on port? |
| Tag shows no data | Node id correct? Namespace index right? Try `ns=0;i=2258` as a sanity check |
| Reads count up but values stale | Server caching? Check the source timestamps shown per tag |
| Web UI briefly unresponsive | The loop blocks during OPC UA calls; expected with slow/unreachable servers |
