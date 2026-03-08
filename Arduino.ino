#include <ESP8266WiFi.h>
#include <ThingSpeak.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------------ OLED ------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ------------ ULTRASONIC ------------
#define TRIG_PIN D5
#define ECHO_PIN D6
#define BIN_HEIGHT_CM 30.0

// ------------ WIFI / THINGSPEAK ------------
const char* WIFI_SSID = "Has";
const char* WIFI_PASS = "00000000";

unsigned long CHANNEL_ID = 3278017;           // e.g. 2456789
const char* WRITE_API_KEY = "XKZZJVZJ4YEEEYVB";     // e.g. "ABCD1234...."

WiFiClient client;

// ThingSpeak free limit: >= 15s
const unsigned long UPLOAD_INTERVAL_MS = 20000;
unsigned long lastUploadMs = 0;

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return -1;
  return (duration * 0.0343f) / 2.0f;
}

int statusFromFill(float fillPercent, String &statusText) {
  if (fillPercent < 30) { statusText = "EMPTY"; return 1; }
  if (fillPercent < 80) { statusText = "MEDIUM"; return 2; }
  if (fillPercent < 95) { statusText = "ALERT"; return 3; }
  statusText = "FULL"; return 4;
}

void showOLED(float distance, float fillPercent, const String& status, bool wifiOk) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.print("Dist: ");
  if (distance < 0) display.println("ERR");
  else { display.print(distance, 1); display.println(" cm"); }

  display.print("Fill: ");
  display.print(fillPercent, 0);
  display.println("%");

  display.print("Status: ");
  display.println(status);

  display.print("WiFi: ");
  display.println(wifiOk ? "OK" : "DOWN");

  display.display();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(300);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(D2, D1);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  connectWiFi();
  ThingSpeak.begin(client);

  Serial.println("Setup done.");
  Serial.print("WiFi status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "NOT CONNECTED");
}

void loop() {
  float distance = readDistanceCm();
  float fillPercent = 0;

  if (distance >= 0) {
    fillPercent = ((BIN_HEIGHT_CM - distance) / BIN_HEIGHT_CM) * 100.0f;
    if (fillPercent < 0) fillPercent = 0;
    if (fillPercent > 100) fillPercent = 100;
  }

  String status;
  int statusCode = statusFromFill(fillPercent, status);

  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  if (!wifiOk) connectWiFi();

  showOLED(distance, fillPercent, status, WiFi.status() == WL_CONNECTED);

  // Upload to ThingSpeak every 20 seconds
  if (millis() - lastUploadMs >= UPLOAD_INTERVAL_MS) {
    lastUploadMs = millis();

    ThingSpeak.setField(1, (distance < 0) ? 0 : distance);
    ThingSpeak.setField(2, fillPercent);
    ThingSpeak.setField(3, statusCode);

    int code = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);

    Serial.print("ThingSpeak writeFields code: ");
    Serial.println(code); // 200 = OK
  }

  delay(1000);
}