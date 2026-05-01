/*
  ACS712 Current Monitor — STM32F446RE Nucleo + I2C LCD
  Arduino IDE (STM32duino core)
  v4 — UART baud changed to 9600 for reliable ESP8266 SoftwareSerial

  Sends via UART (Serial3 → PB10) binary packets to NodeMCU:
    RMS packet:   [0xAA][MSB][LSB][0x55]
    Peak packet:  [0xBB][MSB][LSB][0x66]
    Power packet: [0xCC][MSB][LSB][0x77]

  Values scaled ×100 (e.g. 1.23 A → 123, 45.67 VA → 4567)

  Wiring:
    ACS712 OUT  → A0  (PA0)
    LCD SDA     → PB7
    LCD SCL     → PB6
    Serial3 TX  → PB10  → NodeMCU D7 (GPIO13)
    GND         → NodeMCU GND  ← REQUIRED
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

HardwareSerial Serial3(PC5, PB10);  // RX=PC5, TX=PB10

// ── Configuration ─────────────────────────────────────────────────
#define SENSOR_PIN        A0
#define ADC_RESOLUTION    4096.0f
#define VREF              3.3f
#define SENSITIVITY       0.100f   // 20A module. Change: 5A→0.185f, 30A→0.066f
#define MAINS_VOLTAGE     230.0f
#define SAMPLES           1000
#define LCD_ADDRESS       0x27     // Change to 0x3F if LCD blank
#define NOISE_THRESHOLD   0.25f

// ── Packet headers / footers ──────────────────────────────────────
#define PKT_RMS_HDR     0xAA
#define PKT_RMS_FTR     0x55
#define PKT_PEAK_HDR    0xBB
#define PKT_PEAK_FTR    0x66
#define PKT_POWER_HDR   0xCC
#define PKT_POWER_FTR   0x77
// ─────────────────────────────────────────────────────────────────

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

void sendPacket(uint8_t header, float value, uint8_t footer) {
  uint16_t scaled = (uint16_t)constrain(value * 100.0f, 0.0f, 65535.0f);
  uint8_t pkt[4] = { header, (uint8_t)(scaled >> 8), (uint8_t)(scaled & 0xFF), footer };
  Serial3.write(pkt, 4);
}

float readOffset() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += analogRead(SENSOR_PIN);
  return (float)sum / SAMPLES;
}

void setup() {
  Serial.begin(115200);
  Serial3.begin(9600);       // 9600 baud — reliable for ESP8266 SoftwareSerial

  analogReadResolution(12);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("  ACS712 Meter  ");
  lcd.setCursor(0, 1); lcd.print("  Initializing  ");
  delay(1500);
  lcd.clear();
}

void loop() {
  float offset = readOffset();
  float sumSq  = 0.0f;
  float peak   = 0.0f;

  for (int i = 0; i < SAMPLES; i++) {
    float raw   = (float)analogRead(SENSOR_PIN) - offset;
    float volts = raw * (VREF / ADC_RESOLUTION);
    float amps  = volts / SENSITIVITY;
    sumSq += amps * amps;
    if (fabsf(amps) > peak) peak = fabsf(amps);
  }

  float rms = sqrtf(sumSq / SAMPLES);

  if (rms  < NOISE_THRESHOLD) { rms  = 0.0f; peak = 0.0f; }
  if (peak < NOISE_THRESHOLD) { peak = 0.0f; }

  float power = rms * MAINS_VOLTAGE;

  // ── Serial Monitor ────────────────────────────────────────────
  Serial.print("RMS: ");   Serial.print(rms,   3); Serial.print(" A  |  ");
  Serial.print("Peak: ");  Serial.print(peak,  3); Serial.print(" A  |  ");
  Serial.print("Power: "); Serial.print(power, 2); Serial.println(" VA");

  // ── UART → NodeMCU ───────────────────────────────────────────
  sendPacket(PKT_RMS_HDR,   rms,   PKT_RMS_FTR);
  sendPacket(PKT_PEAK_HDR,  peak,  PKT_PEAK_FTR);
  sendPacket(PKT_POWER_HDR, power, PKT_POWER_FTR);

  // ── LCD ──────────────────────────────────────────────────────
  lcd.setCursor(0, 0);
  lcd.print("I:");   lcd.print(rms,  2); lcd.print("A Pk:"); lcd.print(peak, 2); lcd.print("A ");
  lcd.setCursor(0, 1);
  lcd.print("Power: "); lcd.print(power, 1); lcd.print(" VA   ");

  delay(500);
}
