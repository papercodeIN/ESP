# BACnet/IP Device — Seeed XIAO ESP32C3

A complete, self-contained BACnet/IP device for the **Seeed Studio XIAO
ESP32C3**, built with **PlatformIO only**. It speaks standard BACnet/IP on
UDP 47808, so it appears in YABE, Android BACnet apps and any other BACnet
client, and it ships with a built-in web interface for setup and live
monitoring — no recompiling to change Wi-Fi, the device instance, GPIO
points or memory points.

---

## Features

- **BACnet/IP (Annex J)** with BVLC Original-Unicast and Original-Broadcast
  NPDUs, including the standard **global-broadcast discovery** (DNET 0xFFFF),
  so ordinary clients find the device immediately
- **Who-Is / I-Am** device discovery (with range filtering; the I-Am is sent
  both as a broadcast and unicast back to whoever asked)
- **ReadProperty**, **ReadPropertyMultiple**, **WriteProperty**
- **SubscribeCOV** — clients subscribe once and are notified on every value
  change (plus an immediate notification with the current value on
  subscription); subscriptions are bounded (8 maximum) with ownership and
  lifetime tracking
- Full **16-level priority arrays** on commandable points
- Device, Analog Input/Value, Binary Input/Output/Value objects
- **Configurable GPIO points**: six spare header pins can each be enabled in
  the web UI as a Binary Input or Binary Output with its own instance, name
  and active-low/active-high polarity
- **Configurable software memory points**: up to four Binary Value / Analog
  Value points that exist only in software — optional persistence, min/max
  limits and COV increment
- **Web configuration UI**: Wi-Fi, static IP, device instance, name,
  location, vendor data, ports, setup-AP password
- **Live points page**: read every point, write commandable points, release
  individual priorities
- **Diagnostics counters**: resets, watchdog, BACnet packets and COV
  activity, readable from the web UI and `GET /api/diagnostics`
- **Hardened web API**: authenticated administration, size/argument limits,
  strict validation of every configuration field, and read-only points that
  reject writes consistently over both BACnet and HTTP
- **Setup access point**: if no Wi-Fi is configured or the stored network
  cannot be reached, the device opens a `bacnet-...-XXXX` setup access point
  so you can reach the UI at <http://192.168.4.1>. Configure an AP password
  before deployment; password-protected installations require HTTP Basic auth
  (`admin` plus the configured AP password)
- Settings stored in NVS (flash), survive power cycles, factory reset button
- mDNS host name, e.g. <http://bacnet-xiao-c3.local>
- No cloud, no external assets, no dynamic allocation in the BACnet path

## Hardware

| Item | Value |
| --- | --- |
| Board | Seeed Studio XIAO ESP32C3 (4 MB flash, 320 KB RAM) |
| BACnet transport | BACnet/IP over Wi-Fi (2.4 GHz only) |
| BACnet UDP port | 47808 (0xBAC0), configurable |
| Built-in output | BO 0 → D0 / GPIO2 |
| Built-in input | BI 1 → BOOT button / GPIO9 (active low) |
| Configurable GPIO rows | D1/GPIO3, D2/GPIO4, D3/GPIO5, D4/GPIO6, D5/GPIO7, D10/GPIO10 |

## Getting started

These steps take a brand-new board from bare metal to a visible BACnet
device. If you have already flashed the firmware once, skip to step 5.

### 1. What you need

- A Seeed Studio XIAO ESP32C3 board and a USB-C **data** cable (not
  charge-only)
- Python 3.8 or newer on Windows, macOS or Linux
- A 2.4 GHz Wi-Fi network (name and password) — the ESP32-C3 does not
  support 5 GHz
- A computer on the same network to open the web UI and run BACnet clients

### 2. Get the code

Clone or download this repository, then open a terminal in the
`BACnet IP Device` folder.

### 3. Install VS Code and the PlatformIO IDE extension

