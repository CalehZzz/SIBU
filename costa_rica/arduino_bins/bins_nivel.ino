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
 *
 * Diagnóstico Serial (115200):
 *   HC0 plastico  us=… cm=…   → si us=0 / cm=NAN: sin eco (cable/Echo/VCC)
 *   HC1 rechazo   us=… cm=…   → cm real; vacío debe ser ~altura del bote
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

// true = imprime us/cm cada ciclo (dejalo ON hasta que midan bien)
const bool DEBUG_HC = true;
// true = no pone lamp=red aunque rechazo diga lleno (solo para cablear/probar)
const bool DEMO_NO_STOP = false;

// ===== HC-SR04 =====
// Índices: 0=plástico · 1=rechazo
// Pines NUEVOS (GPIO 12 era strapping y fallaba mucho el eco de plástico).
//   Plástico: TRIG 18 · ECHO 19
//   Rechazo:  TRIG 33 · ECHO 32
const int TRIG[2] = {18, 33};
const int ECHO[2] = {19, 32};

// Calibrá con bote VACÍO: mirá "cm=" en Serial y copiá ese valor a VACIO_CM.
// LLENO_CM = distancia cuando el material llega arriba (casi tocando el sensor).
const float VACIO_CM[2] = {40.0, 40.0};
const float LLENO_CM[2] = {8.0, 8.0};
const int   LLENO_PCT   = 90;
// Lecturas < MIN_CM o > MAX_CM se tratan como fallo (no “lleno”)
const float MIN_CM = 2.0f;
const float MAX_CM = 400.0f;

// ===== LCD I2C plástico =====
const uint8_t LCD_ADDR = 0x27;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// ===== HX711 báscula plástico =====
const int HX_DT  = 26;
const int HX_SCK = 25;
float HX_SCALE = 420.0f;
HX711 scale;

const int OUT_FULL_P = 16;
const int OUT_FULL_R = 27;  // era 19; 19 ahora es ECHO plástico
const int LED_YELLOW = 4;
const int LED_RED    = 5;
const int LED_GREEN  = 15;

float lastCm[2] = {NAN, NAN};
int   lastPct[2] = {0, 0};
bool  lleno[2] = {false, false};
bool  sensorOk[2] = {false, false};
float lastKg = 0.0f;

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
  if (isnan(cm)) return 0;  // sin eco → no inventar “lleno”
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
  if (!sensorOk[0]) snprintf(l0, sizeof(l0), "P:FAIL cable   ");
  else snprintf(l0, sizeof(l0), "P:%3d%%%s        ", lastPct[0], lleno[0] ? " FULL" : "");
  lcd.print(l0);
  lcd.setCursor(0, 1);
  char l1[17];
  if (!sensorOk[1]) snprintf(l1, sizeof(l1), "R:FAIL %5.3fkg", lastKg);
  else snprintf(l1, sizeof(l1), "%5.3fkg R:%3d%%  ", lastKg, lastPct[1]);
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
  lcd.print("HC 18/19 + 33/32");

  scale.begin(HX_DT, HX_SCK);
  scale.set_scale(HX_SCALE);
  scale.tare(20);
  Serial.println("HX711 tare OK — calibrá HX_SCALE con peso conocido");
  Serial.println("HC pines: P TRIG=18 ECHO=19 | R TRIG=33 ECHO=32");
  Serial.println("Poné la mano a ~20 cm de cada sensor: cm debe moverse.");

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
  const char* nombre[2] = {"plastico", "rechazo"};
  for (int i = 0; i < 2; i++) {
    unsigned long us1 = 0, us2 = 0;
    float cm = leerCm(i, &us1);
    delay(40);  // menos crosstalk entre los 2 HC
    float cm2 = leerCm(i, &us2);
    if (!isnan(cm) && !isnan(cm2)) cm = (cm + cm2) / 2.0f;
    else if (isnan(cm)) cm = cm2;

    lastCm[i] = cm;
    sensorOk[i] = !isnan(cm);
    lastPct[i] = cmToPct(i, cm);
    // solo “lleno” si el sensor midió de verdad
    lleno[i] = sensorOk[i] && (lastPct[i] >= LLENO_PCT);

    if (DEBUG_HC) {
      if (sensorOk[i]) {
        Serial.printf("HC%d %-8s us=%lu/%lu cm=%.1f pct=%d%s\n",
                      i, nombre[i], us1, us2, cm, lastPct[i],
                      lleno[i] ? " FULL" : "");
      } else {
        Serial.printf("HC%d %-8s FAIL us=%lu/%lu (sin eco o fuera de rango) — revisá TRIG/ECHO/VCC/GND\n",
                      i, nombre[i], us1, us2);
      }
    }
    delay(40);
  }
  digitalWrite(OUT_FULL_P, lleno[0] ? HIGH : LOW);
  digitalWrite(OUT_FULL_R, lleno[1] ? HIGH : LOW);

  if (scale.is_ready()) {
    float g = scale.get_units(8);
    if (g < 0) g = 0;
    lastKg = g / 1000.0f;
  }

  bool stopAll = lleno[1] && !DEMO_NO_STOP;
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
