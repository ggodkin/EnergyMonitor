// EnergyMonitor - AVR128DB28 firmware
// Samples ZMPT101B voltage + multiple SCT013 currents
// Uses on-chip OpAmps for biasing, sends RMS/power data over UART to ESP32

#include <Arduino.h>

// Pin definitions (match README.md suggestions)
#define VOLTAGE_PIN     PIN_PA5     // ZMPT101B -> ADC
#define CURRENT_PIN_1   PIN_PA3     // SCT013 #1
#define CURRENT_PIN_2   PIN_PD4     // SCT013 #2
// Add more: PIN_PD2, etc. up to ~9

// Config - adjust these!
const float V_REF = 2.5;          // Internal ref or external
const float BIAS_VOLTAGE = 1.65;  // Midpoint bias for AC
const uint16_t SAMPLES_PER_CYCLE = 100;  // ~1.6 ms at 60 Hz
const uint8_t NUM_SENSORS = 2;    // Start small, increase later

// Simple RMS calc function (expand with EmonLibCM later)
float calcRMS(uint8_t pin, uint16_t samples) {
  long sum = 0;
  for (uint16_t i = 0; i < samples; i++) {
    int raw = analogRead(pin);
    int centered = raw - (BIAS_VOLTAGE / V_REF * 4095 / 2);  // 12-bit ADC
    sum += (long)centered * centered;
    delayMicroseconds(100);  // Rough timing
  }
  float rms = sqrt((float)sum / samples);
  return rms * (V_REF / 4095.0) * 1000.0;  // mV RMS - calibrate!
}

void setup() {
  Serial.begin(115200);           // UART to ESP32
  analogReference(INTERNAL2V5);   // Or EXTERNAL if using ref pin
  analogReadResolution(12);

  // Configure OpAmps for biasing if needed (AVR DB specific)
  // OPA.CTRLA = OPA_ENABLE_bm | ... ; see DxCore docs

  Serial.println("AVR EnergyMonitor started - sending to ESP32");
}

void loop() {
  float v_rms = calcRMS(VOLTAGE_PIN, SAMPLES_PER_CYCLE) * 100.0;  // Rough scaling to ~240V

  float i_rms1 = calcRMS(CURRENT_PIN_1, SAMPLES_PER_CYCLE) * 30.0;  // Example calibration factor
  float i_rms2 = calcRMS(CURRENT_PIN_2, SAMPLES_PER_CYCLE) * 30.0;

  float power1 = v_rms * i_rms1;  // Apparent power (add PF later)
  float power2 = v_rms * i_rms2;

  // Simple CSV-like packet to ESP32
  Serial.print("DATA:");
  Serial.print(NUM_SENSORS); Serial.print(",");
  Serial.print(v_rms, 1); Serial.print(",");
  Serial.print(i_rms1, 3); Serial.print(",");
  Serial.print(power1, 0); Serial.print(",");
  Serial.print(i_rms2, 3); Serial.print(",");
  Serial.print(power2, 0);
  Serial.println();

  delay(2000);  // ~ every 2 seconds - adjust based on sampling rate
}