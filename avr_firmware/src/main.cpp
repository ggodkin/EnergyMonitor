// ========================================================
// EnergyMonitor - AVR128DB28 Firmware
// Full version with differential ADC + op-amp biasing
// ========================================================

#include <Arduino.h>

// ==================== PIN DEFINITIONS (match README.md) ====================
#define UART_TX_PIN         PIN_PD1     // pin 7  → ESP32 RX (GPIO16)
#define UART_RX_PIN         PIN_PA4     // pin 25 → ESP32 TX (GPIO17)

#define SCT1_POS_PIN        PIN_PA3     // pin 24 - SCT #1 positive (diff)
#define SCT1_NEG_PIN        PIN_PD2     // pin 8  - SCT #1 negative (bias ref)

#define SCT2_POS_PIN        PIN_PA2     // pin 23 - SCT #2 positive
#define SCT2_NEG_PIN        PIN_PD3     // pin 9

#define SCT3_POS_PIN        PIN_PA5     // pin 26 - SCT #3 positive
#define SCT3_NEG_PIN        PIN_PD4     // pin 10

#define VOLTAGE_POS_PIN     PIN_PA6     // pin 27 - ZMPT101B positive
#define VOLTAGE_NEG_PIN     PIN_PD5     // pin 11 - bias reference

#define LEAKAGE_PIN         PIN_PF6     // pin 18 - optional single-ended leakage CT

#define DEBUG_LED_PIN       PIN_PA7     // pin 1

// ==================== CONFIGURATION ====================
const uint16_t SAMPLES_PER_CYCLE = 80;      // ~1.3 ms at 60 Hz
const uint8_t  CYCLES_TO_AVERAGE   = 8;     // 8 cycles ≈ 133 ms for stable RMS
const float    VREF                = 2.048; // Internal reference (best choice)
const float    BIAS_VOLTAGE        = 1.024; // VREF/2 - op-amp will create this

// ==================== GLOBALS ====================
float voltageRMS = 0;
float currentRMS[3] = {0};   // SCT1, SCT2, SCT3
float leakageRMS = 0;
float realPower[3] = {0};

// Simple UART packet format sent to ESP32 every ~500 ms
void sendDataPacket() {
  Serial.print("EM:");                     // EnergyMonitor header
  Serial.print(voltageRMS, 1); Serial.print(",");
  Serial.print(currentRMS[0], 3); Serial.print(",");
  Serial.print(currentRMS[1], 3); Serial.print(",");
  Serial.print(currentRMS[2], 3); Serial.print(",");
  Serial.print(leakageRMS, 3); Serial.print(",");
  Serial.print(realPower[0], 0); Serial.print(",");
  Serial.print(realPower[1], 0); Serial.print(",");
  Serial.print(realPower[2], 0);
  Serial.println();
}

// ==================== HELPER: Single Differential RMS Reading ====================
float readDifferentialRMS(uint8_t posPin, uint8_t negPin) {
  // Set up this channel
  ADC0.MUXPOS = (posPin == PIN_PA3) ? ADC_MUXPOS_AIN3_gc :
                (posPin == PIN_PA2) ? ADC_MUXPOS_AIN2_gc :
                (posPin == PIN_PA5) ? ADC_MUXPOS_AIN5_gc :
                (posPin == PIN_PA6) ? ADC_MUXPOS_AIN6_gc : ADC_MUXPOS_AIN3_gc;

  ADC0.MUXNEG = (negPin == PIN_PD2) ? ADC_MUXNEG_AIN2_gc :
                (negPin == PIN_PD3) ? ADC_MUXNEG_AIN3_gc :
                (negPin == PIN_PD4) ? ADC_MUXNEG_AIN4_gc :
                (negPin == PIN_PD5) ? ADC_MUXNEG_AIN5_gc : ADC_MUXNEG_AIN2_gc;

  long sumSquares = 0;
  for (uint8_t cycle = 0; cycle < CYCLES_TO_AVERAGE; cycle++) {
    for (uint16_t i = 0; i < SAMPLES_PER_CYCLE; i++) {
      ADC0.COMMAND = ADC_STCONV_bm;                 // Start conversion
      while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));     // Wait for result
      int16_t raw = ADC0.RESULT;                    // signed 12-bit
      sumSquares += (long)raw * raw;
    }
  }

  float rms = sqrt((float)sumSquares / (SAMPLES_PER_CYCLE * CYCLES_TO_AVERAGE));
  return rms * (VREF / 2048.0);                     // convert to volts RMS
}

// ==================== SETUP ====================
void setup() {
  pinMode(DEBUG_LED_PIN, OUTPUT);
  digitalWrite(DEBUG_LED_PIN, HIGH);

  Serial.begin(57600);        // UART to ESP32 (must match ESP32 side)
  Serial.println("AVR EnergyMonitor v2.0 - Differential ADC ready");

  // === Configure internal 2.048V reference ===
  VREF.CTRLA = VREF_ADC0REFSEL_2V048_gc;

  // === Enable ADC0 in differential mode ===
  ADC0.CTRLA = ADC_ENABLE_bm | ADC_RUNSTBY_bm;
  ADC0.CTRLC = ADC_REFSEL_INTREF_gc | ADC_PRESC_DIV16_gc;  // ~1.5 MHz ADC clock

  // === Configure OpAmps for clean biasing (recommended for SCTs) ===
  // OPA0: unity-gain buffer for VREF/2 bias on negative inputs
  OPA0.CTRLA = OPA_ENABLE_bm | OPA_OUTEN_bm;
  OPA0.MUXCTRL = OPA_MUXPOS_VREF_gc | OPA_MUXNEG_OUT_gc;   // VREF/2 → output

  // You can add more op-amps (OPA1/OPA2) for gain/filtering if needed

  digitalWrite(DEBUG_LED_PIN, LOW);
  Serial.println("Initialization complete");
}

// ==================== LOOP ====================
void loop() {
  digitalWrite(DEBUG_LED_PIN, !digitalRead(DEBUG_LED_PIN));  // heartbeat

  // Read all channels
  voltageRMS   = readDifferentialRMS(VOLTAGE_POS_PIN, VOLTAGE_NEG_PIN) * 110.0f;  // rough scaling to ~240V
  currentRMS[0] = readDifferentialRMS(SCT1_POS_PIN, SCT1_NEG_PIN) * 30.0f;       // calibrate these!
  currentRMS[1] = readDifferentialRMS(SCT2_POS_PIN, SCT2_NEG_PIN) * 30.0f;
  currentRMS[2] = readDifferentialRMS(SCT3_POS_PIN, SCT3_NEG_PIN) * 30.0f;

  // Optional leakage (single-ended for simplicity)
  leakageRMS = analogRead(LEAKAGE_PIN) * (VREF / 4095.0) * 5.0f;  // adjust scaling

  // Simple apparent power (add phase correction later for real power)
  realPower[0] = voltageRMS * currentRMS[0];
  realPower[1] = voltageRMS * currentRMS[1];
  realPower[2] = voltageRMS * currentRMS[2];

  sendDataPacket();

  delay(500);   // 2 packets per second - adjust as needed
}