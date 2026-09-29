/*
 * SIBU — ESP32 + RC522 → POST UID a la Raspberry Pi
 *
 * Librerías (Arduino Library Manager):
 *   - MFRC522 by GithubCommunity / Miguel Balboa
 *   - HTTPClient (incluida en ESP32 core)
 *
 * Cableado típico (ESP32 VSPI):
 *   RC522    ESP32
 *   SDA/SS   GPIO 5
 *   SCK      GPIO 18
 *   MOSI     GPIO 23
 *   MISO     GPIO 19
 *   GND      GND
 *   RST      GPIO 22
 *   3.3V     3.3V   (NO 5V)
 *
 * Configurá WIFI + IP de la Pi abajo, flash, y acercá la tarjeta.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// ===== CONFIGURÁ ESTO =====
const char* WIFI_SSID = "TU_HOTSPOT_O_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";
// IP de la Pi en la misma red (wlan hotspot o LAN)
const char* PI_HOST   = "172.20.10.3";  // hostname -I en la Pi (cambia si el hotspot renueva IP)
const int   PI_PORT   = 8081;
// ==========================

#define SS_PIN  5
#define RST_PIN 22

MFRC522 mfrc522(SS_PIN, RST_PIN);
String lastUid = "";
unsigned long lastTapMs = 0;

String uidToHex(MFRC522::Uid uid) {
  String s = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

bool postUid(const String& uid) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://") + PI_HOST + ":" + PI_PORT + "/api/rfid";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"uid\":\"") + uid + "\"}";
  int code = http.POST(body);
  String resp = http.getString();
  Serial.printf("POST %s → %d %s\n", url.c_str(), code, resp.c_str());
  http.end();
  return code >= 200 && code < 300;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("SIBU RFID RC522 listo");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  String uid = uidToHex(mfrc522.uid);
  unsigned long now = millis();
  // anti-rebote: misma tarjeta < 2 s
  if (uid == lastUid && (now - lastTapMs) < 2000) {
    mfrc522.PICC_HaltA();
    return;
  }
  lastUid = uid;
  lastTapMs = now;

  Serial.print("UID: ");
  Serial.println(uid);
  bool ok = postUid(uid);
  Serial.println(ok ? "OK enviado a Pi" : "FALLÓ envío");

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(300);
}
