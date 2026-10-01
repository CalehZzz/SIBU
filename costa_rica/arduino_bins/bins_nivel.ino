/*
 * SIBU — ESP32 · mesa unificada
 *   - 2× HC-SR04: plástico + rechazo
 *   - 1× LCD I2C: % + kg (+ UID al tap)
 *   - 1× HX711: báscula plástico
 *   - 1× RC522: RFID → POST Pi :8081 /api/rfid
 *
 * WiFi:
 *   bins  → POST http://PI:8082/api/bins
 *   RFID  → POST http://PI:8081/api/rfid  { "uid": "..." }
 *
 * Librerías:
 *   - LiquidCrystal I2C (Frank de Brabander)
 *   - HX711 by bogde
 *   - MFRC522 by Miguel Balboa
 *
 * ¿Se satura? No. ESP32 sobra: HC ~cada 400 ms, RFID poll ~20 ms,
 * WiFi solo al publicar bins o al tap. Arduino USB ya no hace falta.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711.h>
#include <SPI.h>
#include <MFRC522.h>

// ===== CONFIG WiFi =====
const char* WIFI_SSID = "TU_HOTSPOT_O_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";
const char* PI_HOST   = "172.20.10.3";  // hostname -I en la Pi
const int   PI_BINS   = 8082;
const int   PI_RFID   = 8081;

const bool DEBUG_HC = true;
const bool DEBUG_HX = true;   // Serial: raw / g / kg — dejalo ON hasta calibrar
const bool DEMO_NO_STOP = false;

// ===== HC-SR04 =====
// 0=plástico · 1=rechazo
const int TRIG[2] = {18, 33};
const int ECHO[2] = {19, 32};
const float VACIO_CM[2] = {40.0, 40.0};
const float LLENO_CM[2] = {8.0, 8.0};
const int   LLENO_PCT   = 90;
const float MIN_CM = 2.0f;
const float MAX_CM = 400.0f;

// ===== LCD I2C =====
const uint8_t LCD_ADDR = 0x27;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// ===== HX711 — módulo "Load Cell Amplifier" / "Load Cell Amp HX711" =====
// Cara lógica (al ESP32):
//   VCC     → 5V del ESP (en este módulo conviene 5V, no 3V3)
//   GND     → GND
//   DT/DOUT → GPIO 26
//   SCK     → GPIO 25   (a veces dice PD_SCK)
// Cara celda (load cell, 4 hilos):
//   E+ / RED    · E− / BLK
//   A+ / GRN    · A− / WHT   (si raw no se mueve, probá swap A+/A−)
// No uses el canal B (B+/B−) salvo celdas especiales.
const int HX_DT  = 26;  // DT / DOUT del HX711
const int HX_SCK = 25;  // SCK / PD_SCK del HX711
// Calibración (gramos): 1) vacío → tare al boot
// 2) poné peso conocido (ej. 100 g)  3) mirá Serial "HX raw=…" / "units="
// 4) HX_SCALE = |raw_con_peso - raw_vacio| / gramos
//    o bien: HX_SCALE = HX_SCALE * units / 100  si units≠100 con 100 g
float HX_SCALE = 420.0f;
HX711 scale;
bool hxOk = false;

// ===== RC522 (SPI propio — no choca con HC 18/19 ni LCD 21/22) =====
// En el módulo RC522 el pin se llama SDA / NSS / SS → es el chip-select SPI
// (NO es I2C; no va a GPIO 21).
//   RC522        ESP32
//   SDA / SS     GPIO 5
//   SCK          GPIO 14
//   MOSI         GPIO 13
//   MISO         GPIO 23
//   RST          GPIO 27   (15 es strapping → SW_RESET raros; 16/17 no en tu placa)
//   3.3V         3.3V      (NUNCA 5V)
//   GND          GND
#define RFID_SS   5
#define RFID_RST  27
#define RFID_SCK  14
#define RFID_MOSI 13
#define RFID_MISO 23
MFRC522 mfrc522(RFID_SS, RFID_RST);
String lastUid = "";
unsigned long lastTapMs = 0;
unsigned long lcdUidUntilMs = 0;
String lcdUidMsg = "";

// Salidas PLC / LEDs opcionales (-1 = off). GPIO 5 = SDA/SS · 27 = RST
const int OUT_FULL_P = -1;
const int OUT_FULL_R = -1;
const int LED_YELLOW = 4;
const int LED_RED    = 2;
const int LED_GREEN  = -1;

float lastCm[2] = {NAN, NAN};
int   lastPct[2] = {0, 0};
bool  lleno[2] = {false, false};
bool  sensorOk[2] = {false, false};
float lastKg = 0.0f;
unsigned long lastBinsMs = 0;

float leerCm(int i, unsigned long* usOut) {
  digitalWrite(TRIG[i], LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[i], LOW);
  unsigned long us = pulseIn(ECHO[i], HIGH, 30000UL);
  if (usOut) *usOut = us;
  if (us == 0) return NAN;
  float cm = (us * 0.0343f) / 2.0f;
  if (cm < MIN_CM || cm > MAX_CM) return NAN;
  return cm;
}

int cmToPct(int i, float cm) {
  if (isnan(cm)) return 0;
  float vacio = VACIO_CM[i];
  float llen = LLENO_CM[i];
  if (vacio <= llen) return 0;
  float pct = (vacio - cm) / (vacio - llen) * 100.0f;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)(pct + 0.5f);
}

void pintarLcd() {
  if (millis() < lcdUidUntilMs && lcdUidMsg.length()) {
    lcd.setCursor(0, 0);
    lcd.print("RFID OK         ");
    lcd.setCursor(0, 1);
    char line[17];
    snprintf(line, sizeof(line), "%-16s", lcdUidMsg.c_str());
    lcd.print(line);
    return;
  }
  lcd.setCursor(0, 0);
  char l0[17];
  if (!sensorOk[0]) snprintf(l0, sizeof(l0), "P:FAIL cable   ");
  else snprintf(l0, sizeof(l0), "P:%3d%%%s        ", lastPct[0], lleno[0] ? " FULL" : "");
  lcd.print(l0);
  lcd.setCursor(0, 1);
  char l1[17];
  if (!sensorOk[1]) snprintf(l1, sizeof(l1), "R:FAIL         ");
  else if (!hxOk) snprintf(l1, sizeof(l1), "HX?  R:%3d%%   ", lastPct[1]);
  else snprintf(l1, sizeof(l1), "%5.3fkg R:%3d%%  ", lastKg, lastPct[1]);
  lcd.print(l1);
}

String uidToHex(MFRC522::Uid uid) {
  String s = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

bool postRfid(const String& uid) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://") + PI_HOST + ":" + PI_RFID + "/api/rfid";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"uid\":\"") + uid + "\"}";
  int code = http.POST(body);
  String resp = http.getString();
  Serial.printf("POST rfid → %d %s\n", code, resp.c_str());
  http.end();
  return code >= 200 && code < 300;
}

void pollRfid() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  String uid = uidToHex(mfrc522.uid);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  unsigned long now = millis();
  if (uid == lastUid && (now - lastTapMs) < 1500UL) return;
  lastUid = uid;
  lastTapMs = now;

  Serial.printf("RFID uid=%s\n", uid.c_str());
  lcdUidMsg = uid.substring(0, 16);
  lcdUidUntilMs = now + 2500UL;
  pintarLcd();
  postRfid(uid);
}

void publicarBins(const char* lamp, bool stopAll) {
  char buf[480];
  snprintf(
    buf, sizeof(buf),
    "{\"plastico\":{\"cm\":%.1f,\"pct\":%d,\"lleno\":%s},"
    "\"aluminio\":{\"cm\":-1,\"pct\":0,\"lleno\":false},"
    "\"vidrio\":{\"cm\":-1,\"pct\":0,\"lleno\":false},"
    "\"rechazo\":{\"cm\":%.1f,\"pct\":%d,\"lleno\":%s},"
    "\"pesoKg\":%.4f,\"pesoMaterial\":\"plastico\","
    "\"lamp\":\"%s\",\"stop\":%s,"
    "\"divert\":{\"plastico\":%s,\"aluminio\":false,\"vidrio\":false},"
    "\"demo\":true}",
    isnan(lastCm[0]) ? -1.0 : lastCm[0], lastPct[0], lleno[0] ? "true" : "false",
    isnan(lastCm[1]) ? -1.0 : lastCm[1], lastPct[1], lleno[1] ? "true" : "false",
    lastKg,
    lamp,
    stopAll ? "true" : "false",
    (lleno[0] && !lleno[1]) ? "true" : "false"
  );

  Serial.println(buf);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + PI_HOST + ":" + PI_BINS + "/api/bins";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(String(buf));
    Serial.printf("POST bins → %d\n", code);
    http.end();
  }
}

void tickBins() {
  const char* nombre[2] = {"plastico", "rechazo"};
  for (int i = 0; i < 2; i++) {
    unsigned long us1 = 0, us2 = 0;
    float cm = leerCm(i, &us1);
    delay(30);
    pollRfid();  // no perder taps mientras midimos
    float cm2 = leerCm(i, &us2);
    if (!isnan(cm) && !isnan(cm2)) cm = (cm + cm2) / 2.0f;
    else if (isnan(cm)) cm = cm2;

    lastCm[i] = cm;
    sensorOk[i] = !isnan(cm);
    lastPct[i] = cmToPct(i, cm);
    lleno[i] = sensorOk[i] && (lastPct[i] >= LLENO_PCT);

    if (DEBUG_HC) {
      if (sensorOk[i]) {
        Serial.printf("HC%d %-8s us=%lu/%lu cm=%.1f pct=%d%s\n",
                      i, nombre[i], us1, us2, cm, lastPct[i],
                      lleno[i] ? " FULL" : "");
      } else {
        Serial.printf("HC%d %-8s FAIL us=%lu/%lu\n", i, nombre[i], us1, us2);
      }
    }
    delay(20);
    pollRfid();
  }
  if (OUT_FULL_P >= 0) digitalWrite(OUT_FULL_P, lleno[0] ? HIGH : LOW);
  if (OUT_FULL_R >= 0) digitalWrite(OUT_FULL_R, lleno[1] ? HIGH : LOW);

  // --- HX711 ---
  if (scale.is_ready()) {
    hxOk = true;
    long raw = scale.read();
    float g = scale.get_units(5);
    // ruido / tara negativa → 0; no ocultar lecturas chicas reales
    if (g < -1.0f) g = 0;
    else if (g < 0) g = 0;
    lastKg = g / 1000.0f;
    if (DEBUG_HX) {
      Serial.printf("HX raw=%ld  units=%.1fg  kg=%.4f  SCALE=%.1f\n",
                    raw, g, lastKg, HX_SCALE);
    }
  } else {
    hxOk = false;
    if (DEBUG_HX) {
      Serial.println("HX FAIL not ready — DT→26 SCK→25 VCC→5V GND · ¿cables cruzados DT/SCK?");
    }
  }

  bool stopAll = lleno[1] && !DEMO_NO_STOP;
  bool divertP = lleno[0] && !stopAll;
  const char* lamp = "green";
  if (stopAll) lamp = "red";
  else if (divertP) lamp = "yellow";

  if (LED_RED >= 0) digitalWrite(LED_RED, stopAll ? HIGH : LOW);
  if (LED_YELLOW >= 0) digitalWrite(LED_YELLOW, divertP ? HIGH : LOW);
  if (LED_GREEN >= 0) digitalWrite(LED_GREEN, (!stopAll && !divertP) ? HIGH : LOW);

  pintarLcd();
  publicarBins(lamp, stopAll);
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("=== SIBU ESP32 boot ===");

  for (int i = 0; i < 2; i++) {
    pinMode(TRIG[i], OUTPUT);
    pinMode(ECHO[i], INPUT);
    digitalWrite(TRIG[i], LOW);
  }
  if (OUT_FULL_P >= 0) pinMode(OUT_FULL_P, OUTPUT);
  if (OUT_FULL_R >= 0) pinMode(OUT_FULL_R, OUTPUT);
  if (LED_YELLOW >= 0) pinMode(LED_YELLOW, OUTPUT);
  if (LED_RED >= 0) pinMode(LED_RED, OUTPUT);
  if (LED_GREEN >= 0) pinMode(LED_GREEN, OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("SIBU ESP32");
  lcd.setCursor(0, 1);
  lcd.print("HC+HX+RFID");

  scale.begin(HX_DT, HX_SCK);
  delay(200);
  scale.set_gain(128);  // canal A
  Serial.print("HX711 wait ready");
  hxOk = false;
  for (int i = 0; i < 50; i++) {
    if (scale.is_ready()) {
      hxOk = true;
      break;
    }
    Serial.print(".");
    delay(50);
  }
  Serial.println();
  if (!hxOk) {
    Serial.println("HX711 FAIL — no responde. VCC 5V, DT=26, SCK=25, GND. Celda E+/E-/A+/A-.");
    lcd.clear();
    lcd.print("HX711 FAIL");
    lcd.setCursor(0, 1);
    lcd.print("DT26 SCK25 5V");
  } else {
    scale.set_scale(HX_SCALE);
    scale.tare(25);
    long z = scale.read_average(10);
    Serial.printf("HX711 OK tare raw≈%ld  SCALE=%.1f (calibrá con peso conocido)\n", z, HX_SCALE);
    Serial.println("Calibrar: poné 100g → mirá units= → HX_SCALE = HX_SCALE * units / 100");
  }

  // Soft-reset del RC522 por pin RST antes de SPI
  pinMode(RFID_RST, OUTPUT);
  digitalWrite(RFID_RST, LOW);
  delay(50);
  digitalWrite(RFID_RST, HIGH);
  delay(50);

  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  mfrc522.PCD_Init();
  delay(80);
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.printf("RC522 version=0x%02X ", v);
  if (v == 0x00 || v == 0xFF) {
    Serial.println("FAIL — revisá 3.3V GND SDA=5 SCK=14 MOSI=13 MISO=23 RST=27");
    lcd.clear();
    lcd.print("RC522 FAIL");
    lcd.setCursor(0, 1);
    lcd.print("cables/3.3V");
  } else {
    Serial.println("OK — acercá tarjeta");
  }

  Serial.println("HC P:18/19  R:33/32 | HX711:26/25 | RFID SDA/SS=5 SCK14 MOSI13 MISO23 RST27");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP ");
    Serial.println(WiFi.localIP());
    Serial.printf("Pi bins :%d  rfid :%d  host=%s\n", PI_BINS, PI_RFID, PI_HOST);
  } else {
    Serial.println("WiFi FAIL — RFID/bins no van a la Pi hasta que conecte");
  }

  Serial.println("SIBU mesa unificada lista");
}

void loop() {
  pollRfid();
  unsigned long now = millis();
  if (now - lastBinsMs >= 400UL) {
    lastBinsMs = now;
    tickBins();
  }
  delay(15);
}
