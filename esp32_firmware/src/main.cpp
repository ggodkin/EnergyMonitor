// EnergyMonitor - ESP32 firmware
// Receives data from AVR via UART, displays on OLED, publishes to MQTT
// Configurable via captive portal (WiFi + MQTT credentials)
// Enters AP mode on WiFi failure or >10 MQTT connection failures
// Stays in AP mode until configuration is submitted

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// UART from AVR
#define UART_RX_PIN 16
#define UART_TX_PIN 17
HardwareSerial SerialAVR(2);

// Config constants
const unsigned long WIFI_TIMEOUT_MS    = 120000UL;   // 2 minutes
const int           MAX_MQTT_FAILS     = 10;

// Global configuration variables
Preferences prefs;
String wifi_ssid;
String wifi_password;
String mqtt_server;
int    mqtt_port       = 1883;
String mqtt_username;
String mqtt_password;
String mqtt_client_id  = "EnergyMonitor-ESP32";
String mqtt_topic_base = "home/energy/hottub/";

// Runtime state
WiFiClient   espClient;
PubSubClient mqttClient(espClient);

String uartBuffer      = "";
bool   inConfigMode    = false;
int    mqttFailCounter = 0;

unsigned long wifiConnectAttemptTime = 0;

// Captive portal components
const byte DNS_PORT = 53;
DNSServer   dnsServer;
AsyncWebServer server(80);

// ────────────────────────────────────────────────
// OLED address auto-detection
// ────────────────────────────────────────────────
uint8_t findOLEDAddress() {
  Serial.println("Scanning I2C for SSD1306...");
  uint8_t candidates[] = {0x3C, 0x3D};
  for (uint8_t addr : candidates) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("OLED found at address 0x%02X\n", addr);
      return addr;
    }
  }
  Serial.println("No OLED found → using default 0x3C");
  return 0x3C;
}

// ────────────────────────────────────────────────
// Load saved settings from NVS
// ────────────────────────────────────────────────
void loadSettings() {
  prefs.begin("emon-cfg", false);
  wifi_ssid       = prefs.getString("ssid", "");
  wifi_password   = prefs.getString("pass", "");
  mqtt_server     = prefs.getString("mqtthost", "192.168.1.100");
  mqtt_port       = prefs.getInt("mqttport", 1883);
  mqtt_username   = prefs.getString("mqttuser", "");
  mqtt_password   = prefs.getString("mqttpass", "");
  mqtt_client_id  = prefs.getString("mqttid",   "EnergyMonitor-ESP32");
  mqtt_topic_base = prefs.getString("mqttbase", "home/energy/hottub/");
  prefs.end();

  Serial.println("Loaded settings:");
  Serial.printf("  WiFi SSID   : %s\n", wifi_ssid.c_str());
  Serial.printf("  MQTT Server : %s:%d\n", mqtt_server.c_str(), mqtt_port);
  Serial.printf("  MQTT User   : %s\n", mqtt_username.c_str());
  Serial.printf("  Topic base  : %s\n", mqtt_topic_base.c_str());
}

// ────────────────────────────────────────────────
// Save settings and restart
// ────────────────────────────────────────────────
void saveSettings(const String &ssid, const String &pass,
                  const String &mqtth, int mqttp,
                  const String &user, const String &pw,
                  const String &id, const String &base) {
  if (ssid.length() == 0 || mqtth.length() == 0) {
    Serial.println("Incomplete config → not saving");
    return;
  }

  prefs.begin("emon-cfg", false);
  prefs.putString("ssid",     ssid);
  prefs.putString("pass",     pass);
  prefs.putString("mqtthost", mqtth);
  prefs.putInt   ("mqttport", mqttp);
  prefs.putString("mqttuser", user);
  prefs.putString("mqttpass", pw);
  prefs.putString("mqttid",   id);
  prefs.putString("mqttbase", base);
  prefs.end();

  Serial.println("Settings saved. Restarting in 2 seconds...");
  delay(2000);
  ESP.restart();
}

