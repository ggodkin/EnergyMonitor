# EnergyMonitor

Real-time power and energy monitoring system for a 240 V split-phase hot tub/spa using a two-MCU architecture:

- **AVR128DB28** (28-pin): High-speed, precise analog sampling of voltage and multiple current channels using built-in op-amps and 12-bit differential ADC.
- **ESP32**: Wi-Fi connectivity, MQTT publishing to Home Assistant, small OLED display, OTA updates.

Supports multiple SCT-013 current transformers (15/50/100 A variants), ZMPT101B voltage sensor, optional ground leakage monitoring, RMS/real/apparent power, power factor, and kWh accumulation.

## Features

- Multi-channel current monitoring (up to 4+ legs + leakage)
- Accurate split-phase 240 V power calculation (US residential)
- Per-sensor calibration stored in EEPROM
- MQTT integration with Home Assistant (JSON payloads)
- Small OLED display on ESP32 (voltage, current, power, energy)
- OTA firmware updates for ESP32
- Low-cost hardware (~$30–50 excluding enclosure/sensors)

## Hardware Overview

- **AVR side**: Focuses on clean, noise-free analog sampling at high speed. Uses built-in op-amps for CT biasing/gain.
- **ESP32 side**: Handles networking, display, and data aggregation via UART from AVR.
- **Communication**: UART (57600 baud default) between AVR and ESP32.
- **Power supply**: Shared 3.3 V rail recommended (AVR runs fine at 3.3 V; matches ESP32 logic levels).

**Important Electrical Notes**
- Run the AVR128DB28 at **3.3 V** (VDD/AVDD) to match ESP32 GPIO levels — no level shifters needed on UART.
- If using 5 V on AVR (for better ADC range): Add bidirectional level shifters (e.g., TXS0102) on UART TX/RX lines to protect ESP32 (3.3 V max).
- **Shared GND** between AVR and ESP32 is mandatory for reliable UART communication.
- Add decoupling capacitors: 0.1 µF + 10 µF near each MCU's VCC/GND pins.
- **UPDI programming**: PA0 (pin 19) must remain accessible — use a header or pogo pins for in-circuit programming.

## Pin Assignments

### AVR128DB28 (28-pin SSOP/SOIC/SPDIP)

Based on Microchip datasheet DS40002247A. Only ADC-capable pins used for analog signals.

| Physical Pin | AVR Name | Function                        | Notes / Alternate Functions                  |
|--------------|----------|---------------------------------|----------------------------------------------|
| 19           | PA0      | UPDI (programming)              | **Do not connect sensors** – reserved        |
| 7            | PD1      | UART TX → ESP32 RX              | USART0 TX (default)                          |
| 25           | PA4      | UART RX ← ESP32 TX              | USART0 RX (default)                          |
| 24           | PA3      | SCT013 #1 (Current Leg 1)       | ADC-capable, OpAmp accessible                |
| 23           | PA2      | SCT013 #2 (Current Leg 2)       | ADC-capable                                  |
| 8            | PD2      | SCT013 #3 (Current Leg 3)       | ADC-capable                                  |
| 9            | PD3      | SCT013 #4 (optional/extra)      | ADC-capable                                  |
| 26           | PA5      | ZMPT101B voltage sense          | ADC-capable, preferred for OpAmp biasing     |
| 18           | PF6      | Optional ground leakage CT      | ADC-capable (PORTF analog)                   |
| 27           | PA6      | Spare / extra CT possible       | ADC-capable                                  |
| 12           | PD6      | Spare / extra CT possible       | ADC-capable                                  |
| 1            | PA7      | Debug LED or spare              | PWM-capable                                  |

**Power pins (do not use for signals):**
- Pin 6: VDDIO2
- Pin 14: AVDD (internally tied to VDD)
- Pin 20: VDD
- Pin 15, 21: GND

### ESP32 (DevKit / NodeMCU style)

| GPIO    | Function                  | Notes                                      |
|---------|---------------------------|--------------------------------------------|
| 21      | I²C SDA (OLED display)    | Default Arduino-ESP32 I²C                  |
| 22      | I²C SCL (OLED display)    | Default, add 4.7 kΩ pull-ups if needed     |
| 16      | UART RX ← AVR TX          | UART2 – no boot conflicts                  |
| 17      | UART TX → AVR RX          | UART2 – safe                               |
| 4, 5    | Spares / status LED       | General purpose, ADC if needed             |

**Avoid strapping pins** (GPIO0, 2, 12, 15) for peripherals to prevent boot issues.

## Wiring Notes

- **Current Transformers (SCT-013)**: Burden resistor on board (e.g., 33 Ω for 100 A model), biased to VREF/2 via op-amp or resistor divider.
- **ZMPT101B voltage sensor**: Output scaled to 0–3.3 V range, centered at 1.65 V.
- **UART**: Direct connect if both at 3.3 V; level shift if AVR at 5 V.
- **OLED**: SSD1306 128×64 I²C display (0.96" common).
- **Calibration**: Use Python scripts in repo for scaling factors and phase correction.

## Setup & Installation

1. Install **PlatformIO** in VS Code.
2. Clone repo: `git clone https://github.com/ggodkin/EnergyMonitor.git`
3. Open project folder in VS Code.
4. Build/upload AVR firmware: Select environment `avr128db28` → Build/Upload.
5. Build/upload ESP32 firmware: Select environment `esp32dev` → Build/Upload.
6. Configure Wi-Fi/MQTT in ESP32 code (or via web portal if implemented).
7. Calibrate sensors using provided Python tools.

See `avr_firmware/src/` and `esp32_firmware/src/` for source code.  
Pin definitions and calibration constants are in `include/config.h`.

## Safety Warnings

- **240 V AC involved** — lethal voltages present. Only qualified persons should wire high-voltage parts.
- Follow local electrical codes.
- Use isolated CTs and proper fusing.
- Test low-voltage sections first.

## License

MIT License (see LICENSE file if added).

Questions or contributions welcome!