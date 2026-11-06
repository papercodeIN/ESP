<div align="center">

<img src="https://visitor-badge.laobi.icu/badge?page_id=papercodeIN.ESP" alt="Repository visitors" height="20" />

# ESP Tutorials — Arduino Examples & Experiments

**MQTT, Modbus TCP/RTU, DHT11 and display projects for ESP32, ESP8266 (NodeMCU), Seeed XIAO ESP32C3, ESP32-C3 Super Mini and WEMOS D1 Mini**

[![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Boards](https://img.shields.io/badge/boards-ESP32_%7C_ESP8266_%7C_XIAO-2F4F7F)](#supported-boards)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

<p align="center">
  <strong>If these tutorials help you, connect with us on social media:</strong>
</p>

[![YouTube](https://img.shields.io/badge/YouTube-Fusion_Automate-FF0000?logo=youtube&logoColor=white)](https://www.youtube.com/channel/UCKKhdFV0q8CV5vWUDfiDfTw)
[![Telegram](https://img.shields.io/badge/Telegram-%40fusionautomate-26A5E4?logo=telegram&logoColor=white)](https://t.me/fusionautomate)
[![Instagram](https://img.shields.io/badge/Instagram-%40fusion__automate-E4405F?logo=instagram&logoColor=white)](https://www.instagram.com/fusion_automate)
[![WhatsApp](https://img.shields.io/badge/WhatsApp-%2B91--9974477759-25D366?logo=whatsapp&logoColor=white)](https://wa.me/919974477759)
[![Buy Me a Coffee](https://img.shields.io/badge/Buy_Me_a_Coffee-support-FFDD00?logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/pylin)

</div>

---

## About

This is the tutorial companion repository for the **Fusion Automate** YouTube channel. Each board folder contains Arduino sketches, wiring diagrams (Fritzing), library setup screenshots and example code used in our step-by-step videos — covering **MQTT**, **Modbus TCP/RTU**, sensor publishing (**DHT11**, SHT20) and small display projects.

Tutorial status legend:

| Icon | Meaning |
|:---:|---|
| 🆓 | Free video tutorial available |
| ⌛ | Tutorial in progress — sketch is here, video coming soon |

## Supported Boards

| Board | Folder | What you'll find |
|---|---|---|
| **ESP32** | [`ESP32/`](<ESP32/README.md>) | MQTT broker tutorials using **PicoMQTT** — basic broker, custom TCP port, secure broker with username/password authentication |
| **ESP8266 (NodeMCU)** | [`ESP8266/`](<ESP8266/README.md>) | The largest collection — MQTT publishing (DHT11 → Mosquitto/HiveMQ/Ignition SCADA), secure MQTT broker (with WiFi AP mode), Modbus TCP client & server, Modbus RS485 master & slave, Modbus TCP → MQTT converter, WS2812B LED control over Modbus |
| **Seeed XIAO ESP32C3** | [`XIAO ESP32C3/`](<XIAO%20ESP32C3/>) | DHT11 reading + publishing to HiveMQ, Modbus TCP client/server, Modbus TCP → MQTT converter, 1602 LCD **desk clock** with 3D-printed case, Android Auto experiments |
| **ESP32-C3 Super Mini** | [`ESP32-C3 Super Mini/`](<ESP32-C3%20Super%20Mini/>) | Getting started — Blink |
| **WEMOS D1 Mini** | [`WEMOS D1 Mini/`](<WEMOS%20D1%20Mini/>) | Getting started — Blink |

## What's Covered

- ✅ **MQTT** — brokers on-device (PicoMQTT, secure ESP8266 broker with AP mode), publishing sensor data to Mosquitto / HiveMQ, integration with **Ignition SCADA** (MQTT Engine module)
- ✅ **Modbus TCP** — client & built-in server examples (coils, holding registers, input registers)
- ✅ **Modbus RTU (RS485)** — master and slave with wiring diagrams
- ✅ **Protocol bridges** — Modbus TCP → MQTT converter (single & multiple holding registers)
- ✅ **Sensors** — DHT11, SHT20 RS485 transmitter
- ✅ **Actuators** — WS2812B addressable LEDs, digital pin control over Modbus
- ✅ **Projects** — 1602 LCD desk clock (3D-printable case included), Android Auto on XIAO ESP32C3
- ✅ Every tutorial includes sketch + connection diagram; most include a **video** and/or **blog post**

## Repository Structure

```
ESP/
├── ESP32/                          # PicoMQTT broker tutorials (video-linked)
├── ESP32-C3 Super Mini/            # Getting started
├── ESP8266/                        # MQTT, Modbus TCP/RTU, DHT11 (full tutorial index in its README)
│   ├── DHT11_MQTT_NodeMCU_Publish/
│   ├── MQTT Broker/
│   ├── Modbus Client/
│   ├── Modbus Client + MQTT Publisher/
│   ├── Modbus Master/
│   ├── Modbus Server/
│   └── Modbus Slave/
├── WEMOS D1 Mini/                  # Getting started
├── XIAO ESP32C3/                   # Desk clock, Android Auto, DHT11, MQTT, Modbus
│   ├── Desk Clock/                 # Code + connection diagram + 3D printing files
│   ├── ESP32_Android_Auto/
│   ├── Modbus/
│   └── How to ... DHT11 .../       # Step-by-step sensor & MQTT tutorials
└── README.md
```

Full tutorial lists with video and blog links live in each board's `README.md` — start with [`ESP8266/README.md`](<ESP8266/README.md>) and [`ESP32/README.md`](<ESP32/README.md>).

## Getting Started

1. Install the [Arduino IDE](https://www.arduino.cc/en/software)
2. Add the board packages via **File → Preferences → Additional Board Manager URLs**:
   - ESP32 / XIAO ESP32C3 / ESP32-C3 Super Mini: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - ESP8266 (NodeMCU / WEMOS D1 Mini): `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Install the libraries used across tutorials (Library Manager):
   - [PicoMQTT](https://github.com/mlesniew/PicoMQTT) — on-device MQTT broker
   - [modbus-esp8266](https://github.com/emelianov/modbus-esp8266) — Modbus TCP/RTU client & server
   - **DHT sensor library** (Adafruit) — DHT11
   - **FastLED** — WS2812B LEDs
4. Open any sketch, select your board and port, and follow the matching video

## Contributing

Found an issue in a sketch or wiring diagram? Open an [Issue](https://github.com/papercodeIN/ESP/issues) — pull requests are welcome.

---

<div align="center">

### Support

If these tutorials help you, consider supporting the channel:

<a href="https://buymeacoffee.com/pylin" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" height="40"></a>

<a href="https://www.youtube.com/channel/UCKKhdFV0q8CV5vWUDfiDfTw" target="_blank"><img src="https://img.shields.io/badge/SUBSCRIBE%20ON%20YOUTUBE-FF0000?style=for-the-badge&logo=youtube&logoColor=white" alt="Subscribe on YouTube" height="35"></a>

</div>
