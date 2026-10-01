/*
 * SIBU — ESP32 · mesa unificada
 * HC ×2 · LCD · HX711 · RC522
 *
 * Librerías: LiquidCrystal I2C · HX711 (bogde) · MFRC522
 *
 * Si el internet/roaming va mal: WIFI_ENABLE = false
 * (probás báscula/RFID por Serial/LCD sin pelear con WiFi).
 * Cuando tengas red estable: WIFI_ENABLE = true.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711.h>
#include <SPI.h>
#include <MFRC522.h>

// ===== WiFi =====
// false = no conecta WiFi (recomendado si el roaming falla)
const bool WIFI_ENABLE = false;
const char* WIFI_SSID = "TU_HOTSPOT_O_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";
const char* PI_HOST   = "172.20.10.3";
const int   PI_BINS   = 8082;
const int   PI_RFID   = 8081;

const bool DEBUG_HC = false;
const bool DEBUG_HX = true;
const bool DEMO_NO_STOP = false;

// ===== HC-SR04 =====
const int TRIG[2] = {18, 33};
const int ECHO[2] = {19, 32};
const float VACIO_CM[2] = {40.0, 40.0};
const float LLENO_CM[2] = {8.0, 8.0};
const int   LLENO_PCT   = 90;
const float MIN_CM = 2.0f;
const float MAX_CM = 400.0f;

const uint8_t LCD_ADDR = 0x27;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// ===== HX711 =====
// VCC→3V3 · GND · DT→26 · SCK→25
// Celda: ROJO E+ · NEGRO E− · VERDE A+ · BLANCO A−
const int HX_DT  = 26;
const int HX_SCK = 25;
// Tu log: delta salía negativo → true
const bool HX_INVERT = true;
const float HX_CAL_GRAMS = 100.0f;
// Con ~500 cuentas / 100 g → 5.0
float HX_SCALE = 5.0f;

HX711 scale;
bool hxOk = false;
int  hxFailStreak = 0;
long hxTareRaw = 0;
float lastKg = 0.0f;
unsigned long lastHxMs = 0;

// ===== RC522 =====
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
unsigned long lastRfidKickMs = 0;
bool rfidOk = false;

const int LED_YELLOW = 4;
const int LED_RED    = 2;

float lastCm[2] = {NAN, NAN};
int   lastPct[2] = {0, 0};
bool  lleno[2] = {false, false};
bool  sensorOk[2] = {false, false};
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
  if (!rfidOk) snprintf(l0, sizeof(l0), "RFID FAIL       ");
  else if (!sensorOk[0]) snprintf(l0, sizeof(l0), "P:FAIL cable   ");
  else snprintf(l0, sizeof(l0), "P:%3d%%%s        ", lastPct[0], lleno[0] ? " FULL" : "");
  lcd.print(l0);

  lcd.setCursor(0, 1);
  char l1[17];
  if (!sensorOk[1]) {
    snprintf(l1, sizeof(l1), "R:FAIL         ");
  } else if (!hxOk && hxFailStreak > 15) {
    snprintf(l1, sizeof(l1), "HX?  R:%3d%%   ", lastPct[1]);
  } else {
    // Siempre último kg bueno (no parpadea HX? por un fallo)
    snprintf(l1, sizeof(l1), "%5.3fkg R:%3d%%  ", lastKg, lastPct[1]);
  }
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

void rfidBusReady() {
  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  pinMode(RFID_SS, OUTPUT);
  digitalWrite(RFID_SS, HIGH);
}

void rfidKick() {
  rfidBusReady();
  pinMode(RFID_RST, OUTPUT);
  digitalWrite(RFID_RST, LOW);
  delay(5);
  digitalWrite(RFID_RST, HIGH);
  delay(20);
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  rfidOk = (v != 0x00 && v != 0xFF);
  if (DEBUG_HX || !rfidOk) {
    Serial.printf("RFID kick version=0x%02X %s\n", v, rfidOk ? "OK" : "FAIL");
  }
  lastRfidKickMs = millis();
}

bool postRfid(const String& uid) {
  if (!WIFI_ENABLE || WiFi.status() != WL_CONNECTED) {
    Serial.println("RFID OK local (sin WiFi) — no POST a Pi");
    return false;
  }
  HTTPClient http;
  String url = String("http://") + PI_HOST + ":" + PI_RFID + "/api/rfid";
  http.setTimeout(2000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"uid\":\"") + uid + "\"}";
  int code = http.POST(body);
  Serial.printf("POST rfid → %d\n", code);
  http.end();
  rfidBusReady();
  return code >= 200 && code < 300;
}

void pollRfid() {
  if (millis() - lastRfidKickMs > 7000UL) rfidKick();
  rfidBusReady();

  bool present = mfrc522.PICC_IsNewCardPresent();
  if (!present) present = mfrc522.PICC_IsNewCardPresent();
  if (!present) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = uidToHex(mfrc522.uid);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  digitalWrite(RFID_SS, HIGH);

  unsigned long now = millis();
  if (uid == lastUid && (now - lastTapMs) < 1200UL) return;
  lastUid = uid;
  lastTapMs = now;

  Serial.printf("RFID uid=%s\n", uid.c_str());
  lcdUidMsg = uid.substring(0, 16);
  lcdUidUntilMs = now + 2500UL;
  pintarLcd();
  postRfid(uid);
}

bool hxWaitReady(unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (scale.is_ready()) return true;
    delay(1);
  }
  return scale.is_ready();
}

long hxReadMedian(int want) {
  long vals[9];
  int n = 0;
  for (int i = 0; i < want; i++) {
    if (!hxWaitReady(60)) {
      delay(5);
      continue;
    }
    vals[n++] = scale.read();
    delay(4);
  }
  if (n < 3) return 0x7FFFFFFF;  // sentinel = fallo
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (vals[j] < vals[i]) {
        long t = vals[i]; vals[i] = vals[j]; vals[j] = t;
      }
    }
  }
  return vals[n / 2];
}

void hxDoTare() {
  scale.power_up();
  delay(150);
  long sum = 0;
  int n = 0;
  for (int i = 0; i < 25; i++) {
    long v = hxReadMedian(5);
    if (v == 0x7FFFFFFF) continue;
    sum += v;
    n++;
    delay(10);
  }
  if (n > 3) {
    hxTareRaw = sum / n;
    hxOk = true;
    hxFailStreak = 0;
    lastKg = 0;
    Serial.printf("HX TARE raw=%ld (n=%d)\n", hxTareRaw, n);
  } else {
    Serial.println("HX TARE falló");
  }
}

void leerBascula() {
  if (millis() - lastHxMs < 250UL) return;
  lastHxMs = millis();

  scale.power_up();
  long raw = hxReadMedian(7);
  if (raw == 0x7FFFFFFF) {
    hxFailStreak++;
    if (hxFailStreak > 15) hxOk = false;
    if (DEBUG_HX) Serial.printf("HX pocas muestras streak=%d (kg se mantiene %.3f)\n", hxFailStreak, lastKg);
    return;
  }

  long delta = raw - hxTareRaw;
  if (HX_INVERT) delta = -delta;

  // gramos
  float g = (float)delta / HX_SCALE;
  // ruido chico alrededor de 0
  if (g > -3.0f && g < 3.0f) g = 0;
  if (g < 0) g = 0;
  if (g > 30000.0f) {
    if (DEBUG_HX) Serial.printf("HX g absurdo %.1f ignore\n", g);
    return;
  }

  hxFailStreak = 0;
  hxOk = true;
  lastKg = g / 1000.0f;

  if (DEBUG_HX) {
    Serial.printf("HX raw=%ld tare=%ld delta=%ld  g=%.1f  kg=%.4f  SCALE=%.2f INV=%d\n",
                  raw, hxTareRaw, HX_INVERT ? -(raw - hxTareRaw) : (raw - hxTareRaw),
                  g, lastKg, HX_SCALE, (int)HX_INVERT);
  }
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
    lastKg, lamp, stopAll ? "true" : "false",
    (lleno[0] && !lleno[1]) ? "true" : "false"
  );
  Serial.println(buf);

  if (!WIFI_ENABLE || WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(2000);
  String url = String("http://") + PI_HOST + ":" + PI_BINS + "/api/bins";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(String(buf));
  Serial.printf("POST bins → %d\n", code);
  http.end();
  rfidBusReady();
}

void tickBins() {
  for (int i = 0; i < 2; i++) {
    unsigned long us1 = 0, us2 = 0;
    float cm = leerCm(i, &us1);
    delay(15);
    pollRfid();
    float cm2 = leerCm(i, &us2);
    if (!isnan(cm) && !isnan(cm2)) cm = (cm + cm2) / 2.0f;
    else if (isnan(cm)) cm = cm2;
    lastCm[i] = cm;
    sensorOk[i] = !isnan(cm);
    lastPct[i] = cmToPct(i, cm);
    lleno[i] = sensorOk[i] && (lastPct[i] >= LLENO_PCT);
    pollRfid();
  }

  leerBascula();
  pollRfid();

  bool stopAll = lleno[1] && !DEMO_NO_STOP;
  bool divertP = lleno[0] && !stopAll;
  const char* lamp = stopAll ? "red" : (divertP ? "yellow" : "green");
  if (LED_RED >= 0) digitalWrite(LED_RED, stopAll ? HIGH : LOW);
  if (LED_YELLOW >= 0) digitalWrite(LED_YELLOW, divertP ? HIGH : LOW);

  pintarLcd();
  publicarBins(lamp, stopAll);
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("=== SIBU ESP32 boot ===");
  Serial.printf("WIFI_ENABLE=%d  HX_INVERT=%d  HX_SCALE=%.2f\n",
                (int)WIFI_ENABLE, (int)HX_INVERT, HX_SCALE);

  for (int i = 0; i < 2; i++) {
    pinMode(TRIG[i], OUTPUT);
    pinMode(ECHO[i], INPUT);
    digitalWrite(TRIG[i], LOW);
  }
  if (LED_YELLOW >= 0) pinMode(LED_YELLOW, OUTPUT);
  if (LED_RED >= 0) pinMode(LED_RED, OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("SIBU ESP32");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_ENABLE ? "WiFi ON " : "WiFi OFF");

  scale.begin(HX_DT, HX_SCK);
  delay(400);
  scale.set_gain(128);
  scale.power_up();
  Serial.print("HX711 wait");
  for (int i = 0; i < 60 && !scale.is_ready(); i++) {
    Serial.print(".");
    delay(50);
  }
  Serial.println();
  hxDoTare();

  rfidKick();
  Serial.println(rfidOk ? "RC522 OK" : "RC522 FAIL");
  Serial.println("Pines: HC 18/19 33/32 | HX 26/25 | RFID 5/14/13/23/27");

  if (WIFI_ENABLE) {
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
    } else {
      Serial.println("WiFi FAIL — sigo local");
    }
    rfidKick();
    // Retara con WiFi ya decidido
    delay(200);
    hxDoTare();
  } else {
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi OFF — báscula/RFID solo local (Serial/LCD)");
  }

  Serial.println("SIBU listo — vacío al boot = 0 kg; poné peso y mirá g=");
}

void loop() {
  pollRfid();
  unsigned long now = millis();
  if (now - lastBinsMs >= 600UL) {
    lastBinsMs = now;
    tickBins();
  }
  delay(10);
}
