/*
 * SIBU — ESP32 · mesa unificada
 *   HC-SR04 ×2 · LCD · HX711 · RC522
 *
 * WiFi: bins → :8082/api/bins · RFID → :8081/api/rfid
 *
 * Librerías: LiquidCrystal I2C · HX711 (bogde) · MFRC522
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

const bool DEBUG_HC = false;  // true solo si depurás ultrasonidos
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

// ===== HX711 Load Cell Amp =====
// VCC→3V3 (o VIN) · GND · DT→26 · SCK→25
// Celda: ROJO E+ · NEGRO E− · VERDE A+ · BLANCO A−
const int HX_DT  = 26;
const int HX_SCK = 25;
// true si al poner peso el raw BAJA (invertí A+/A− o poné true)
const bool HX_INVERT = false;
// Peso conocido para el asistente de calibración (gramos)
const float HX_CAL_GRAMS = 100.0f;
// Empezá con 1.0f: units ≈ (raw-tare). Luego ajustá con la fórmula del Serial.
float HX_SCALE = 1.0f;
HX711 scale;
bool hxOk = false;
long hxTareRaw = 0;

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
  if (!rfidOk) snprintf(l0, sizeof(l0), "RFID FAIL       ");
  else if (!sensorOk[0]) snprintf(l0, sizeof(l0), "P:FAIL cable   ");
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

void rfidBusReady() {
  // Reclama el SPI del RC522 (por si quedó raro tras WiFi/HTTP)
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
  rfidBusReady();  // SPI otra vez después de WiFi
  return code >= 200 && code < 300;
}

void pollRfid() {
  // Cada ~7 s re-init (el RC522 a veces “se muda” con WiFi)
  if (millis() - lastRfidKickMs > 7000UL) {
    rfidKick();
  }

  rfidBusReady();

  // Quirk MFRC522: a veces hace falta 2º intento de IsNewCardPresent
  bool present = mfrc522.PICC_IsNewCardPresent();
  if (!present) present = mfrc522.PICC_IsNewCardPresent();
  if (!present) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = uidToHex(mfrc522.uid);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  digitalWrite(RFID_SS, HIGH);

  unsigned long now = millis();
  // Debounce corto — sacar y volver a acercar la tarjeta
  if (uid == lastUid && (now - lastTapMs) < 1200UL) return;
  lastUid = uid;
  lastTapMs = now;

  Serial.printf("RFID uid=%s\n", uid.c_str());
  lcdUidMsg = uid.substring(0, 16);
  lcdUidUntilMs = now + 2500UL;
  pintarLcd();
  postRfid(uid);
}

void leerBascula() {
  if (!scale.is_ready()) {
    hxOk = false;
    if (DEBUG_HX) Serial.println("HX not ready");
    return;
  }
  hxOk = true;

  // Promedio estable (raw)
  long sum = 0;
  const int N = 8;
  int nOk = 0;
  for (int i = 0; i < N; i++) {
    if (!scale.is_ready()) {
      delay(2);
      continue;
    }
    sum += scale.read();
    nOk++;
    delay(2);
  }
  if (nOk < 3) {
    hxOk = false;
    return;
  }
  long raw = sum / nOk;
  long delta = raw - hxTareRaw;
  if (HX_INVERT) delta = -delta;

  // Con HX_SCALE=1 → units ≈ delta (cuentas). Después de calibrar = gramos.
  float units = (float)delta / HX_SCALE;
  if (units < 0 && units > -2.0f) units = 0;  // ruido chico
  if (units < 0) units = 0;
  lastKg = units / 1000.0f;

  if (DEBUG_HX) {
    Serial.printf(
      "HX raw=%ld tare=%ld delta=%ld  → %.1f (SCALE=%.3f)  kg=%.4f\n",
      raw, hxTareRaw, delta, units, HX_SCALE, lastKg
    );
    // Ayuda: si |delta| < 200 con 100 g encima → montaje 3D / celda no carga
    long ad = delta < 0 ? -delta : delta;
    if (ad < 200) {
      Serial.println("HX aviso: delta casi 0 con peso → la fuerza NO llega a la celda");
      Serial.println("  (plataforma toca paredes, tornillos traban ambos lados, o celda mal sujeta)");
    } else if (HX_SCALE <= 1.01f) {
      Serial.printf(
        "HX calib: con %.0fg poné  HX_SCALE = |delta|/%.0f  → ej. %.1f\n",
        HX_CAL_GRAMS, HX_CAL_GRAMS, (float)ad / HX_CAL_GRAMS
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
  const char* nombre[2] = {"plastico", "rechazo"};
  for (int i = 0; i < 2; i++) {
    unsigned long us1 = 0, us2 = 0;
    float cm = leerCm(i, &us1);
    delay(20);
    pollRfid();
    float cm2 = leerCm(i, &us2);
    if (!isnan(cm) && !isnan(cm2)) cm = (cm + cm2) / 2.0f;
    else if (isnan(cm)) cm = cm2;

    lastCm[i] = cm;
    sensorOk[i] = !isnan(cm);
    lastPct[i] = cmToPct(i, cm);
    lleno[i] = sensorOk[i] && (lastPct[i] >= LLENO_PCT);

    if (DEBUG_HC) {
      if (sensorOk[i]) {
        Serial.printf("HC%d %-8s cm=%.1f pct=%d\n", i, nombre[i], cm, lastPct[i]);
      } else {
        Serial.printf("HC%d %-8s FAIL\n", i, nombre[i]);
      }
    }
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
  if (LED_YELLOW >= 0) pinMode(LED_YELLOW, OUTPUT);
  if (LED_RED >= 0) pinMode(LED_RED, OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("SIBU ESP32");
  lcd.setCursor(0, 1);
  lcd.print("HC+HX+RFID");

  // --- HX711 primero (bit-bang; no usa SPI HW) ---
  scale.begin(HX_DT, HX_SCK);
  delay(300);
  scale.set_gain(128);
  Serial.print("HX711 wait");
  hxOk = false;
  for (int i = 0; i < 60; i++) {
    if (scale.is_ready()) { hxOk = true; break; }
    Serial.print(".");
    delay(50);
  }
  Serial.println();
  if (!hxOk) {
    Serial.println("HX711 FAIL — VCC 3V3/VIN · DT26 · SCK25 · GND · celda E/A");
  } else {
    // Tara en RAW (sin scale inventado)
    scale.set_scale(1.0f);
    delay(100);
    hxTareRaw = scale.read_average(20);
    scale.set_scale(HX_SCALE);
    Serial.printf("HX711 OK tare_raw=%ld  HX_SCALE=%.3f\n", hxTareRaw, HX_SCALE);
    Serial.println("Vacío al boot. Poné peso conocido y mirá delta=");
    Serial.println("Si delta~0 → montaje 3D no carga la celda.");
    Serial.printf("Si delta grande: HX_SCALE = |delta| / %.0f  (gramos)\n", HX_CAL_GRAMS);
  }

  // --- RC522 ---
  rfidKick();
  if (rfidOk) Serial.println("RC522 OK — acercá tarjeta (sacala y volvé a acercar)");
  else Serial.println("RC522 FAIL — 3.3V SDA5 SCK14 MOSI13 MISO23 RST27");

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
    Serial.printf("Pi %s  bins:%d rfid:%d\n", PI_HOST, PI_BINS, PI_RFID);
  } else {
    Serial.println("WiFi FAIL");
  }

  // WiFi a veces tumba SPI → re-init RFID
  rfidKick();
  Serial.println("SIBU listo");
}

void loop() {
  pollRfid();
  unsigned long now = millis();
  if (now - lastBinsMs >= 500UL) {
    lastBinsMs = now;
    tickBins();
  }
  delay(10);
}