// ────────────────────────────────────────────────
// HTML configuration page
// ────────────────────────────────────────────────
const char configPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>EnergyMonitor Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {font-family:Arial; margin:20px; background:#f8f9fa;}
    h2 {color:#343a40;}
    label {display:block; margin:12px 0 4px; font-weight:bold;}
    input {width:100%; padding:10px; margin-bottom:8px; border:1px solid #ced4da; border-radius:4px; box-sizing:border-box;}
    input[type=submit] {background:#28a745; color:white; border:none; padding:12px; font-size:16px; cursor:pointer; border-radius:4px;}
    input[type=submit]:hover {background:#218838;}
  </style>
</head>
<body>
  <h2>EnergyMonitor Configuration</h2>
  <form action="/save">
    <label>WiFi SSID:</label><input type="text" name="ssid" required><br>
    <label>WiFi Password:</label><input type="password" name="pass"><br><br>
    <label>MQTT Server:</label><input type="text" name="mqtthost" value="192.168.1.100" required><br>
    <label>MQTT Port:</label><input type="number" name="mqttport" value="1883"><br>
    <label>MQTT Username (optional):</label><input type="text" name="mqttuser"><br>
    <label>MQTT Password (optional):</label><input type="password" name="mqttpass"><br>
    <label>MQTT Client ID:</label><input type="text" name="mqttid" value="EnergyMonitor-ESP32"><br>
    <label>MQTT Topic Base:</label><input type="text" name="mqttbase" value="home/energy/hottub/"><br>
    <input type="submit" value="Save & Restart">
  </form>
</body>
</html>
)rawliteral";

// ────────────────────────────────────────────────
// Start AP + captive portal
// ────────────────────────────────────────────────
void startConfigPortal() {
  if (inAPMode) return;
  inAPMode = true;
  mqttFailCounter = 0;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP("EnergyMonitor-Setup", "");   // ← add password if wanted

  Serial.println("Config portal active");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", configPage);
  });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!req->hasParam("ssid") || !req->hasParam("mqtthost") ||
        req->getParam("ssid")->value().length() == 0 ||
        req->getParam("mqtthost")->value().length() == 0) {
      req->send(400, "text/plain", "Missing required fields");
      return;
    }

    String ssid     = req->getParam("ssid")->value();
    String pass     = req->hasParam("pass")     ? req->getParam("pass")->value()     : "";
    String host     = req->getParam("mqtthost")->value();
    int    port     = req->hasParam("mqttport") ? req->getParam("mqttport")->value().toInt() : 1883;
    String user     = req->hasParam("mqttuser") ? req->getParam("mqttuser")->value() : "";
    String pw       = req->hasParam("mqttpass") ? req->getParam("mqttpass")->value() : "";
    String id       = req->hasParam("mqttid")   ? req->getParam("mqttid")->value()   : "EnergyMonitor-ESP32";
    String base     = req->hasParam("mqttbase") ? req->getParam("mqttbase")->value() : "home/energy/hottub/";

    saveSettings(ssid, pass, host, port, user, pw, id, base);

    req->send(200, "text/html", "<h2>Settings saved!<br>Device restarting...</h2>");
  });

  server.onNotFound([](AsyncWebServerRequest *req){
    req->redirect("/");
  });

  server.begin();
  Serial.println("Web server started");
}

// ────────────────────────────────────────────────
// MQTT reconnect
// ────────────────────────────────────────────────
bool tryConnectMQTT() {
  bool ok = false;

  if (mqtt_username.length() > 0 && mqtt_password.length() > 0) {
    ok = mqttClient.connect(mqtt_client_id.c_str(), mqtt_username.c_str(), mqtt_password.c_str());
  } else {
    ok = mqttClient.connect(mqtt_client_id.c_str());
  }

  if (ok) {
    mqttFailCounter = 0;
    Serial.println("MQTT connected");
    return true;
  }

  mqttFailCounter++;
  Serial.printf("MQTT connect failed (%d/%d) rc=%d\n", mqttFailCounter, MAX_MQTT_FAILS, mqttClient.state());

  if (mqttFailCounter >= MAX_MQTT_FAILS) {
    Serial.println("Too many MQTT failures → entering config mode");
    startConfigPortal();
  }

  return false;
}

