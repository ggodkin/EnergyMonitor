# EnergyMonitor

Real-time power & energy monitoring system for a 240 V split-phase hot tub/spa (US residential wiring).

Uses a two-MCU architecture:
- **AVR128DB28** (28-pin): High-speed analog sampling of voltage + multiple current channels using 12-bit **differential ADC** and built-in op-amps.
- **ESP32**: Wi-Fi, MQTT to Home Assistant, small OLED display, OTA updates.

Supports multiple SCT-013 CTs (15/50/100 A variants), ZMPT101B voltage sensor, optional ground leakage monitoring, RMS/real/apparent power, power factor, and kWh accumulation.

## Features

- Multi-channel current monitoring (legs + optional leakage)
- Accurate split-phase power calculation
- Per-sensor calibration (stored in EEPROM)
- MQTT JSON publishing to Home Assistant
- OLED display on ESP32 (voltage/current/power/energy)
- OTA firmware updates
- Low-cost (~$30–50 excluding enclosure/sensors)

## Hardware Overview

- AVR side: Clean, high-speed analog front-end.
- ESP32 side: Connectivity, display, data handling.
- Communication: UART (57600 baud default) between MCUs.
- Recommended supply: **3.3 V** shared rail (AVR DB series works well at 3.3 V; matches ESP32 logic).

**Important Electrical Notes**
- Use **3.3 V** for AVR128DB28 (VDD/AVDD) to avoid level shifters on UART.
- If 5 V on AVR: Add bidirectional level shifters (e.g., TXS0102) on UART lines to protect ESP32 GPIOs.
- **Shared GND** between AVR and ESP32 is required for UART.
- Decoupling: 0.1 µF + 10 µF caps near each MCU VCC/GND.
- **UPDI programming**: PA0 (pin 19) — keep accessible (header recommended).

## Pin Assignments

### AVR128DB28 (28-pin SSOP/SOIC/SPDIP)

Based on Microchip datasheet DS40002247A. Analog pins chosen for ADC compatibility; negative differential inputs limited to PORTD pins.

| Physical Pin | AVR Name | Function                              | Notes                                      |
|--------------|----------|---------------------------------------|--------------------------------------------|
| 19           | PA0      | UPDI (programming)                    | **Reserved** – do not connect sensors      |
| 7            | PD1      | UART TX → ESP32 RX                    | USART0 TX (default)                        |
| 25           | PA4      | UART RX ← ESP32 TX                    | USART0 RX (default)                        |
| 24           | PA3      | SCT #1 (Current Leg 1) – ADC+         | Positive diff input; OpAmp accessible      |
| 8            | PD2      | SCT #1 – ADC- (differential negative) | Negative diff input (PORTD required)       |
| 23           | PA2      | SCT #2 (Current Leg 2) – ADC+         | Positive diff input                        |
| 9            | PD3      | SCT #2 – ADC-                         | Negative diff input                        |
| 26           | PA5      | SCT #3 (Current Leg 3) – ADC+         | Positive diff input                        |
| 10           | PD4      | SCT #3 – ADC-                         | Negative diff input                        |
| 18           | PF6      | Optional leakage CT – single-ended    | ADC-capable (positive only)                |
| 27           | PA6      | ZMPT101B voltage sense – ADC+         | Preferred for OpAmp biasing                |
| 11           | PD5      | ZMPT101B – ADC- (differential ref)    | Use Vref/2 or clean midpoint               |
| 1            | PA7      | Debug LED / spare                     | PWM-capable                                |
| 12           | PD6      | Spare / extra CT possible             | ADC-capable                                |

**Power pins (do NOT use for signals):**
- Pin 6: VDDIO2
- Pin 14: AVDD
- Pin 20: VDD
- Pin 15, 21: GND

### ESP32 (DevKit-style)

| GPIO | Function                  | Notes                              |
|------|---------------------------|------------------------------------|
| 21   | I²C SDA (OLED)            | Default Arduino-ESP32 I²C          |
| 22   | I²C SCL (OLED)            | Default; add pull-ups if needed    |
| 16   | UART RX ← AVR TX          | UART2 – safe, no boot issues       |
| 17   | UART TX → AVR RX          | UART2 – safe                       |
| 4,5  | Spares / LED              | General purpose                    |

**UART Wiring (crossover – required!)**  
- AVR TX (PD1) → ESP32 RX (GPIO16)  
- AVR RX (PA4) → ESP32 TX (GPIO17)  
- **Not** TX-to-TX or RX-to-RX — that won't work.

## Analog Input Recommendations

Use **differential ADC mode** for SCT-013 channels:

- Bias AC signal to Vref/2 (~1.65 V at 3.3 V supply) using op-amp or resistor divider.
- Connect biased CT output to **positive ADC input** (MUXPOS).
- Connect bias reference (Vref/2) to **negative ADC input** (MUXNEG – PORTD only).
- Benefits: Full range usage, common-mode noise rejection, better accuracy at low currents.

Example code snippet (DxCore style):
```cpp
ADC0.MUXPOS  = ADC_MUXPOS_AIN3_gc;   // PA3 positive
ADC0.MUXNEG  = ADC_MUXNEG_AIN2_gc;   // PD2 negative
ADC0.CTRLC   = ADC_REFSEL_VDD_gc | ADC_PRESC_DIV8_gc;
ADC0.CTRLA  |= ADC_ENABLE_bm | ADC_DIFFMODE_bm;