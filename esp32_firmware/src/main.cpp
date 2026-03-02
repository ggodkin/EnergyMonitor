// EnergyMonitor - ESP32 firmware
// Receives data from AVR via UART, shows on OLED, publishes MQTT
// With auto-detect I2C address for SSD1306

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);  // We'll set address later

// UART from AVR
#define UART_RX_PIN 16
#define UART_TX_PIN 17
HardwareSerial SerialAVR(2);

// ── CHANGE THESE ──
const char* ssid          = "your-wifi-ssid";
const char* password      = "your-wifi-password";
const char* mqtt_server   = "192.168.1.100";
const int   mqtt_port     = 1883;
const char* mqtt_client_id = "EnergyMonitor-ESP32";
const char* mqtt_topic_base = "home/energy/hottub/";

WiFiClient espClient;
PubSubClient client(espClient);

String receivedData = "";
uint8_t numSensors = 0;
float voltage = 0.0f, curr1 = 0.0f, pow1 = 0.0f;
float curr2 = 0.0f, pow2 = 0.0f;

unsigned long lastReconnectAttempt = 0;

// ── Auto-detect I2C address ──
uint8_t detectOLEDAddress() {
  Serial.println("Scanning I2C bus for SSD1306 OLED...");

  // Common addresses first
  uint8_t candidates[] = {0x3C, 0x3D};
  for (uint8_t i = 0; i < sizeof(candidates); i++) {
    uint8_t addr = candidates[i];
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found OLED at 0x");
      Serial.println(addr, HEX);
      return addr;
    }
  }

  // Full scan if common ones not found
  Serial.println("Common addresses not found - scanning all...");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      return addr;  // Assume first found is OLED
    }
  }

  Serial.println("No I2C device found! Check wiring / pull-ups.");
  return 0x3C;  // Fallback to most common - code will likely fail gracefully
}

// ── Helper functions ──

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed.");
  }
}

bool reconnectMQTT() {
  Serial.print("MQTT connect...");
  String clientId = String(mqtt_client_id) + "-" + String(random(0xffff), HEX);
  if (client.connect(clientId.c_str())) {
    Serial.println("OK");
    return true;
  } else {
    Serial.print("failed (rc=");
    Serial.print(client.state());
    Serial.println(")");
    return false;
  }
}

void processData(String data) {
  if (!data.startsWith("DATA:")) return;
  data = data.substring(5);

  int idx[6] = {0};
  int pos = 0;
  for (int i = 0; i < 5; i++) {
    idx[i] = data.indexOf(',', pos);
    if (idx[i] == -1) return;
    pos = idx[i] + 1;
  }
  idx[5] = data.length();

  numSensors = data.substring(0, idx[0]).toInt();
  voltage    = data.substring(idx[0]+1, idx[1]).toFloat();
  curr1      = data.substring(idx[1]+1, idx[2]).toFloat();
  pow1       = data.substring(idx[2]+1, idx[3]).toFloat();
  curr2      = data.substring(idx[3]+1, idx[4]).toFloat();
  pow2       = data.substring(idx[4]+1, idx[5]).toFloat();

  Serial.printf("Parsed: V=%.1f I1=%.3f P1=%.0f I2=%.3f P2=%.0f\n",
                voltage, curr1, pow1, curr2, pow2);

  if (client.connected()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", voltage); client.publish((String(mqtt_topic_base) + "voltage").c_str(), buf);
    snprintf(buf, sizeof(buf), "%.3f", curr1);   client.publish((String(mqtt_topic_base) + "current1").c_str(), buf);
    snprintf(buf, sizeof(buf), "%.0f", pow1);    client.publish((String(mqtt_topic_base) + "power1").c_str(), buf);
    if (numSensors >= 2) {
      snprintf(buf, sizeof(buf), "%.3f", curr2); client.publish((String(mqtt_topic_base) + "current2").c_str(), buf);
      snprintf(buf, sizeof(buf), "%.0f", pow2);  client.publish((String(mqtt_topic_base) + "power2").c_str(), buf);
    }
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.printf("Sensors: %d\n", numSensors);
  display.printf("V: %.1f V\n", voltage);
  display.printf("I1: %.3f A\n", curr1);
  display.printf("P1: %.0f W\n", pow1);
  if (numSensors >= 2) {
    display.printf("I2: %.3f A\n", curr2);
    display.printf("P2: %.0f W\n", pow2);
  }
  display.setCursor(90, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "NoWiFi");
  display.setCursor(90, 10);
  display.print(client.connected() ? "MQTT" : "NoMQ");
  display.display();
}

// ── Main ──

void setup() {
  Serial.begin(115200);
  SerialAVR.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  Wire.begin();  // Default I2C pins (GPIO21 SDA, GPIO22 SCL on most ESP32)

  // Auto-detect address
  uint8_t oledAddr = detectOLEDAddress();

  // Init display with detected address
  if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddr)) {
    Serial.println(F("SSD1306 init failed - check wiring/address"));
    // Continue anyway - display may still work if fallback is correct
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("EnergyMonitor");
  display.print("OLED @ 0x");
  display.println(oledAddr, HEX);
  display.display();

  connectWiFi();
  client.setServer(mqtt_server, mqtt_port);
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

  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnectMQTT()) lastReconnectAttempt = 0;
    }
  } else {
    client.loop();
  }

  updateDisplay();
  delay(100);
}