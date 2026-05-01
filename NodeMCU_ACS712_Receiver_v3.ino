/*
  NodeMCU ESP8266 — ACS712 Data Receiver + ThingSpeak Upload
  v3 — Added ThingSpeak cloud upload

  Packet Protocol:
    RMS packet:   [0xAA][MSB][LSB][0x55]  → RMS current  (A ×100)
    Peak packet:  [0xBB][MSB][LSB][0x66]  → Peak current (A ×100)
    Power packet: [0xCC][MSB][LSB][0x77]  → Apparent power (VA ×100)

  ThingSpeak Fields:
    Field 1 → RMS Current (A)
    Field 2 → Peak Current (A)
    Field 3 → Apparent Power (VA)

  Wiring:
    STM32 PB10 (TX3) → NodeMCU D6 (GPIO12)
    STM32 GND        → NodeMCU GND

  Libraries needed:
    - ThingSpeak  (by MathWorks)
    - ESP8266WiFi (built-in with ESP8266 core)
*/

#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>

// ── WiFi Credentials ─────────────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"        // ← Replace with your WiFi name
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"    // ← Replace with your WiFi password

// ── ThingSpeak Credentials ────────────────────────────────────────
#define TS_CHANNEL_ID       0                        // ← Replace with your Channel ID
#define TS_WRITE_API_KEY    "YOUR_WRITE_API_KEY"     // ← Replace with your Write API Key

// ── ThingSpeak free tier minimum update interval = 15 seconds ────
#define TS_UPLOAD_INTERVAL  20000

// ── SoftwareSerial ────────────────────────────────────────────────
SoftwareSerial stm32Serial(12, 5);   // D6=RX (GPIO12), D1=TX (GPIO5)

// ── Packet definitions ────────────────────────────────────────────
#define PKT_RMS_HDR     0xAA
#define PKT_RMS_FTR     0x55
#define PKT_PEAK_HDR    0xBB
#define PKT_PEAK_FTR    0x66
#define PKT_POWER_HDR   0xCC
#define PKT_POWER_FTR   0x77

// ── Parser ────────────────────────────────────────────────────────
enum ParserState { WAIT_START, READ_HIGH, READ_LOW, READ_END };
ParserState state    = WAIT_START;
uint8_t     hdr      = 0;
uint8_t     highByte = 0;
uint8_t     lowByte  = 0;

// ── Received values ───────────────────────────────────────────────
float rms   = 0.0f;
float peak  = 0.0f;
float power = 0.0f;

bool gotRMS   = false;
bool gotPeak  = false;
bool gotPower = false;

// ── Debug counters ────────────────────────────────────────────────
uint32_t totalBytes     = 0;
uint32_t validPackets   = 0;
uint32_t invalidPackets = 0;

WiFiClient wifiClient;

// ── WiFi Connect ─────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

// ── Packet Parser ─────────────────────────────────────────────────
void processByte(uint8_t b) {
  totalBytes++;

  switch (state) {
    case WAIT_START:
      if (b == PKT_RMS_HDR || b == PKT_PEAK_HDR || b == PKT_POWER_HDR) {
        hdr   = b;
        state = READ_HIGH;
      }
      break;

    case READ_HIGH:
      highByte = b;
      state    = READ_LOW;
      break;

    case READ_LOW:
      lowByte = b;
      state   = READ_END;
      break;

    case READ_END: {
      bool valid = false;
      if (hdr == PKT_RMS_HDR   && b == PKT_RMS_FTR)   valid = true;
      if (hdr == PKT_PEAK_HDR  && b == PKT_PEAK_FTR)  valid = true;
      if (hdr == PKT_POWER_HDR && b == PKT_POWER_FTR) valid = true;

      if (valid) {
        validPackets++;
        float value = ((uint16_t)highByte << 8 | lowByte) / 100.0f;
        if      (hdr == PKT_RMS_HDR)   { rms   = value; gotRMS   = true; }
        else if (hdr == PKT_PEAK_HDR)  { peak  = value; gotPeak  = true; }
        else if (hdr == PKT_POWER_HDR) { power = value; gotPower = true; }
      } else {
        invalidPackets++;
      }
      state = WAIT_START;
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  stm32Serial.begin(9600);

  Serial.println("\n=== NodeMCU ACS712 Receiver v3 ===");

  connectWiFi();
  ThingSpeak.begin(wifiClient);

  Serial.println("Waiting for STM32 data...");
  Serial.println("──────────────────────────────────────");
}

void loop() {
  // ── Read UART from STM32 ────────────────────────────────────
  while (stm32Serial.available()) {
    processByte((uint8_t)stm32Serial.read());
  }

  // ── Print to Serial Monitor when full set received ──────────
  if (gotRMS && gotPeak && gotPower) {
    gotRMS   = false;
    gotPeak  = false;
    gotPower = false;

    Serial.print("RMS: ");   Serial.print(rms,   3); Serial.print(" A  |  ");
    Serial.print("Peak: ");  Serial.print(peak,  3); Serial.print(" A  |  ");
    Serial.print("Power: "); Serial.print(power, 2); Serial.println(" VA");
  }

  // ── Upload to ThingSpeak every 15 seconds ───────────────────
  static uint32_t lastUpload = 0;
  if (millis() - lastUpload >= TS_UPLOAD_INTERVAL) {
    lastUpload = millis();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      connectWiFi();
    }

    ThingSpeak.setField(1, rms);
    ThingSpeak.setField(2, peak);
    ThingSpeak.setField(3, power);

    int result = ThingSpeak.writeFields(TS_CHANNEL_ID, TS_WRITE_API_KEY);
    if (result == 200) {
      Serial.println("ThingSpeak upload OK");
    } else {
      Serial.print("ThingSpeak upload failed, code: ");
      Serial.println(result);
    }
  }

  // ── Diagnostics every 10 seconds ────────────────────────────
  static uint32_t lastDiag = 0;
  if (millis() - lastDiag >= 10000) {
    lastDiag = millis();
    Serial.print("[Diag] Bytes: ");   Serial.print(totalBytes);
    Serial.print(" | Valid pkts: "); Serial.print(validPackets);
    Serial.print(" | Invalid: ");    Serial.print(invalidPackets);
    Serial.print(" | WiFi: ");       Serial.println(WiFi.status() == WL_CONNECTED ? "OK" : "LOST");
  }
}
