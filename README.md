# EnergyMonitor

ESP32 + AVR128DB28 based real-time power and energy monitoring system for a 240V hot tub/spa in split-phase US setup.

Measures voltage (ZMPT101B), current (up to 9× SCT013 variants: 15A/1V, 50A/1V, 100A/1V), calculates RMS values, real/apparent power, power factor, and accumulated energy (kWh). Data published via MQTT to Home Assistant, with local OLED display for quick stats.

## Features
- Precise analog sampling on AVR128DB28 (12-bit ADC + on-chip OpAmps for biasing/filtering)
- Wi-Fi, MQTT, OTA updates, and OLED display on ESP32
- Support for multiple CTs (current transformers) with per-sensor calibration
- Ground leakage current detection option
- Python scripts for calibration/analysis (see `requirements.txt`)

## Hardware Overview

### Microcontrollers
- **AVR128DB28** (28-pin package): Handles analog measurements (voltage + currents), uses EmonLibCM (avrdb branch) for accurate RMS/power/energy calc.
- **ESP32** (DevKit / Dev Module): Connectivity (Wi-Fi/MQTT), OLED display, data aggregation from AVR.

### Sensors
- **ZMPT101B** — AC voltage sensor (biased ~1.65V midpoint)
- **SCT013** variants — Non-invasive current sensors (15/50/100A models with 1V output; built-in burden resistor)
- Optional: Additional CT for ground wire leakage

### Other Components
- OLED 128×64 SSD1306 (I2C) — Displays real-time stats for first two sensors + total attached CTs
- Filtering capacitors (e.g., 10μF on ADC inputs for noise reduction)

## Recommended MCU Pin Assignments

These are suggested starting pins for prototyping (based on AVR128DB28 28-pin SSOP/SOIC/SPDIP and standard ESP32 DevKit). Adjust in `include/config.h` or code as needed. See Microchip AVR DB datasheet and ESP32 pinout guides for details.

### AVR128DB28 (28-pin package)

| Pin # | AVR Pin Name | Function / Connection                  | Notes / Why Chosen                          |
|-------|--------------|----------------------------------------|---------------------------------------------|
| 1     | PA7         | Debug LED or spare                     | General digital I/O, PWM capable            |
| 4     | PA3         | SCT013 #1 (Current 1) → ADC input      | ADC + OpAmp accessible                      |
| 5     | PD4         | SCT013 #2 (Current 2) → ADC input      | ADC + OpAmp                                 |
| 6     | PD2         | SCT013 #3 (Current 3) → ADC input      | ADC-capable                                 |
| 7     | PD3         | SCT013 #4 (optional)                   | ADC-capable                                 |
| 8     | PD1         | UART TX to ESP32                       | USART0 TX (Serial1)                         |
| 9     | PA4         | UART RX from ESP32                     | USART0 RX                                   |
| 10    | UPDI        | UPDI programming pin                   | Required for programming (pyupdi/tool)      |
| 11    | PF6         | Optional ground leakage CT             | Analog-capable                              |
| 23    | PA5         | ZMPT101B voltage sense                 | ADC + OpAmp (biasing/filtering)             |
| 24    | PA6         | Spare / additional CT                  | ADC-capable                                 |
| 27    | PD6         | Spare                                  | ADC-capable                                 |
| -     | VDD/AVDD    | 3.3V or 5V supply                      | AVDD tied to VDD internally                 |
| -     | GND         | Ground                                 | Multiple for good decoupling                |

**AVR Notes**:
- Use on-chip OpAmps for 1.65V biasing of AC signals.
- Add 10μF caps near ADC inputs for filtering.
- Up to ~10 ADC channels available for more CTs.

### ESP32 (DevKit-style board)

| ESP32 GPIO | Function / Connection                  | Notes / Why Chosen                                      |
|------------|----------------------------------------|---------------------------------------------------------|
| GPIO21     | SDA (I2C to OLED SSD1306)              | Standard I2C default (Arduino-ESP32)                    |
| GPIO22     | SCL (I2C to OLED)                      | Standard I2C default                                    |
| GPIO16     | UART RX from AVR                       | UART2 RX (dedicated, no conflicts)                      |
| GPIO17     | UART TX to AVR                         | UART2 TX                                                |
| GPIO4      | Spare / status LED or button           | General purpose, ADC-capable if needed                  |
| GPIO5      | Spare                                  | General purpose                                         |
| 3V3 / GND  | Power to OLED/sensors                  | 3.3V logic (AVR MVIO compatible)                        |

**ESP32 Notes**:
- Avoid strapping pins (GPIO0/2/12/15) for general use.
- OLED: Use Adafruit_SSD1306 + Adafruit_GFX libs (address 0x3C typical).
- UART to AVR: 115200 baud recommended.

## Wiring Tips
- **ZMPT101B**: Connect output to AVR PA5 (or ESP32 ADC if desired); bias midpoint ~1.65V using voltage divider or OpAmp.
- **SCT013**: Output to AVR ADC pins (direct, since 1V output fits 3.3V/5V range with bias).
- **AVR ↔ ESP32**: UART cross-connect (TX→RX, RX→TX); share GND.
- **OLED**: I2C with 4.7kΩ pull-ups if not built-in.
- Use level shifters if mixing 5V AVR with 3.3V ESP32 (though MVIO helps).
- Add decoupling caps (0.1μF ceramic + 10μF) near each MCU.

(Coming soon: Wiring diagrams in `docs/` folder)

## Setup & Installation

1. Clone the repo:
   ```bash
   git clone https://github.com/ggodkin/EnergyMonitor.git
   cd EnergyMonitor