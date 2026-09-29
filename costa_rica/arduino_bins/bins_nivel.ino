/*
 * SIBU — Niveles de botes (4× HC-SR04 + LCD I2C)
 *
 * Arduino IDE · placa recomendada: ESP32 (WiFi → Pi :8082)
 * También compila en Uno/Nano (solo Serial USB → Pi lee la línea JSON).
 *
 * Librerías:
 *   - LiquidCrystal I2C (Frank de Brabander)  ó  LiquidCrystal_I2C
 *   - (ESP32) WiFi + HTTPClient incluidas en el core
 *
 * Por ahora 1 LCD = bote PLÁSTICO (addr 0x27). Los otros 3 se agregan
 * cambiando LCD_ADDR_* cuando tengas más módulos.
 *
 * Lógica lámpara:
 *   - Algún bote material (P/A/V) lleno → AMARILLA · ese material se desvía a rechazo
 *   - Bote RECHAZO lleno → ROJA · stop total (stop=true)
 *   - Todo OK → VERDE
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#endif

// ===== CONFIG =====
#if defined(ESP32)
const char* WIFI_SSID = "TU_HOTSPOT_O_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";
const char* PI_HOST   = "172.20.10.3";  // IP de la Pi
const int   PI_PORT   = 8082;
#endif

// Distancia (cm): sensor arriba mirando al fondo del bote
// VACIO_CM = eco cuando está vacío · LLENO_CM = umbral “lleno”
const float VACIO_CM[4]  = { 40.0, 40.0, 40.0, 40.0 };
const float LLENO_CM[4]  = { 8.0,  8.0,  8.0,  8.0  };
const int   LLENO_PCT    = 90;   // % para marcar lleno

// HC-SR04  TRIG, ECHO
#if defined(ESP32)
const int TRIG[4] = {13, 14, 26, 33};
const int ECHO[4] = {12, 27, 25, 32};
// Salidas a PLC (24 V vía opto/relé) — opcional
const int OUT_FULL[4] = {16, 17, 18, 19};  // P, A, V, Rechazo
const int LED_YELLOW = 4;
const int LED_RED    = 5;
const int LED_GREEN  = 15;
#else
// Arduino Uno / Nano
const int TRIG[4] = {2, 4, 6, 8};
const int ECHO[4] = {3, 5, 7, 9};
const int OUT_FULL[4] = {10, 11, 12, A0};
const int LED_YELLOW = A1;
const int LED_RED    = A2;
const int LED_GREEN  = A3;
#endif

// LCD I2C — solo plástico por ahora
const uint8_t LCD_ADDR_PLASTICO = 0x27;
LiquidCrystal_I2C lcd(LCD_ADDR_PLASTICO, 16, 2);

const char* NOMBRES[4] = {"Plastico", "Aluminio", "Vidrio", "Rechazo"};
enum { BIN_P = 0, BIN_A = 1, BIN_V = 2, BIN_R = 3 };

float lastCm[4] = {NAN, NAN, NAN, NAN};
int   lastPct[4] = {0, 0, 0, 0};
bool  lleno[4] = {false, false, false, false};

float leerCm(int i) {
  digitalWrite(TRIG[i], LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[i], LOW);
  unsigned long us = pulseIn(ECHO[i], HIGH, 30000UL);  // timeout 30 ms
  if (us == 0) return NAN;
  return (us * 0.0343f) / 2.0f;
}

int cmToPct(int i, float cm) {
  if (isnan(cm)) return lastPct[i];
  float vacio = VACIO_CM[i];
  float llen = LLENO_CM[i];
  if (vacio <= llen) return 0;
  float pct = (vacio - cm) / (vacio - llen) * 100.0f;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)(pct + 0.5f);
}

void pintarLcdPlastico() {
  lcd.setCursor(0, 0);
  lcd.print("Plastico        ");
  lcd.setCursor(0, 1);
  char line[17];
  if (isnan(lastCm[BIN_P])) {
    snprintf(line, sizeof(line), "Sin eco         ");
  } else {
    snprintf(line, sizeof(line), "%3d%% %s          ", lastPct[BIN_P], lleno[BIN_P] ? "LLENO" : "ok");
  }
  lcd.print(line);
}

void publicar(const char* lamp, bool stopAll) {
  // JSON una línea (Pi Serial o HTTP)
  char buf[420];
  snprintf(
    buf, sizeof(buf),
    "{\"plastico\":{\"cm\":%.1f,\"pct\":%d,\"lleno\":%s},"
    "\"aluminio\":{\"cm\":%.1f,\"pct\":%d,\"lleno\":%s},"
    "\"vidrio\":{\"cm\":%.1f,\"pct\":%d,\"lleno\":%s},"
    "\"rechazo\":{\"cm\":%.1f,\"pct\":%d,\"lleno\":%s},"
    "\"lamp\":\"%s\",\"stop\":%s,\"divert\":{\"plastico\":%s,\"aluminio\":%s,\"vidrio\":%s}}",
    isnan(lastCm[0]) ? -1.0 : lastCm[0], lastPct[0], lleno[0] ? "true" : "false",
    isnan(lastCm[1]) ? -1.0 : lastCm[1], lastPct[1], lleno[1] ? "true" : "false",
    isnan(lastCm[2]) ? -1.0 : lastCm[2], lastPct[2], lleno[2] ? "true" : "false",
    isnan(lastCm[3]) ? -1.0 : lastCm[3], lastPct[3], lleno[3] ? "true" : "false",
    lamp,
    stopAll ? "true" : "false",
    (lleno[0] && !lleno[3]) ? "true" : "false",
    (lleno[1] && !lleno[3]) ? "true" : "false",
    (lleno[2] && !lleno[3]) ? "true" : "false"
  );

  Serial.println(buf);

#if defined(ESP32)
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + PI_HOST + ":" + PI_PORT + "/api/bins";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(String(buf));
    Serial.printf("POST bins → %d\n", code);
    http.end();
  }
#endif
}

void setup() {
  Serial.begin(115200);
  delay(200);

  for (int i = 0; i < 4; i++) {
    pinMode(TRIG[i], OUTPUT);
    pinMode(ECHO[i], INPUT);
    digitalWrite(TRIG[i], LOW);
    pinMode(OUT_FULL[i], OUTPUT);
    digitalWrite(OUT_FULL[i], LOW);
  }
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("SIBU bins");
  lcd.setCursor(0, 1);
  lcd.print("Plastico LCD");

#if defined(ESP32)
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
    Serial.println("WiFi fail — solo Serial");
  }
#endif

  Serial.println("SIBU bins listo");
}

void loop() {
  for (int i = 0; i < 4; i++) {
    float cm = leerCm(i);
    // mediana simple: 2 lecturas
    delay(30);
    float cm2 = leerCm(i);
    if (!isnan(cm) && !isnan(cm2)) cm = (cm + cm2) / 2.0f;
    else if (isnan(cm)) cm = cm2;
    lastCm[i] = cm;
    lastPct[i] = cmToPct(i, cm);
    lleno[i] = lastPct[i] >= LLENO_PCT;
    digitalWrite(OUT_FULL[i], lleno[i] ? HIGH : LOW);
    delay(40);
  }

  bool stopAll = lleno[BIN_R];
  bool anyMatFull = lleno[BIN_P] || lleno[BIN_A] || lleno[BIN_V];
  const char* lamp = "green";
  if (stopAll) lamp = "red";
  else if (anyMatFull) lamp = "yellow";

  digitalWrite(LED_RED, stopAll ? HIGH : LOW);
  digitalWrite(LED_YELLOW, (!stopAll && anyMatFull) ? HIGH : LOW);
  digitalWrite(LED_GREEN, (!stopAll && !anyMatFull) ? HIGH : LOW);

  pintarLcdPlastico();
  publicar(lamp, stopAll);

  delay(400);
}
