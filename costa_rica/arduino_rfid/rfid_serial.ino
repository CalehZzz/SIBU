/*
 * SIBU — Arduino + RC522 → Serial USB → Raspberry Pi (RFID)
 *
 * La Pi lee el puerto (rfid_gate.py --serial /dev/ttyUSB0 o ACM0)
 * y desbloquea por cuenta en Firestore.
 *
 * Librería: MFRC522 by Miguel Balboa
 *
 * Cableado Uno / Nano (SPI):
 *   RC522     Arduino
 *   SDA/SS    D10
 *   SCK       D13
 *   MOSI      D11
 *   MISO      D12
 *   RST       D9
 *   3.3V      3.3V   (NO 5V)
 *   GND       GND
 *
 * Conectá el Arduino a un USB de la Pi. Serial 115200.
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  10
#define RST_PIN 9

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

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) { /* Uno nativo */ }
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("{\"ok\":true,\"rfid\":\"arduino\",\"msg\":\"SIBU RFID listo\"}");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(40);
    return;
  }

  String uid = uidToHex(mfrc522.uid);
  unsigned long now = millis();
  if (uid == lastUid && (now - lastTapMs) < 2000) {
    mfrc522.PICC_HaltA();
    return;
  }
  lastUid = uid;
  lastTapMs = now;

  // Una línea JSON por tap — la Pi la consume
  Serial.print("{\"uid\":\"");
  Serial.print(uid);
  Serial.println("\"}");

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(200);
}
