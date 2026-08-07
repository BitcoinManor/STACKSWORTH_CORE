# 🟠 STACKSWORTH CORE

### Where Data Comes to Life.

STACKSWORTH CORE is a compact, handcrafted Bitcoin metrics display built to keep the Bitcoin network visible on your desk.

Powered by an ESP32 and a 2.8-inch touchscreen, CORE displays live Bitcoin price, block, mining, fee, time, weather, and milestone information through a clean multi-screen interface.

CORE combines custom firmware, live data from the SatoNak metrics service, touchscreen navigation, Wi-Fi configuration, and optional CNC-milled hardwood construction in a small desktop device designed for Bitcoiners.

---

## 📌 Project Status

**Current firmware:** v2.1.4  
**Current release:** SAT Market Release  
**Hardware platform:** ESP32 with 2.8-inch ILI9341 touchscreen  
**Status:** Active development and production

Current development priorities include:

- Completing and testing the customer OTA update workflow
- Improving firmware reliability and diagnostics
- Refining screen transitions and interface alignment
- Improving the local setup portal
- Expanding the Bitcoin metrics screen collection
- Preparing production units for the Vancouver Bitcoin Block Party

---

## 🔥 Current Features

- Live Bitcoin price
- Selected fiat currency display
- 24-hour price change
- Sats per unit of fiat
- Current Bitcoin block height
- Recently identified miner or mining pool
- Recommended fee rate
- Local date and time
- Automatic timezone and daylight-saving configuration
- Local weather information
- Next Bitcoin halving countdown
- Countdown to block 1,000,000
- New-block celebration screen
- Touchscreen screen navigation
- Wi-Fi captive portal setup
- Local configuration portal at `core.local`
- Saved user settings using ESP32 Preferences
- Adjustable display brightness
- Wi-Fi reconnection handling
- Firmware version checking
- OTA firmware update system under active testing

---

## 📺 Current Screens

CORE currently includes four main touchscreen screens.

### 1. Dashboard

The primary overview screen displays:

- Bitcoin price
- Selected currency
- 24-hour change
- Sats per fiat unit
- Fee rate
- Block height
- Miner or mining pool
- Date and time
- Live connection status

### 2. Block Focus

A dedicated Bitcoin block screen featuring:

- Large current block height
- Miner or mining pool
- Automatic new-block notification

### 3. Time Focus

A local information screen featuring:

- Day of the week
- Local time
- Full date
- Saved city
- Local temperature
- Current weather condition
- Bitcoin price
- Block height

### 4. Bitcoin Milestones

A Bitcoin progress screen featuring:

- Blocks remaining until the next halving
- Target halving block
- Blocks remaining until block 1,000,000

Additional Bitcoin network and market screens are being evaluated for future firmware releases.

---

## 🧱 Hardware

STACKSWORTH CORE is currently built around:

- ESP32 development board
- 2.8-inch ILI9341 TFT LCD
- 320 × 240 landscape resolution
- XPT2046 resistive touchscreen
- Wi-Fi connectivity
- USB power
- Optional internal rechargeable battery support
- SPIFFS storage for the local web portal
- CNC-milled hardwood enclosure options

### Display and Touch Configuration

The current production hardware uses:

- ILI9341 display driver
- LovyanGFX graphics library
- XPT2046 touch controller
- Separate SPI buses for display and touch

---

## 🪵 Hardwood Edition

The premium STACKSWORTH CORE enclosure is CNC milled, sanded, stained, and finished by hand in Calgary, Alberta, Canada.

The current hardwood configuration uses:

- Black Walnut rear enclosure
- Orange Padauk faceplate
- African Mahogany faceplate
- Purpleheart faceplate
- Special Edition Zebra Faceplate

Natural colour and grain variation make every hardwood CORE unique.

---

## 🌐 Wi-Fi Setup

When a new CORE is powered on without saved Wi-Fi credentials, it starts its own setup access point.

### Initial Setup

1. Power on the CORE.
2. Connect your phone or computer to the CORE Wi-Fi access point.
3. Open the setup portal at:

   `192.168.4.1`

4. Enter the Wi-Fi network credentials.
5. Select the city, timezone, currency, temperature unit, brightness, and device name.
6. Save the settings.
7. Allow CORE to restart and connect to the selected network.

After setup, the local configuration portal can normally be reached at:

`http://core.local`

The device and the browser must be connected to the same local network.

---

## ⚙️ Saved Settings

CORE currently stores the following settings in ESP32 non-volatile preferences:

- Wi-Fi network name
- Wi-Fi password
- City
- Timezone
- Fiat currency
- Temperature unit
- Device name
- Screen brightness

These settings are designed to remain stored after a normal restart.

Preserving all customer settings during OTA firmware updates is part of the current release-testing checklist.

---

## 🌍 SatoNak Data Service

CORE retrieves Bitcoin information through the Bitcoin Manor SatoNak metrics service.

Current data includes:

- Bitcoin price
- Block height
- Miner or mining pool
- Fee rate
- 24-hour price change
- Market data used by current or future screens

