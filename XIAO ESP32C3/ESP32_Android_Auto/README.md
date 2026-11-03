# ESP32 as a Wired → Wireless Android Auto Bridge — Feasibility Study

> **Status:** Planning / feasibility stage only — no firmware written yet.
> **Goal:** Plug an ESP32 dongle into the car's **wired Android Auto** USB port so the dongle appears to the car as the phone (USB Android Auto), while the phone connects to the dongle **wirelessly** (Wi-Fi and/or Bluetooth). Effectively a DIY version of commercial wireless-AA dongles (AAWireless, Motorola MA1).

---

## TL;DR Verdict

| Question | Answer |
|---|---|
| Is this possible with the **XIAO ESP32-C3**? | ❌ **No — hard blockers** (see below) |
| Is it possible with **Wi-Fi**? | ✅ Yes (Wi-Fi carries all AA data) |
| Is it possible with **Bluetooth only**? | ❌ No (BT is only the handshake, never the data transport) |
| Recommended hardware | **XIAO ESP32-S3** (Wi-Fi variant), optional **classic ESP32** add-on for the Bluetooth auto-connect handshake |
| Reference projects | Both checked — see [Reference projects](#reference-projects) |

---

## Why the ESP32-C3 is disqualified

Two hardware limitations that no amount of software can work around:

### 1. No USB device mode (fatal for the car side)
- The car's head unit is the **USB host**; the dongle must present itself as a **USB device** and respond to the Android Open Accessory 2.0 handshake (vendor requests `51` / `53`), then re-enumerate as an AA accessory (`VID 0x18D1 / PID 0x2D00`) with bulk endpoints.
- The ESP32-C3 has **no USB-OTG peripheral**. Its USB port is a fixed-function **USB-Serial/JTAG** controller (CDC serial + JTAG only). It can never enumerate as a custom USB device, so the head unit will never engage Android Auto.
- Custom USB device roles (TinyUSB) require **ESP32-S2 / ESP32-S3 / ESP32-P4**.

### 2. No Bluetooth Classic (fatal for the phone side)
- The ESP32-C3 has **BLE 5 only** — no Bluetooth Classic (BR/EDR), therefore no RFCOMM.
- The official wireless-AA trigger requires Bluetooth Classic (see next section). BLE cannot substitute in stock Android Auto.

Because of blocker 1, the C3 fails even in a "Wi-Fi-only" variant. Blocker 2 would additionally break the Bluetooth variant.

---

## How wireless Android Auto actually works

Wireless AA is **not** "Android Auto over Bluetooth" and **not** "Android Auto over Wi-Fi" — it is a two-phase protocol:

1. **Bluetooth Classic — trigger & handshake.** The adapter and phone pair over BT. The adapter opens an RFCOMM service (AA service UUID, channel 8) and the Wi-Fi SSID/password are exchanged over that secure socket.
2. **Wi-Fi — all projection data.** The phone joins the adapter's Wi-Fi network, opens a **TCP connection on port 5277**, upgrades to **TLS 1.2**, and runs the AA protocol: protobuf messages across channels (control, input, sensor, video, 3× audio, navigation, media status), each framed with a 4-byte header (channel, flags, length).

Implications:

- **Bluetooth-only AA:** ❌ Not possible. Stock AA never carries the video/audio protobuf streams over BT (BT EDR ≈ 2–3 Mbps is far too little anyway). BT handles only the handshake and optionally hands-free call audio.
- **Wi-Fi-only AA:** ⚠️ Possible with a workaround. A stock phone won't start AA over a random Wi-Fi AP without the BT handshake. Workaround: enable AA **developer settings** (tap the version in the AA app repeatedly) and use the **"Start head unit server"** option — the phone then serves the AA protocol on TCP 5277 once it joins your AP. Trade-off: one-time manual setup on the phone, vs. fully automatic connection with the Bluetooth flow.

---

## Reference projects

### [madkoding/esp32-android-auto-wifi](https://github.com/madkoding/esp32-android-auto-wifi)
- **Architecturally our exact concept:** dongle plugged into the car via USB (AOA 2.0), phone connecting over Wi-Fi.
- Actually targets the **ESP32-S2** — the C3 references in its build instructions are a leftover/inconsistency (the S2 has native USB-OTG; a C3 build makes no sense).
- Since the S2 has no Bluetooth at all, it uses a **companion Kotlin app** on the phone to initiate the wireless session instead of the stock BT handshake.
- Rust-based, zero-copy architecture (static ring buffers, DMA, lock-free buffer management).
- **Status:** experimental — no releases, no throughput numbers, no proven end-to-end run.

### [koenieee/esp32-android-auto-nav](https://github.com/koenieee/esp32-android-auto-nav)
- **Not a car adapter** — it's a DIY **head unit** with a display (WT32-SC01 Plus, ESP32-S3R8 @ 240 MHz, 2 MB PSRAM).
- Hosts a Wi-Fi AP (`ESP32-AA-HU`), phone joins it and streams; ESP32 decodes H.264 800×480 in software → only ~3–5 fps, audio channels stubbed out. Archived (read-only).
- **Value for us: excellent protocol reference** — documents the wireless AA link end-to-end (TCP 5277 → TLS → protobuf channels, mDNS `_androidauto._tcp`, DHCP without gateway so the phone keeps 4G data).

---

## Feasible architectures

> Key constraint: **no single Espressif chip has both USB-OTG and Bluetooth Classic** — S2/S3/P4 have USB but BLE-only; the classic ESP32 has BT Classic + Wi-Fi but no USB.

### Variant A — XIAO ESP32-S3 only (recommended starting point)
- **Car side:** TinyUSB device mode (AOA 2.0) over the S3's native USB-OTG.
- **Phone side:** S3 hosts a Wi-Fi SoftAP + TCP 5277 AA server; phone triggers via AA developer mode ("head unit server").
- Lower complexity, single chip. BLE stays available for a status/config UI.

### Variant B — ESP32-S3 + classic ESP32 (stock experience)
- Same S3 core (USB + Wi-Fi data path), plus a **classic ESP32** module (~$3) that emulates the official **BT Classic RFCOMM handshake** — phone pairs, receives SSID/PSK over the socket, joins the S3's AP, and AA starts **fully automatically**, like a real wireless-AA car / AAWireless dongle.
- The classic ESP32 is not on the data path — it only whispers credentials over BT, so it barely needs to talk to the S3 (UART sync, negligible traffic).

| | Variant A (S3 only) | Variant B (S3 + classic ESP32) |
|---|---|---|
| Car side | TinyUSB device (AOA 2.0) | Same |
| Phone trigger | AA developer-mode (manual-ish) | Stock BT handshake → auto-connect |
| Data path | Phone → S3 SoftAP → relay → car USB | Same |
| Complexity / cost | Lower | Higher |

---

## Risks & open questions for the build phase

- **USB bandwidth:** S2/S3 USB is **Full-Speed (12 Mbps)** → realistically ~7–9 Mbps. The negotiated video resolution/bitrate must be capped in the AA handshake (e.g., 800×480 @ 30 fps, ~3–6 Mbps H.264) so the phone→Wi-Fi→USB chain fits. Commercial dongles do the same.
- **TLS on MCU:** the Wi-Fi link requires TLS 1.2 — S3 hardware AES/SHA + 8 MB PSRAM (XIAO ESP32-S3) make this tractable. (The koenieee project's poor performance came from software video *decoding*, which a relay never does.)
- **Call audio (Variant B):** in official wireless AA, BT stays connected and call audio may route over HFP to the "head unit" — the classic ESP32 must handle this or calls may be silent.
- **Wi-Fi Direct vs SoftAP:** official head units often use Wi-Fi Direct (P2P); ESP-IDF has no native P2P support. SoftAP works (proven by reference projects), but phone behavior with credential handoff needs testing.
- **Head-unit compatibility:** AOA behavior varies by manufacturer; must be tested against the target car.

---

## Recommendation

1. Proceed on the **XIAO ESP32-S3**, not the C3.
2. **One project, staged:** start with **Variant A** (single S3, developer-mode trigger) to prove the USB + Wi-Fi relay pipeline — this is the hard part.
3. Bolt on **Variant B's** classic-ESP32 Bluetooth handshake later using the same S3 core.
4. **Skip** a Bluetooth-transport project entirely — it is not a viable transport.

## Sources

- [madkoding/esp32-android-auto-wifi](https://github.com/madkoding/esp32-android-auto-wifi) — reference dongle project (ESP32-S2 + companion app)
- [koenieee/esp32-android-auto-nav](https://github.com/koenieee/esp32-android-auto-nav) — DIY head unit / wireless AA protocol reference (TCP 5277, TLS, protobuf channels)
- [PlatformIO forum — ESP32-C3 has no USB OTG](https://community.platformio.org/t/esp32-usb-otg-board-needed/23087)
- [Espressif — USB-Serial/JTAG peripheral introduction](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/usb-serial-jtag-console.html)
- [open-android-auto — wireless Bluetooth setup (RFCOMM UUID / channel 8)](https://github.com/mrmees/open-android-auto/blob/main/docs/wireless-bluetooth-setup.md)
- [Engadget — why wireless Android Auto uses both Bluetooth and Wi-Fi](https://www.engadget.com/2210415/why-wireless-android-auto-uses-both-bluetooth-wifi)
