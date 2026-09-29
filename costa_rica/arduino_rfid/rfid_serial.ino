/*
 * SIBU — Arduino + RC522 → Serial USB → Raspberry Pi (RFID)
 *
 * Librería: MFRC522 by Miguel Balboa
 *
 * Cableado Uno / Nano (SPI hardware):
 *   RC522     Arduino
 *   SDA/SS    D10
 *   SCK       D13
 *   MOSI      D11
 *   MISO      D12
 *   RST       D9
 *   3.3V      3.3V   (NO 5V en VCC del RC522)
 *   GND       GND
 *
 * Serial Monitor: 115200 baud · Both NL & CR
 * Al arrancar DEBE decir version=0x91 o 0x92. Si dice 0x00 / 0xFF → cableado/alimentación.
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);
String lastUid = "";
unsigned long lastTapMs = 0;
unsigned long lastBeatMs = 0;
bool readerOk = false;

String uidToHex(MFRC522::Uid uid) {
  String s = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

bool checkReader() {
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print("{\"diag\":\"rc522\",\"version\":\"");
  Serial.print(v < 0x10 ? "0x0" : "0x");
  Serial.print(v, HEX);
  Serial.print("\"");

  // 0x91 / 0x92 = OK · 0x88 a veces clone · 0x00/0xFF = no comunica
  if (v == 0x00 || v == 0xFF) {
    Serial.println(",\"ok\":false,\"error\":\"sin comunicacion SPI — revisa 3.3V, GND, SDA=D10, SCK/MOSI/MISO, RST=D9\"}");
    return false;
  }
  Serial.println(",\"ok\":true}");
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);
  delay(50);

  SPI.begin();
  mfrc522.PCD_Init();
  delay(50);
  mfrc522.PCD_Init();  // segundo init ayuda en clones

  // Antena a máxima ganancia (mejor lectura)
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  readerOk = checkReader();
  if (readerOk) {
    Serial.println("{\"ok\":true,\"rfid\":\"arduino\",\"msg\":\"acercá la tarjeta al RC522\"}");
  } else {
    Serial.println("{\"ok\":false,\"msg\":\"RC522 no responde — no va a leer tarjetas hasta arreglar cableado\"}");
  }
}

void loop() {
  // Re-chequeo periódico si falló el init
  if (!readerOk && (millis() - lastBeatMs > 3000)) {
    lastBeatMs = millis();
    mfrc522.PCD_Init();
    readerOk = checkReader();
    return;
  }

  if (readerOk && (millis() - lastBeatMs > 2000)) {
    lastBeatMs = millis();
    Serial.println("{\"beat\":true,\"msg\":\"esperando tarjeta\"}");
  }

  // A veces el primer poll falla: intentar dos veces
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(30);
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(30);
    return;
  }

  String uid = uidToHex(mfrc522.uid);
  unsigned long now = millis();
  if (uid == lastUid && (now - lastTapMs) < 1500) {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }
  lastUid = uid;
  lastTapMs = now;

  Serial.print("{\"uid\":\"");
  Serial.print(uid);
  Serial.println("\"}");

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(250);
}
