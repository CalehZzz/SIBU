/*
 * SIBU — Arduino + RC522 → Serial USB → Raspberry Pi
 *
 * Cableado Uno/Nano:
 *   SDA/SS→D10  SCK→D13  MOSI→D11  MISO→D12  RST→D9
 *   VCC→3.3V   GND→GND
 *
 * Serial Monitor: 115200
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);
String lastUid = "";
unsigned long lastTapMs = 0;
unsigned long lastBeatMs = 0;

String uidToHex(MFRC522::Uid uid) {
  String s = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2500) {}

  SPI.begin();
  mfrc522.PCD_Init();
  delay(100);
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("version=0x"));
  Serial.println(v, HEX);

  if (v == 0x00 || v == 0xFF) {
    Serial.println(F("FAIL SPI — revisa 3.3V GND D10/D11/D12/D13/D9"));
  } else {
    Serial.println(F("RC522 OK — acercá la tarjeta"));
  }
}

void loop() {
  // Heartbeat legible en el IDE
  if (millis() - lastBeatMs > 2000) {
    lastBeatMs = millis();
    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    Serial.print(F("esperando... version=0x"));
    Serial.println(v, HEX);
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  String uid = uidToHex(mfrc522.uid);
  if (uid == lastUid && (millis() - lastTapMs) < 1500) {
    mfrc522.PICC_HaltA();
    return;
  }
  lastUid = uid;
  lastTapMs = millis();

  // Línea simple para el IDE + JSON para la Pi
  Serial.print(F("UID: "));
  Serial.println(uid);
  Serial.print(F("{\"uid\":\""));
  Serial.print(uid);
  Serial.println(F("\"}"));

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(300);
}
