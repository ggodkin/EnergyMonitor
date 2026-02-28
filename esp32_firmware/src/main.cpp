// EnergyMonitor - ESP32 firmware
// Receives data from AVR via UART, shows on OLED, publishes MQTT

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>     // MQTT

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// UART from AVR (use Serial2 on most ESP32)
#define UART_RX_PIN 16
#define UART_TX_PIN 17
HardwareSerial SerialAVR(2);

// WiFi & MQTT - fill in your details!
const char* ssid = "your-ssid";
const char* password = "your-pass";
const char* mqtt_server = "your-mqtt-broker-ip";  // e.g., homeassistant.local
WiFiClient espClient;
PubSubClient client(espClient);

String receivedData = "";
uint8_t numSensors = 0;
float voltage = 0, curr1 = 0, pow1 = 0, curr2 = 0, pow2 = 0;

void setup() {
  Serial.begin(115200);               // Debug
  SerialAVR.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("EnergyMonitor");
  display.display();

  // WiFi & MQTT connect (add your code here)
  // WiFi.begin(ssid, password);
  // client.setServer(mqtt_server, 1883);
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
  // client.loop();  // MQTT
  delay(100);
}

void processData(String data) {
  if (data.startsWith("DATA:")) {
    data = data.substring(5);
    int comma1 = data.indexOf(',');
    numSensors = data.substring(0, comma1).toInt();

    int comma2 = data.indexOf(',', comma1+1);
    voltage = data.substring(comma1+1, comma2).toFloat();

    int comma3 = data.indexOf(',', comma2+1);
    curr1 = data.substring(comma2+1, comma3).toFloat();

    int comma4 = data.indexOf(',', comma3+1);
    pow1 = data.substring(comma3+1, comma4).toFloat();

    int comma5 = data.indexOf(',', comma4+1);
    curr2 = data.substring(comma4+1, comma5).toFloat();

    pow2 = data.substring(comma5+1).toFloat();

    Serial.printf("Parsed: V=%.1fV, I1=%.3fA, P1=%.0fW, I2=%.3fA, P2=%.0fW\n",
                  voltage, curr1, pow1, curr2, pow2);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print("Sensors: "); display.println(numSensors);
  display.print("V: "); display.print(voltage, 1); display.println(" V");
  display.print("I1: "); display.print(curr1, 3); display.println(" A");
  display.print("P1: "); display.print(pow1, 0); display.println(" W");
  if (numSensors >= 2) {
    display.print("I2: "); display.print(curr2, 3); display.println(" A");
    display.print("P2: "); display.print(pow2, 0); display.println(" W");
  }
  display.display();
}