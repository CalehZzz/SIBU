/*
 * SIBU — ESP32 · mesa unificada
 *   HC-SR04 ×2 · LCD · HX711 · RC522
 *
 * WiFi: bins → :8082/api/bins · RFID → :8081/api/rfid
 *
 * Librerías: LiquidCrystal I2C · HX711 (bogde) · MFRC522
 *
 * Báscula: si ves HX? a veces = WiFi pisa el HX711 un instante (ya no borra el último kg).
 * Kg altísimos con HX_SCALE=1.0 = normal (son cuentas, no gramos). Calibrá SCALE.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711.h>
#include <SPI.h>
#include <MFRC522.h>

// ===== WiFi =====
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
const bool HX_INVERT = false;
const float HX_CAL_GRAMS = 100.0f;
// IMPORTANTE: después de calibrar poné el valor que te diga el Serial
// (ej. 420.0). Con 1.0 el LCD muestra cuentas/1000 → kg inventados altos.
float HX_SCALE = 1.0f;
HX711 scale;
bool hxOk = false;
int  hxFailStreak = 0;
long hxTareRaw = 0;
long hxLastDelta = 0;

// ===== RC522 =====
// SDA/SS→5 · SCK→14 · MOSI→13 · MISO→23 · RST→27 · 3.3V
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
const int LED_GREEN  = -1;

float lastCm[2] = {NAN, NAN};
int   lastPct[2] = {0, 0};
bool  lleno[2] = {false, false};
bool  sensorOk[2] = {false, false};
float lastKg = 0.0f;
unsigned long lastBinsMs = 0;
unsigned long lastHxMs = 0;

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
  } else if (!hxOk && hxFailStreak > 8) {
    // Solo HX? si falló varias veces seguidas (no un glitch de WiFi)
    snprintf(l1, sizeof(l1), "HX?  R:%3d%%   ", lastPct[1]);
  } else if (HX_SCALE <= 1.01f) {
    // Sin calibrar: mostrá delta crudo (cuentas), no "kg"
    snprintf(l1, sizeof(l1), "d:%5ld R:%3d%%", hxLastDelta, lastPct[1]);
  } else {
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
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://") + PI_HOST + ":" + PI_RFID + "/api/rfid";
  http.setTimeout(2500);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"uid\":\"") + uid + "\"}";
  int code = http.POST(body);
  String resp = http.getString();
  Serial.printf("POST rfid → %d %s\n", code, resp.c_str());
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

void leerBascula() {
  // No leer HX en cada tick si acabamos de leer (deja respirar al WiFi/SPI)
  if (millis() - lastHxMs < 300UL && hxOk) return;
  lastHxMs = millis();

  scale.power_up();
  if (!hxWaitReady(80)) {
    hxFailStreak++;
    // NO apagues hxOk de una: mantené el último kg en LCD
    if (hxFailStreak > 8) hxOk = false;
    if (DEBUG_HX) Serial.printf("HX skip (not ready) streak=%d — se mantiene kg=%.4f\n", hxFailStreak, lastKg);
    return;
  }

  // 5 lecturas, descartá min/max (filtro simple de picos)
  long vals[5];
  int n = 0;
  for (int i = 0; i < 5; i++) {
    if (!hxWaitReady(40)) continue;
    vals[n++] = scale.read();
    delay(3);
  }
  if (n < 3) {
    hxFailStreak++;
    if (hxFailStreak > 8) hxOk = false;
    if (DEBUG_HX) Serial.println("HX pocas muestras");
    return;
  }

  // bubble sort chico
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (vals[j] < vals[i]) {
        long t = vals[i];
        vals[i] = vals[j];
        vals[j] = t;
      }
    }
  }
  long raw = vals[n / 2];  // mediana
  long delta = raw - hxTareRaw;
  if (HX_INVERT) delta = -delta;

  // Rechazar picos absurdos vs lectura anterior (ruido / glitch)
  if (hxOk && hxLastDelta != 0) {
    long jump = delta - hxLastDelta;
    if (jump < 0) jump = -jump;
    // si SCALE aún es 1, delta es grande; umbral en cuentas ~ 2e6
    if (jump > 2000000L) {
      if (DEBUG_HX) Serial.printf("HX pico ignorado delta=%ld (prev=%ld)\n", delta, hxLastDelta);
      return;
    }
  }

  hxFailStreak = 0;
  hxOk = true;
  hxLastDelta = delta;

  float units = (float)delta / HX_SCALE;
  if (units < 0 && units > -5.0f) units = 0;
  if (units < 0) units = 0;
  // Tope demo: más de 50 kg con SCALE calibrado = basura
  if (HX_SCALE > 1.01f && units > 50000.0f) {
    if (DEBUG_HX) Serial.printf("HX units absurdas %.1f — ignore\n", units);
    return;
  }
  lastKg = units / 1000.0f;

  if (DEBUG_HX) {
    Serial.printf(
      "HX raw=%ld tare=%ld delta=%ld  → %.1f (SCALE=%.3f)  kg=%.4f\n",
      raw, hxTareRaw, delta, units, HX_SCALE, lastKg
    );
    long ad = delta < 0 ? -delta : delta;
    if (ad < 200) {
      Serial.println("HX aviso: delta~0 → montaje 3D no carga la celda");
    } else if (HX_SCALE <= 1.01f) {
      Serial.printf(
        "HX calib: con %.0fg →  HX_SCALE = %ld / %.0f  = %.1f\n",
        HX_CAL_GRAMS, ad, HX_CAL_GRAMS, (float)ad / HX_CAL_GRAMS
      );
    }
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
    lastKg,
    lamp,
    stopAll ? "true" : "false",
    (lleno[0] && !lleno[1]) ? "true" : "false"
  );

  Serial.println(buf);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(2500);
    String url = String("http://") + PI_HOST + ":" + PI_BINS + "/api/bins";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(String(buf));
    Serial.printf("POST bins → %d\n", code);
    http.end();
    rfidBusReady();
  }
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
  const char* lamp = "green";
  if (stopAll) lamp = "red";
  else if (divertP) lamp = "yellow";

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
  lcd.print("HC+HX+RFID");

  scale.begin(HX_DT, HX_SCK);
  delay(400);
  scale.set_gain(128);
  scale.power_up();
  Serial.print("HX711 wait");
  hxOk = false;
  for (int i = 0; i < 80; i++) {
    if (scale.is_ready()) { hxOk = true; break; }
    Serial.print(".");
    delay(50);
  }
  Serial.println();
  if (!hxOk) {
    Serial.println("HX711 FAIL — VCC 3V3 · DT26 · SCK25 · GND");
  } else {
    delay(200);
    // Promedio de tara más largo = más estable
    long sum = 0;
    int n = 0;
    for (int i = 0; i < 30; i++) {
      if (hxWaitReady(100)) {
        sum += scale.read();
        n++;
      }
      delay(5);
    }
    hxTareRaw = (n > 0) ? (sum / n) : 0;
    hxLastDelta = 0;
    hxFailStreak = 0;
    Serial.printf("HX711 OK tare_raw=%ld  HX_SCALE=%.3f\n", hxTareRaw, HX_SCALE);
    Serial.println("LCD sin calibrar muestra d:##### (delta), no kg.");
    Serial.printf("Con %.0fg: HX_SCALE = |delta|/%.0f  luego re-flash\n", HX_CAL_GRAMS, HX_CAL_GRAMS);
  }

  rfidKick();
  if (rfidOk) Serial.println("RC522 OK — sacá y acercá tarjeta");
  else Serial.println("RC522 FAIL");

  Serial.println("Pines: HC 18/19 33/32 | HX 26/25 | RFID 5/14/13/23/27");

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
    Serial.printf("Pi %s bins:%d rfid:%d\n", PI_HOST, PI_BINS, PI_RFID);
  } else {
    Serial.println("WiFi FAIL");
  }

  rfidKick();
  // Retara DESPUÉS del WiFi (el consumo cambia un poco el offset)
  if (hxOk) {
    delay(300);
    long sum = 0;
    int n = 0;
    for (int i = 0; i < 20; i++) {
      if (hxWaitReady(80)) {
        sum += scale.read();
        n++;
      }
      delay(5);
    }
    if (n > 5) {
      hxTareRaw = sum / n;
      Serial.printf("HX retara post-WiFi tare_raw=%ld\n", hxTareRaw);
    }
  }
  Serial.println("SIBU listo");
}

void loop() {
  pollRfid();
  unsigned long now = millis();
  if (now - lastBinsMs >= 550UL) {
    lastBinsMs = now;
    tickBins();
  }
  delay(10);
}