// ────────────────────────────────────────────────
// Parse data from AVR
// ────────────────────────────────────────────────
void parseAVRData(String line) {
  if (!line.startsWith("DATA:")) return;
  line = line.substring(5);

  int pos[6] = {0};
  int start = 0;
  for (int i = 0; i < 5; i++) {
    pos[i] = line.indexOf(',', start);
    if (pos[i] == -1) return;
    start = pos[i] + 1;
  }
  pos[5] = line.length();

  numSensors = line.substring(0, pos[0]).toInt();
  voltage    = line.substring(pos[0]+1, pos[1]).toFloat();
  curr1      = line.substring(pos[1]+1, pos[2]).toFloat();
  pow1       = line.substring(pos[2]+1, pos[3]).toFloat();
  curr2      = line.substring(pos[3]+1, pos[4]).toFloat();
  pow2       = line.substring(pos[4]+1, pos[5]).toFloat();

  Serial.printf("Parsed → V=%.1f I1=%.3f P1=%.0f I2=%.3f P2=%.0f\n",
                voltage, curr1, pow1, curr2, pow2);

  if (mqttClient.connected()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", voltage); mqttClient.publish((mqtt_topic_base + "voltage").c_str(), buf);
    snprintf(buf, sizeof(buf), "%.3f", curr1);   mqttClient.publish((mqtt_topic_base + "current1").c_str(), buf);
    snprintf(buf, sizeof(buf), "%.0f", pow1);    mqttClient.publish((mqtt_topic_base + "power1").c_str(), buf);
    if (numSensors >= 2) {
      snprintf(buf, sizeof(buf), "%.3f", curr2); mqttClient.publish((mqtt_topic_base + "current2").c_str(), buf);
      snprintf(buf, sizeof(buf), "%.0f", pow2);  mqttClient.publish((mqtt_topic_base + "power2").c_str(), buf);
    }
  }
}

// ────────────────────────────────────────────────
// Update OLED screen
// ────────────────────────────────────────────────
void refreshDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (inConfigMode) {
    display.setTextSize(2);
    display.println("CONFIG");
    display.setTextSize(1);
    display.println("Connect to:");
    display.println("EnergyMonitor-Setup");
    display.println("Open browser:");
    display.println("192.168.4.1");
  } else {
    display.printf("V: %.1f V\n", voltage);
    display.printf("I1: %.3f A\n", curr1);
    display.printf("P1: %.0f W\n", pow1);
    if (numSensors >= 2) {
      display.printf("I2: %.3f A\n", curr2);
      display.printf("P2: %.0f W\n", pow2);
    }
    display.setCursor(0, 48);
    if (mqttFailCounter > 0) {
      display.printf("MQTT fail %d/%d", mqttFailCounter, MAX_MQTT_FAILS);
    } else if (mqttClient.connected()) {
      display.println("MQTT OK");
    } else {
      display.println("MQTT disconnected");
    }
  }
  display.display();
}

// ────────────────────────────────────────────────
// Setup
// ────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nEnergyMonitor ESP32 starting...");

  SerialAVR.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  Wire.begin();
  uint8_t oledAddr = findOLEDAddress();
  if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddr)) {
    Serial.println("SSD1306 init failed");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("EnergyMonitor");
  display.println("Starting...");
  display.display();

  loadSettings();

  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);

  if (wifi_ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    wifiConnectAttemptTime = millis();
    Serial.println("Connecting to saved WiFi...");
  } else {
    startConfigPortal();
  }
}

// ────────────────────────────────────────────────
// Loop
// ────────────────────────────────────────────────
void loop() {
  if (inConfigMode) {
    dnsServer.processNextRequest();
  } else {
    // Monitor WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiConnectAttemptTime > 0 && millis() - wifiConnectAttemptTime > WIFI_TIMEOUT_MS) {
        Serial.println("WiFi timeout → starting config portal");
        startConfigPortal();
      }
    } else {
      // Monitor MQTT
      if (!mqttClient.connected()) {
        static unsigned long lastMqttTry = 0;
        if (millis() - lastMqttTry > 5000) {
          lastMqttTry = millis();
          tryConnectMQTT();
        }
      } else {
        mqttClient.loop();
      }
    }

    // Read from AVR
    while (SerialAVR.available()) {
      char c = SerialAVR.read();
      if (c == '\n') {
        parseAVRData(uartBuffer);
        uartBuffer = "";
      } else {
        uartBuffer += c;
      }
    }
  }

  refreshDisplay();
  delay(50);
}