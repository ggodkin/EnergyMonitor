# EnergyMonitor

ESP32 + AVR128DB28 based real-time power and energy monitoring for 240V hot tub/spa.

## Features
- Voltage via ZMPT101B
- Up to 9 current channels (SCT013 15/50/100A variants)
- Real/apparent power, energy (kWh) accumulation
- MQTT publishing to Home Assistant
- OLED display (SSD1306) for live stats
- Per-sensor calibration in firmware

## Hardware Overview

| Component          | MCU       | Interface | Notes                          |
|--------------------|-----------|-----------|--------------------------------|
| ZMPT101B Voltage   | AVR/ESP32 | ADC       | Biased ~1.65V                  |
| SCT013 Current     | AVR       | ADC + OpAmp | Up to 9 channels               |
| OLED 128x64        | ESP32     | I2C       | GPIO21 SDA, GPIO22 SCL         |
| AVR ↔ ESP32 comm   | Both      | UART/I2C  | Data transfer protocol TBD     |

## Setup & Installation

1. Clone repo
2. Open in VS Code + PlatformIO
3. `pip install -r requirements.txt` (for calibration scripts)
4. Select env (avr128db28 or esp32dev) in platformio.ini
5. Build & Upload

## Calibration

- Use known loads (e.g., 10A test current) to adjust sensitivity
- ...

## Wiring Diagram

![Wiring Diagram](docs/wiring-diagram.png)

License: MIT