Centralizing these requests through SatoNak gives STACKSWORTH devices a consistent data format and allows data-source improvements without requiring every device to contact several public services independently.

Weather information is retrieved separately using location data saved by the user.

---

## 🔄 Firmware and OTA Updates

The STACKSWORTH CORE firmware is written in C++ using the Arduino framework for ESP32.

The main source is maintained as an Arduino sketch with an `.ino` file extension.

### Terminology

- **Firmware:** The compiled software installed and executed on the CORE.
- **Source code:** The human-readable C++ and Arduino code used to build the firmware.
- **Arduino sketch:** The project source saved in an `.ino` file.
- **Firmware binary:** The compiled `.bin` file installed through USB or OTA.

CORE already contains the foundation for over-the-air firmware updates through the local web portal.

The OTA release workflow is currently being hardened and tested before it is relied upon for customer updates.

Testing requirements include:

- Detecting a newer firmware release
- Downloading the correct CORE firmware binary
- Confirming model and version compatibility
- Preserving Wi-Fi and user preferences
- Handling interrupted or failed downloads
- Rebooting into the new firmware successfully
- Completing multiple consecutive OTA updates
- Testing updates on more than one physical CORE
- Confirming long-term stability after updating

Until the full workflow has passed these tests, OTA should be considered an active development feature.

---

## 🧪 Firmware Release Checklist

Before a firmware version is marked stable, it should pass the following checks.

### Boot Experience

- Smooth startup
- Clear STACKSWORTH splash screen
- Visible hardware-check progress
- Clear Wi-Fi connection status
- Predictable setup mode when credentials are missing
- Professional transition into the main interface

### User Interface

- Correct alignment on all screens
- Consistent spacing
- Readable typography
- Accurate touch response
- Clean screen transitions
- No overlapping or clipped data
- Correct formatting for large values

### Reliability

- Stable Wi-Fi connection
- Automatic Wi-Fi reconnection
- Graceful handling of unavailable APIs
- Stable memory usage
- No progressive heap loss
- Accurate time synchronization
- Reliable new-block detection
- Long-duration runtime testing
- OTA update tested repeatedly

### User Experience

- Straightforward first-time setup
- Clear local portal instructions
- Settings preserved after reboot
- Helpful error messages
- Obvious update status
- Safe update instructions
- Recovery after interrupted updates
- Intuitive touchscreen navigation

---

## 🧰 Building the Firmware

The project is developed using the Arduino framework for ESP32.

Typical development requirements include:

- Arduino IDE or compatible ESP32 build environment
- ESP32 board support package
- LovyanGFX
- ArduinoJson
- ESPAsyncWebServer
- AsyncTCP
- ESP32 Preferences
- SPIFFS support
- ESP32 Update library

The exact production board profile, flash size, partition scheme, dependency versions, and build procedure will be documented before public firmware releases are published.

Do not flash a production CORE using an unverified partition scheme or incompatible hardware configuration.

---

## 📁 Planned Repository Structure

```text
stacksworth-core-firmware/
├── STACKSWORTH_CORE.ino
├── data/
│   └── core.html.gz
├── docs/
│   ├── BUILDING.md
│   ├── OTA-TESTING.md
│   └── HARDWARE.md
├── CHANGELOG.md
├── LICENSE
└── README.md

## 🚧 Development Roadmap

### Firmware Reliability

- Complete OTA validation and recovery workflow
- Add firmware integrity verification
- Add hardware and model compatibility checks
- Add update progress feedback
- Add free-heap and minimum-heap monitoring
- Add uptime and diagnostic logging
- Improve non-blocking Wi-Fi recovery
- Improve API error tracking

### Portal Improvements

- Display installed firmware version
- Display latest available firmware version
- Add firmware release notes
- Add clear update confirmation
- Add update progress and result messages
- Improve mobile layout
- Improve setup guidance
- Add clearer recovery instructions

### Display Improvements

- Refine current screen alignment
- Improve screen transitions
- Reduce unnecessary full-screen redraws
- Add one or more dedicated Bitcoin network screens
- Evaluate mining and difficulty information
- Expand user-selectable display options

---

## 🧡 Design Philosophy

STACKSWORTH CORE is designed around three principles:

> **Simple. Reliable. Beautiful.**

CORE is not intended to be another temporary gadget or a screen filled with unnecessary information.

It is designed to make Bitcoin visible, understandable, and present in everyday life through meaningful live data and handcrafted construction.

---

## 🔗 STACKSWORTH Ecosystem

CORE is part of the wider STACKSWORTH Bitcoin display lineup.

- **MATRIX** — Scrolling LED Bitcoin ticker
- **CORE** — Compact 2.8-inch touchscreen Bitcoin display
- **SPARK** — 7-inch touchscreen Bitcoin display
- **PULSE** — 5-inch Bitcoin node and touchscreen platform
- **EDGE** — 7-inch Bitcoin node and touchscreen platform
- **INFINITY** — 10-inch premium Bitcoin node and multimedia display

Future models will build upon the firmware, data, portal, and update systems developed for CORE.
