<div align="center">

<img src="https://visitor-badge.laobi.icu/badge?page_id=papercodeIN.ESP-Private-Projects" alt="Repository visitors" height="20" />

# Private & Commercial Projects

**Production-ready ESP32 / ESP8266 firmware for BACnet/IP and Modbus TCP — available on request**

These projects are **not open source**. This folder is a public index only: it shows what is available and how to reach us. Source code, build files and hardware design files are kept private.

[![Source](https://img.shields.io/badge/source-closed-red)](#what-this-folder-is)
[![Enquiry](https://img.shields.io/badge/enquiry-form_on_request-2F4F7F)](#how-to-enquire)
[![YouTube](https://img.shields.io/badge/YouTube-Fusion_Automate-FF0000?logo=youtube&logoColor=white)](https://www.youtube.com/channel/UCKKhdFV0q8CV5vWUDfiDfTw)
</div>

---

## What this folder is

The [main ESP repository](../README.md) contains free Arduino tutorials and example sketches. The projects below are different: they are complete, web-configured firmware products used for real BACnet/IP and Modbus TCP integration work, and their source is not published.

If you want to **build something similar** — or deploy one of these as-is — contact us and ask for the **project enquiry form and commercial terms**. We will share the scope, deliverables, pricing and lead time for your requirement.

> **In short:** the README is public, the source is private, and every engagement starts with a short enquiry.

## Available projects

| Project | Direction | Runs on | Capacity |
|---|---|---|---|
| **ESP32 BACnet/IP Device** | BACnet/IP device — read, write, COV | Seeed XIAO ESP32C3 | 6 GPIO points + up to 4 software points |
| **BACnet/IP → MQTT Gateway (Uni)** | BACnet → MQTT, read-only | XIAO ESP32-C3, ESP32 DevKit | 8 devices / 32 points |
| **BACnet/IP ⇄ MQTT Gateway (Bi)** | BACnet ↔ MQTT, with write-back | XIAO ESP32-C3, ESP32 DevKit | 8 devices / 32 points |
| **Modbus TCP → MQTT Gateway (Uni)** | Modbus TCP → MQTT, read-only | ESP8266, ESP32, WT32-ETH01, XIAO ESP32-C3 | 6 Modbus TCP servers |
| **Modbus TCP ⇄ MQTT Gateway (Bi)** | Modbus TCP ↔ MQTT, with write-back | ESP8266, ESP32, WT32-ETH01, XIAO ESP32-C3 | 6 Modbus TCP servers |

---

## 1. ESP32 BACnet/IP Device

A self-contained BACnet/IP device for the **Seeed Studio XIAO ESP32C3**. It speaks standard BACnet/IP on UDP 47808, so it appears immediately in YABE, Android BACnet apps and any other BACnet client — with a built-in web interface for setup and live monitoring, and no recompiling to change Wi-Fi, the device instance, GPIO points or memory points.

**Highlights**

- BACnet/IP (Annex J) with BVLC Original-Unicast / Original-Broadcast and global-broadcast discovery
- **Who-Is / I-Am**, ReadProperty, ReadPropertyMultiple, WriteProperty and **SubscribeCOV** (8 bounded subscriptions)
- Device, Analog Input/Value and Binary Input/Output/Value objects
- **6 configurable GPIO points** (binary input or output, own instance, name and polarity)
- Up to **4 software memory points** with optional persistence, min/max limits and COV increment
- Full **16-level priority arrays** on commandable points
- Web UI for Wi-Fi, static IP, device identity, points and diagnostics; live points page with priority release
- Setup access point fallback, mDNS, NVS-persisted settings, factory-reset button, authenticated API
- No cloud dependency and no dynamic allocation in the BACnet path

**Typical use:** BACnet training kits, product prototypes, BMS integration testing, stand-alone I/O devices.

## 2. BACnet/IP → MQTT Gateway (Unidirectional)

Reads BACnet/IP objects and publishes their values to an MQTT broker. It is **read-only by design** — it only sends BACnet `ReadProperty` requests, so it can never change a controller's setpoints or outputs.

**Highlights**

- Up to **8 BACnet/IP devices** and **32 points** (analog, binary and multi-state objects)
- **Who-Is discovery** — browse the object list and tick the points you want; no instance numbers to type
- JSON payload per device **or** one MQTT topic per point (RAW mode)
- Browser-based configuration and live dashboard; editable device/point tables, bulk point generation, config export/import
- Recovery access point (`BacnetGateway-AP`) when Wi-Fi is missing or wrong, plus mDNS, OTA updates and SNTP timestamps
- Power-fail-safe configuration storage in LittleFS with backup and EEPROM mirror

**Typical use:** monitoring BMS / BACnet controllers into MQTT, SCADA or cloud dashboards without any write risk.

## 3. BACnet/IP ⇄ MQTT Gateway (Bidirectional)

The bidirectional version of the gateway above: BACnet values are published to MQTT as usual, and **writable points also accept MQTT commands** that are written back to the BACnet controller using `WriteProperty`. Read-only points remain protected.

**Highlights**

- Everything in the unidirectional gateway, plus opt-in write-back per point
- MQTT command handling with write confirmation and error reporting
- Suitable for setpoint and command control from MQTT dashboards while keeping monitoring points read-only
- Same web configuration, live dashboard, discovery, OTA and recovery features

**Typical use:** BMS integration where a SCADA or MQTT dashboard must both supervise and command BACnet points.

## 4. Modbus TCP → MQTT Gateway (Unidirectional)

Polls one or more **Modbus TCP/IP** devices and publishes their tag values to an MQTT broker. Fully web-configured, and **read-only** — nothing in the firmware ever writes back to a Modbus device.

**Highlights**

- Builds for **ESP8266** (NodeMCU / Wemos D1 mini), **ESP32**, wired **WT32-ETH01** (Ethernet) and **Seeed XIAO ESP32-C3**
- Up to **6 Modbus TCP servers** (IP/hostname, port, unit ID); every tag is bound to one device
- Four function codes: coils (FC01), discrete inputs (FC02), holding registers (FC03), input registers (FC04)
- **Read optimisation** — tags are merged into the largest consecutive ranges and fetched with one request (max 125 registers / 2000 bits per block)
- Data types: bool, uint16, int16, uint32, int32, float32 — with word swap, scale, offset, decimals and unit
- MQTT authentication, base topic, retain flag, LWT and gateway telemetry
- Live dashboard with per-tag **Good / Stale / Bad** quality, per-device state and error counters
- Fixed IP or DHCP, captive-portal recovery AP, config backup/restore, OTA updates and SNTP timestamps
- Fault isolation and non-blocking polling — one bad tag, block or device never stops the scan

**Typical use:** PLC, energy meter and Modbus TCP tag monitoring into MQTT / SCADA / cloud.

## 5. Modbus TCP ⇄ MQTT Gateway (Bidirectional)

The bidirectional version: it polls Modbus TCP devices and publishes values to MQTT, and tags marked **Writable** accept MQTT commands that are written back to the Modbus device.

**Highlights**

- Everything in the unidirectional gateway, plus **write-back (opt-in)** for coils and holding registers
- Writes use FC05, FC06, FC15 and FC16
- Command topics per writable tag, with write OK / failed counters in the dashboard and telemetry
- Same block-optimised polling, data types, recovery AP, OTA and fault isolation

**Typical use:** dashboards that need to command coils or setpoints as well as monitor Modbus TCP equipment.

---

## How to enquire

1. **Pick a project** from the list above — or describe the variant you need if none fits exactly.
2. **Message us** on WhatsApp or email with:
   - the project name
   - your board / hardware
   - the protocols and devices you need to integrate
   - quantity and deployment timeline
3. We reply with the **project enquiry form** and the **commercial terms** — scope, deliverables, pricing and lead time.
4. Once the scope is agreed, we proceed as per the proposal.

Custom variants — extra points, different boards, branding, protocol adjustments or integration support — can be quoted as part of the same enquiry.

## Contact

<div align="center">

[![WhatsApp](https://img.shields.io/badge/WhatsApp-%2B91--9974477759-25D366?style=for-the-badge&logo=whatsapp&logoColor=white)](https://wa.me/919974477759)
[![Email](https://img.shields.io/badge/Email-eng.innovativ%40gmail.com-c71610?style=for-the-badge&logo=gmail&logoColor=white)](mailto:eng.innovativ@gmail.com)
[![Website](https://img.shields.io/badge/Website-fusionautomate.in-blueviolet?style=for-the-badge&logo=internet&logoColor=white)](https://fusionautomate.in/)
[![Telegram](https://img.shields.io/badge/Telegram-%40fusionautomate-26A5E4?style=for-the-badge&logo=telegram&logoColor=white)](https://t.me/fusionautomate)

</div>

Please use WhatsApp or email for commercial enquiries rather than GitHub issues, so we can share the form and pricing privately.

## Frequently asked

**Is the source code public?**
No. These are closed-source commercial projects; only this index README is public. Deliverables are defined in the commercial terms shared on request.

**Are these the same as the tutorials in the main repository?**
No. The main [ESP repository](../README.md) is free tutorial material. These are complete products with web configuration, OTA, recovery and production features.

**Can you build a custom version?**
Yes — tell us the board, point count, protocols and dashboard requirements, and we will quote the change.

**How is pricing decided?**
Per project scope: number of points/devices, hardware target, customisation and support. Ask for the current enquiry form for details.

---

<div align="center">

### Looking for free examples?

Browse the [ESP tutorial repository](../README.md) for open-source MQTT, Modbus and sensor examples for ESP32 and ESP8266.

</div>
