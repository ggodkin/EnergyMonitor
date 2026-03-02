// EnergyMonitor - ESP32 TEST BUILD (minimal)
// Compile test to confirm function order fix

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define UART_RX_PIN 16
#define UART_TX_PIN 17
HardwareSerial SerialAVR(2);

String receivedData = "";
uint8_t numSensors = 0;
float voltage = 0.0f;

// ────────────────────────────────────────────────
// All helper functions FIRST
// ────────────────────────────────────────────────

void processData(String data) {
  Serial.println("processData called with: " + data);
  // Dummy parse - just for testing
  numSensors = random(1, 5);
  voltage = random(220, 250) + random(0, 99) / 100.0;
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("TEST BUILD");
  display.printf("Sensors: %d\n", numSensors);
  display.printf("V: %.1f V\n", voltage);
  display.display();
}

// ────────────────────────────────────────────────
// Arduino entry points LAST
// ────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  SerialAVR.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("EnergyMonitor");
  display.println("TEST MODE");
  display.display();

  Serial.println("Setup complete - waiting for UART data");
}

void loop() {
  if (SerialAVR.available()) {
    char c = SerialAVR.read();
    if (c == '\n') {
      processData(receivedData);
      receivedData = "";
    } else {
      receivedData += c;
    }
  }

  updateDisplay();
  delay(500);  // slower for easier observation
}