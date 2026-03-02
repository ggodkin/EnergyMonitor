// EnergyMonitor - ESP32 firmware with WiFi fallback AP + web config

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

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define UART_RX_PIN 16
#define UART_TX_PIN 17
HardwareSerial SerialAVR(2);

Preferences prefs;

String wifi_ssid;
String wifi_pass;
String mqtt_server_str;
int    mqtt_port = 1883;
String mqtt_client_id = "EnergyMonitor-ESP32";
String mqtt_topic_base = "home/energy/hottub/";

WiFiClient espClient;
PubSubClient client(espClient);
String receivedData = "";
uint8_t numSensors = 0;
float voltage = 0.0f, curr1 = 0.0f, pow1 = 0.0f, curr2 = 0.0f, pow2 = 0.0f;

unsigned long wifiConnectStart = 0;
bool inAPMode = false;
const unsigned long AP_FALLBACK_TIMEOUT = 120000UL;

const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80);

uint8_t detectOLEDAddress() {
  Serial.println("Scanning I2C for OLED...");
  uint8_t candidates[] = {0x3C, 0x3D};
  for (auto addr : candidates) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) return addr;
  }
  return 0x3C;
}

void loadCredentials() {
  prefs.begin("energy-cfg", false);
  wifi_ssid = prefs.getString("ssid", "");
  wifi_pass = prefs.getString("pass", "");
  mqtt_server_str = prefs.getString("mqtt_srv", "192.168.1.100");
  mqtt_port = prefs.getInt("mqtt_port", 1883);
  mqtt_client_id = prefs.getString("mqtt_id", "EnergyMonitor-ESP32");
  prefs.end();
}

void saveCredentials(const String& ssid, const String& pass, const String& mqttSrv, int mqttPrt, const String& mqttId) {
  prefs.begin("energy-cfg", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("mqtt_srv", mqttSrv);
  prefs.putInt("mqtt_port", mqttPrt);
  prefs.putString("mqtt_id", mqttId);
  prefs.end();
  ESP.restart();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>EnergyMonitor Setup</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:Arial;margin:20px;}input{width:100%;padding:10px;margin:8px 0;}</style></head><body><h2>EnergyMonitor Config</h2><form action="/save"><label>WiFi SSID:</label><input type="text" name="ssid" required><br><label>WiFi Password:</label><input type="password" name="pass"><br><br><label>MQTT Server:</label><input type="text" name="mqtt_srv" value="192.168.1.100" required><br><label>MQTT Port:</label><input type="number" name="mqtt_prt" value="1883"><br><label>MQTT Client ID:</label><input type="text" name="mqtt_id" value="EnergyMonitor-ESP32"><br><input type="submit" value="Save & Restart"></form></body></html>
)rawliteral";

void startAPMode() {
  inAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP("EnergyMonitor-Setup", "");

  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request){
    String ssid = request->getParam("ssid")->value();
    String pass = request->getParam("pass")->value();
    String mqtt_srv = request->getParam("mqtt_srv")->value();
    int mqtt_prt = request->getParam("mqtt_prt")->value().toInt();
    String mqtt_id = request->getParam("mqtt_id")->value();
    if (ssid.length() > 0) saveCredentials(ssid, pass, mqtt_srv, mqtt_prt, mqtt_id);
    request->send(200, "text/html", "<h2>Saved! Restarting...</h2>");
  });

  // Redirect everything else to root
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.begin();
  Serial.println("Captive portal active");
}

bool reconnectMQTT() {
  Serial.print("MQTT connect...");
  if (client.connect(mqtt_client_id.c_str())) {
    Serial.println("OK");
    return true;
  }
  Serial.println("failed");
  return false;
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
  voltage = data.substring(idx[0]+1, idx[1]).toFloat();
  curr1 = data.substring(idx[1]+1, idx[2]).toFloat();
  pow1 = data.substring(idx[2]+1, idx[3]).toFloat();
  curr2 = data.substring(idx[3]+1, idx[4]).toFloat();
  pow2 = data.substring(idx[4]+1, idx[5]).toFloat();

  Serial.printf("Data: V=%.1f I1=%.3f P1=%.0f\n", voltage, curr1, pow1);

  if (client.connected()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", voltage); client.publish((mqtt_topic_base + "voltage").c_str(), buf);
    // add others as needed
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  if (inAPMode) {
    display.println("CONFIG MODE");
    display.println("Connect to:");
    display.println("EnergyMonitor-Setup");
    display.println("http://192.168.4.1");
  } else {
    display.printf("V: %.1f V\n", voltage);
    display.printf("I1: %.3f A\n", curr1);
    display.printf("P1: %.0f W\n", pow1);
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  SerialAVR.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  Wire.begin();
  uint8_t addr = detectOLEDAddress();
  if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) {
    Serial.println("OLED failed");
  }
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Starting...");
  display.display();

  loadCredentials();

  client.setServer(mqtt_server_str.c_str(), mqtt_port);

  if (wifi_ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    wifiConnectStart = millis();
  } else {
    startAPMode();
  }
}

void loop() {
  if (inAPMode) {
    dnsServer.processNextRequest();
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - wifiConnectStart > AP_FALLBACK_TIMEOUT) {
        startAPMode();
      }
    } else {
      if (!client.connected()) {
        static unsigned long last = 0;
        if (millis() - last > 5000) {
          last = millis();
          reconnectMQTT();
        }
      } else {
        client.loop();
      }
    }

    while (SerialAVR.available()) {
      char c = SerialAVR.read();
      if (c == '\n') {
        processData(receivedData);
        receivedData = "";
      } else {
        receivedData += c;
      }
    }
  }

  updateDisplay();
  delay(50);
}