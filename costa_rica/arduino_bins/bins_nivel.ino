/*
 * SIBU — ESP32 · DEMO mesa
 *   - 2× HC-SR04: plástico + rechazo
 *   - 1× LCD I2C: % plástico (+ peso)
 *   - 1× HX711: báscula de plástico (ejemplo)
 *
 * WiFi → POST Pi :8082 /api/bins
 *
 * Librerías (Library Manager):
 *   - LiquidCrystal I2C (Frank de Brabander)
 *   - HX711 by bogde  (o "HX711 Arduino Library")
 *
 * Lógica lámpara (demo):
 *   - Plástico lleno → AMARILLA · divert plástico a rechazo
 *   - Rechazo lleno  → ROJA · stop total
 *   - OK → VERDE
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711.h>

// ===== CONFIG WiFi =====
const char* WIFI_SSID = "TU_HOTSPOT_O_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";
const char* PI_HOST   = "172.20.10.3";
const int   PI_PORT   = 8082;

// ===== HC-SR04 =====
// Índices: 0=plástico · 1=rechazo
const int TRIG[2] = {13, 33};
const int ECHO[2] = {12, 32};
const float VACIO_CM[2] = {40.0, 40.0};
const float LLENO_CM[2] = {8.0, 8.0};
const int   LLENO_PCT   = 90;

// ===== LCD I2C plástico =====
const uint8_t LCD_ADDR = 0x27;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// ===== HX711 báscula plástico =====
// DT → GPIO 26 · SCK → GPIO 25  (cambiá si hace falta)
const int HX_DT  = 26;
const int HX_SCK = 25;
// Calibración: ajustá con un peso conocido (gramos por unidad del HX711)
// Procedimiento: tare vacío → poné 100 g → SCALE = reading/100
float HX_SCALE = 420.0f;  // ← calibrar
HX711 scale;

// Salidas opcionales a PLC (opto)
const int OUT_FULL_P = 16;
const int OUT_FULL_R = 19;
const int LED_YELLOW = 4;
const int LED_RED    = 5;
const int LED_GREEN  = 15;

float lastCm[2] = {NAN, NAN};
int   lastPct[2] = {0, 0};
bool  lleno[2] = {false, false};
float lastKg = 0.0f;

float leerCm(int i) {
  digitalWrite(TRIG[i], LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[i], LOW);
  unsigned long us = pulseIn(ECHO[i], HIGH, 30000UL);
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

void pintarLcd() {
  lcd.setCursor(0, 0);
  char l0[17];
  snprintf(l0, sizeof(l0), "P:%3d%%%s        ", lastPct[0], lleno[0] ? " FULL" : "");
  lcd.print(l0);
  lcd.setCursor(0, 1);
  char l1[17];
  snprintf(l1, sizeof(l1), "%5.3fkg R:%3d%%  ", lastKg, lastPct[1]);
  lcd.print(l1);
}

void publicar(const char* lamp, bool stopAll) {
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
    String url = String("http://") + PI_HOST + ":" + PI_PORT + "/api/bins";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(String(buf));
    Serial.printf("POST bins → %d\n", code);
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  for (int i = 0; i < 2; i++) {
    pinMode(TRIG[i], OUTPUT);
    pinMode(ECHO[i], INPUT);
    digitalWrite(TRIG[i], LOW);
  }
  pinMode(OUT_FULL_P, OUTPUT);
  pinMode(OUT_FULL_R, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("SIBU ESP32");
  lcd.setCursor(0, 1);
  lcd.print("HX711+2xHC");

  scale.begin(HX_DT, HX_SCK);
  scale.set_scale(HX_SCALE);
  scale.tare(20);
  Serial.println("HX711 tare OK — calibrá HX_SCALE con peso conocido");

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
  }

  Serial.println("SIBU bins+scale listo (demo P+R)");
}

void loop() {
  // Ultrasonicos
  for (int i = 0; i < 2; i++) {
    float cm = leerCm(i);
    delay(25);
    float cm2 = leerCm(i);
    if (!isnan(cm) && !isnan(cm2)) cm = (cm + cm2) / 2.0f;
    else if (isnan(cm)) cm = cm2;
    lastCm[i] = cm;
    lastPct[i] = cmToPct(i, cm);
    lleno[i] = lastPct[i] >= LLENO_PCT;
    delay(30);
  }
  digitalWrite(OUT_FULL_P, lleno[0] ? HIGH : LOW);
  digitalWrite(OUT_FULL_R, lleno[1] ? HIGH : LOW);

  // Báscula (gramos → kg). get_units calibrado en gramos.
  if (scale.is_ready()) {
    float g = scale.get_units(8);
    if (g < 0) g = 0;
    lastKg = g / 1000.0f;
  }

  bool stopAll = lleno[1];           // rechazo
  bool divertP = lleno[0] && !stopAll;
  const char* lamp = "green";
  if (stopAll) lamp = "red";
  else if (divertP) lamp = "yellow";

  digitalWrite(LED_RED, stopAll ? HIGH : LOW);
  digitalWrite(LED_YELLOW, divertP ? HIGH : LOW);
  digitalWrite(LED_GREEN, (!stopAll && !divertP) ? HIGH : LOW);

  pintarLcd();
  publicar(lamp, stopAll);
  delay(350);
}