1. Install [Visual Studio Code](https://code.visualstudio.com/) if you do
   not have it yet, and [Python 3.8+](https://www.python.org/downloads/)
   (on Windows, tick **Add python.exe to PATH** in the Python installer).
2. In VS Code open the **Extensions** view (`Ctrl+Shift+X`), search for
   **PlatformIO IDE** (publisher: `PlatformIO`) and install it. The
   extension downloads PlatformIO Core on its first start — this takes a
   few minutes and needs Python, which it will help you install if missing.
3. **File → Open Folder…** and select the `BACnet IP Device` folder.
   PlatformIO detects `platformio.ini` and imports the project
   automatically.

### 4. Build and flash in VS Code

The PlatformIO sidebar (the insect icon in the Activity Bar) lists
**PROJECT TASKS → env:seeed_xiao_esp32c3**:

| Task | Where | What it does |
| --- | --- | --- |
| **Build** | Project Tasks → General → Build (or the `✓` button in the status bar) | compiles the firmware — works without a board |
| **Upload** | Project Tasks → General → Upload (or the `→` button) | flashes the board over USB |
| **Monitor** | Project Tasks → General → Monitor (or the plug button) | live serial log; 115200 baud is taken from `platformio.ini` |

Every task can also be started from the command palette
(`Ctrl+Shift+P` → *PlatformIO: Build*, *PlatformIO: Upload*,
*PlatformIO: Serial Monitor*). Use **General → Upload and Monitor** to
watch the boot log immediately after flashing.

Notes:

- Close any open serial monitor before uploading — a stale monitor can lock
  the COM port.
- The **first build** downloads the Espressif 32 platform and the RISC-V
  toolchain (several hundred MB) and can take 10–20 minutes; later builds
  finish in under a minute. The project pins `platform = espressif32@6.12.0`
  (Arduino core 2.0.17), so builds are reproducible — a clean build reports
  roughly 17 % RAM and 68 % of the 1.25 MB application partition used.
- On Windows the XIAO ESP32C3 shows up as a COM port named
  **USB Serial Device** — no extra driver is normally required.
- If **Upload** cannot find a port, hold the **BOOT** button while plugging
  in the USB cable (this puts the ESP32-C3 into its ROM bootloader) and
  start Upload again.

#### Command-line alternative

Everything above also works from a plain terminal. If the `pio` command is
not on your PATH (common in Git Bash), use:

```bash
python -m platformio run                        # build (works without a board)
python -m platformio run -t upload              # flash over USB
python -m platformio device monitor -b 115200   # live serial log
```

### 5. First boot

Real Wi-Fi credentials live in `src/bacnet_secrets.h`, a git-ignored file
that is never committed (copy `src/bacnet_secrets.h.example` and fill it
in), so a fresh device joins that network automatically. The serial monitor
confirms the connection:

```
[WiFi] Connecting to "YOUR_WIFI_SSID"...
[WiFi] Connected: 192.168.1.x (-60 dBm)
[BACnet] Device 1234 ready on 192.168.1.x:47808
```

1. Connect your computer to the same network and open the device's IP from
   the serial log, or use `http://bacnet-xiao-c3.local` (mDNS).
2. **Network** tab: change the SSID/password any time, optionally set a
   static IP, then **Save and restart**.
3. **BACnet** tab: set the device instance (must be unique on your network),
   name, location and vendor details.
4. **GPIO** tab (optional): enable spare header pins as Binary Inputs or
   Binary Outputs — see [GPIO points](#gpio-points) below.
5. **Memory** tab (optional): create software-only AV/BV points — see
   [Memory points](#memory-points) below.

If the stored network cannot be reached within 25 seconds of boot, the
device opens an **open** (no password) setup access point named
`bacnet-xiao-c3-XXXX`. Join it and open <http://192.168.4.1> to configure
the correct network — a wrong password never bricks the device.

### 6. Verify it works

- **Any BACnet client**: YABE on the PC (see
  [Using YABE](#using-yabe)) or a BACnet discovery app on a phone. Press
  **Who-Is** / start a scan — the device answers within seconds and appears
  next to the other BACnet devices on your network.
- **No BACnet tools installed?** Run the bundled smoke test from the same
  computer (Python only, no dependencies):
  `python tools/bacnet_test.py --device 1234`.
- The web **Points** tab shows live values and lets you write and release
  commandable points without any BACnet client at all.

## Web interface

| Endpoint | Purpose |
| --- | --- |
| `GET /` | configuration and monitoring UI |
| `GET /api/status` | Wi-Fi, IP, RSSI, uptime, heap, instance, port |
| `GET /api/diagnostics` | reset, watchdog, BACnet and COV counters |
| `GET /api/settings` | stored configuration as JSON; passwords are never returned |
| `POST /api/settings` | update configuration (form encoded, validated) |
| `GET /api/points` | all points with present values and priority arrays |
| `POST /api/points/write` | write a value / release a priority |
| `GET /api/memory` | configurable AV/BV memory-point definitions |
| `POST /api/memory` | save memory-point definitions and restart |
| `GET /api/gpio` | GPIO point-mapping table |
| `POST /api/gpio` | save GPIO point mapping and restart |
| `POST /api/reboot` | restart |
| `POST /api/factory-reset` | erase settings and restart into setup mode |

The UI works entirely offline; the page is embedded in the firmware flash.

Configuration changes that need protection (settings, point writes, GPIO,
memory, reboot, factory reset) are rejected with HTTP errors when the
request is malformed, oversized (max 40 form arguments / 4096 bytes) or
unauthorized. When a setup-AP password is configured, these endpoints
require HTTP Basic authentication with username `admin` and that password;
password-free commissioning stays available on a first boot until a
password is set.

### GPIO points

The **GPIO** tab assigns the six spare XIAO header pins (D1, D2, D3, D4,
D5, D10) to BACnet **Binary Input (BI)** or **Binary Output (BO)** points.
The built-in points BO:0 (D0 / GPIO2) and BI:1 (D9 / BOOT) are fixed and
are not shown in this table.

Each row has:

- **Enabled** — a disabled row releases its pin entirely.
- **Kind** — Input (BI) or Output (BO).
- **Instance** — the BACnet instance number, defaulting to 2–7. Instances
  must be unique per object type and must not collide with the built-in
  BI 0/1 or BO 0.
- **Name** — the BACnet `object-name` for the point.
- **Polarity** — *Active low* means an input reads Active when the pin is
  pulled to GND (internal pull-up), and an output drives the pin low while
  the point is Active; *Active high* is the reverse.

Rows are fixed to their pin so a pin can never be assigned twice. Press
**Save and restart** to store the table in NVS — mapping changes take
effect on the next boot. Factory reset restores all rows to disabled.

Example: enable row 1 as **Output, instance 2, "Exhaust Fan", active high**
and the point appears as BO:2, commandable over BACnet and from the Points
page, driving GPIO3.

### Memory points

The **Memory** tab creates up to four software-only BACnet points. They do not
use a GPIO pin and are disabled by default:

- **Binary Value (BV)** stores Active/Inactive data.
- **Analog Value (AV)** stores a numeric value with minimum, maximum and COV
  settings.
- **Read-only** points reject Present_Value writes.
- **Writable** points use the normal BACnet priority array and NULL release
  behavior.
- **Volatile** points return to their configured default after reboot.
- **Persistent** points restore their retained value. The value is written to
  NVS a few seconds after a change to reduce flash wear; priority slots and
  COV subscriptions are not retained.

Enable a row, choose its type, instance and name, then press **Save and
restart**. Instances must be unique for the selected BACnet object type and
must not conflict with an existing point. Configuration changes take effect
on the next boot. The new definitions are stored under a separate `memory`
NVS key, so existing network and GPIO settings remain compatible. Factory
reset erases memory-point definitions too.

Example configuration:

```text
BV:100  Maintenance Mode       Writable   Persistent
AV:100  Temperature Setpoint   Writable   Persistent
AV:101  Daily Energy Target    Read-only  Volatile
```

## BACnet objects

| Object | Instance | Description | Writable |
| --- | --- | --- | --- |
| Device | 1234 (configurable) | device identity | partially |
| Analog Input | 0 | ESP32-C3 chip temperature (°C) | no |
| Analog Input | 1 | Wi-Fi RSSI (dB) | no |
| Analog Input | 2 | uptime (s) | no |
| Analog Input | 3 | generated sine 0…100 % | no |
| Analog Value | 0 | temperature setpoint, 5…35 °C | yes, priority array |
| Analog Value | 1 | analog output 0…100 % | yes, priority array |
| Binary Input | 0 | Wi-Fi connected | no |
| Binary Input | 1 | BOOT button | no |
| Binary Output | 0 | relay on GPIO2 | yes, priority array |
| Binary Value | 0 | simulation enable (Active on a fresh boot) | yes, priority array |
| Binary Input/Output | configured | GPIO-tab points on spare header pins | output yes, input no |
| Analog/Binary Value | configured | optional software memory point | according to Memory tab |

Properties implemented per object: `object-identifier`, `object-name`,
`object-type`, `description`, `present-value`, `status-flags`,
`event-state`, `reliability`, `out-of-service`, `units`, `cov-increment`,
`min/max-pres-value`, `resolution`, `priority-array`,
`relinquish-default`, `polarity`, `active-text`, `inactive-text`,
`time-delay`, `event-enable`, `acked-transitions`, `notify-type`,
`property-list`. The Device object also exposes `object-list`,
`vendor-*`, `model-name`, `firmware-revision`,
`application-software-version`, `protocol-version`,
`protocol-revision`, `protocol-object-types-supported`,
`protocol-services-supported`, `max-apdu-length-accepted`,
`segmentation-supported`, `apdu-timeout`, `number-of-apdu-retries`,
`device-address-binding`, `database-revision` and `system-status`.

Read-only points advertise and enforce their read-only status consistently:
Present_Value writes are rejected over BACnet and the property lists match
what each object actually supports.

## Using YABE

1. Connect the PC to the same subnet as the device.
2. Start YABE, set the local port to `47808` (or any free port) and add a
   broadcast address, e.g. `192.168.1.255/24`.
3. Press **Who-Is**. The device answers with an I-Am and appears in the tree.
   It answers the standard global-broadcast Who-Is, so other tools (CAS
   BACnet Explorer, BACnet apps on phones, …) find it the same way.
4. Read any point. To command AV 0 / BO 0, right-click the point, choose
   **Write Property**, enter the value and the priority (8 is a good default).

Point writes over BACnet and over the web UI use the same priority arrays, so
a value written from YABE is visible on the Points page immediately.

Priority notes (per ASHRAE 135): a WriteProperty without a priority writes
at priority 16 (the lowest), and priority 6 is reserved for the Minimum
On/Off algorithm — writes at priority 6 are rejected with
write-access-denied.

## Command-line smoke test

`tools/bacnet_test.py` needs only the Python standard library:

```bash
python tools/bacnet_test.py                    # discover + full test
python tools/bacnet_test.py --device 1234      # expect a specific instance
python tools/bacnet_test.py --host 192.168.1.50
```

It checks discovery, device properties, analog/binary reads, a
priority-8 write, a NULL release with fallback to the relinquish default,
ReadPropertyMultiple, and a COV subscription (SubscribeCOV + a pushed
change-of-value notification).

### Network sniffer

`tools/bacnet_sniffer.py` listens on UDP 47808 and prints every BACnet/IP
packet with its source address, then a per-IP summary — use it to find a
client that floods a shared network with requests (close YABE first; only
one program can own the port):

```bash
python tools/bacnet_sniffer.py --seconds 30
```

## Project layout

```
platformio.ini          PlatformIO environment (seeed_xiao_esp32c3)
src/
  pio_entry.cpp         setup()/loop()
  app.cpp               Wi-Fi, setup AP, mDNS, server lifecycle
  portal_name.h         setup-AP SSID builder (hostname + chip-id suffix)
  bacnet_config.h       compile-time defaults (first boot only)
  bacnet_secrets.h(.example)  git-ignored local Wi-Fi credentials
  bacnet_settings.h/.cpp persistent settings in NVS (network, GPIO, memory)
  bacnet_types.h        BACnet constants and value containers
  bacnet_encoding.h/.cpp tag encoding/decoding
  bacnet_objects.h/.cpp object database, properties, priority arrays, I/O
  bacnet_server.h/.cpp  BACnet/IP, BVLC/NPDU/APDU, RP/RPM/WP/COV services
  bacnet_web.h/.cpp     HTTP API, validation and configuration handlers
  web_page.h            embedded single-page UI
tools/bacnet_test.py    dependency-free BACnet/IP test client
tools/bacnet_sniffer.py UDP 47808 packet watcher
```

## Status, security boundaries and remaining work

The hardening work originally developed on the "version 1.2" branch is
**merged into `main`** and was rebuilt and flashed onto real hardware with a
full factory reset on 2026-11-18; the bundled smoke test passes all checks
on that hardware. The firmware also answers the standard global-broadcast
Who-Is after the discovery fix (`c98f8e0`), so clients find it immediately.

What is hardened in this firmware:

- **Configuration API** — every settings field is validated (hostname and
  IPv4 formats, length limits, binary values must be `0`/`1`, priorities
  1–16); malformed requests are rejected with explicit HTTP 400/413 errors.
- **Request bounding** — admin endpoints cap form arguments (40) and body
  size (4096 bytes); point writes validate object type, instance and
  priority ranges before touching the object database.
- **COV subscriptions** — bounded to 8 concurrent subscriptions with
  per-client ownership and lifetime tracking.
- **Consistent read-only enforcement** — read-only points reject
  Present_Value writes and keep their advertised property lists consistent.
- **Telemetry** — reset reason, watchdog and packet counters on
  `/api/diagnostics`.

This is **not** a certification claim. Before industrial deployment, the
operator must test the actual hardware for power cycling, brownout behavior,
relay/GPIO electrical safety, Wi-Fi loss and recovery, temperature, EMI/EMC,
long-duration operation, and flash persistence. BACnet/BTL conformance,
secure boot/flash encryption, signed OTA, and a real ASHRAE vendor ID remain
release prerequisites where required by the installation.

### Field-test checklist

- Build with `python -m platformio run`.
- Flash only after reviewing the build and backing up the current device.
- Confirm the web API does not return Wi-Fi or setup-AP passwords.
- Configure a non-empty setup-AP password before connecting the device to a
  production network. HTTP Basic authentication uses username `admin` and the
  configured setup-AP password; plain HTTP must remain on a trusted VLAN.
- Test memory-point read-only/writable behavior, priority writes, NULL release,
  reboot retention, and factory reset.
- Test GPIO input/output rows with the real load in both active-low and
  active-high polarity, and confirm a mapping change survives a restart.
- Disconnect and restore Wi-Fi; verify BACnet and the web UI recover.
- Capture `/api/diagnostics` after normal boot and after a deliberate restart.
- Re-run `python tools/bacnet_test.py` after every firmware change.

## Configuration notes

- `src/bacnet_config.h` seeds the defaults on first boot and after a factory
  reset. After that, everything is configured in the web UI.
- Real Wi-Fi credentials belong in `src/bacnet_secrets.h` (git-ignored, copy
  `src/bacnet_secrets.h.example`). The fallback defaults are empty, so a
  build without local secrets enters commissioning mode instead of trying
  placeholder credentials. Configure credentials through the setup access
  point before shipping a device or publishing the binary.
- The `firmware-revision` and `application-software-version` properties
  default to `1.1.0` and can be changed in the web UI (BACnet tab).
- Radio settings for BACnet stability: Wi-Fi modem sleep is disabled and
  TX power is pinned at the ESP32-C3 maximum (19.5 dBm). Remaining signal
  gains are physical: antenna routing/placement, distance to the router,
  and a clean 2.4 GHz path. Watch the Signal row on the Status tab
  (also BACnet point AI:1) — aim for −75 dBm or better.
- **Vendor ID 999 is the BACnet test/development ID.** Request a real vendor
  ID from ASHRAE before shipping a product.
- The device does not route, does not support segmented requests and accepts
  APDUs up to 1476 bytes. It answers the global-broadcast Who-Is (DNET
  0xFFFF) but drops NPDUs addressed to other networks. It does not provide
  BACnet/SC, BBMD, foreign-device registration, or BACnet protocol
  authentication; use a trusted VLAN/firewall for BACnet/IP control traffic.
- Built-in points are defined in `src/bacnet_objects.cpp`. GPIO points are
  mapped from the web **GPIO** tab (stored under a dedicated NVS key); user
  memory points are configured from the **Memory** tab. Do not edit the
  object table for those points.
- Memory point definitions are limited to four AV/BV rows and GPIO mappings
  to six rows in this release. A restart is required after changing their
  configuration.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Device takes minutes to appear in a BACnet app | Old firmware dropped the standard global-broadcast Who-Is; flash the latest build (`c98f8e0` or later) and re-scan |
| No setup AP | Serial log at 115200 baud; board powered and flashed |
| Serial monitor shows nothing | Stop the monitor, unplug/replug the USB cable, reopen it and pick the `USB Serial Device` COM port. Output printed before the monitor attached is not replayed, and the device logs only on events — a quiet console in setup mode is normal |
| Boot loop: `assert failed: tcpip_send_msg_wait_sem (Invalid mbox)` | Firmware 1.0.0 started the web server before the Wi-Fi stack was up; upload the fixed firmware (1.0.1 or later). The `nvs_get_blob ... cfg NOT_FOUND` line on a fresh device is harmless |
| YABE: `Communication Error: Couldn't fetch properties` | Check the Signal row on the Status tab (or read AI:1): below −75 dBm the link is marginal — move the device closer to the router. Firmware 1.0.3+ disables Wi-Fi modem sleep for BACnet latency. Also remove the device from the YABE tree and re-run Who-Is after its IP changes |
| Compiled-in defaults (Wi-Fi, AP password) not taking effect | Stored settings always override `src/bacnet_config.h` — do a Factory reset from the web UI Maintenance card to pick up new defaults |
| GPIO change did not take effect | GPIO mappings load at boot — press **Save and restart** in the GPIO tab, and confirm the row was enabled before saving |
| Web UI unreachable | Correct IP on the Status tab, port 80 free, same subnet |
| No I-Am in YABE | UDP 47808 not blocked, client and device on same subnet/VLAN |
| Device missing after reboot | Instance changed? Re-run Who-Is; check serial log |
| Write rejected | Point not commandable, value outside min/max, or bad priority |
