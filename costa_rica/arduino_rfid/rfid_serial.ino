/*
 * SIBU — Arduino + RC522 → Serial USB → Raspberry Pi (RFID)
 *
 * Librería: MFRC522 by Miguel Balboa
 *
 * Cableado Uno / Nano:
 *   SDA/SS→D10  SCK→D13  MOSI→D11  MISO→D12  RST→D9
 *   VCC→3.3V (mejor regulador 3.3V externo)  GND→GND
 *
 * Serial 115200. Si ves beat pero no uid → alimentación/antena del RC522.
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
int failReads = 0;

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
  Serial.print(F("{\"diag\":\"rc522\",\"version\":\"0x"));
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
  Serial.print(F("\""));
  if (v == 0x00 || v == 0xFF) {
    Serial.println(F(",\"ok\":false,\"error\":\"SPI muerto — 3.3V/GND/cables\"}"));
    return false;
  }
  Serial.println(F(",\"ok\":true}"));
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(20);
  digitalWrite(RST_PIN, HIGH);
  delay(50);

  SPI.begin();
  // Clones: SPI más lento suele estabilizar
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  mfrc522.PCD_Init();
  delay(20);
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  mfrc522.PCD_AntennaOn();
  SPI.endTransaction();

  readerOk = checkReader();
  if (readerOk) {
    Serial.println(F("{\"ok\":true,\"msg\":\"RC522 OK — acercá tarjeta 1cm al chip/antena\"}"));
  }
}

void loop() {
  if (!readerOk) {
    if (millis() - lastBeatMs > 2500) {
      lastBeatMs = millis();
      mfrc522.PCD_Init();
      readerOk = checkReader();
    }
    delay(50);
    return;
  }

  // Lectura agresiva: varios intentos por ciclo
  bool got = false;
  for (int attempt = 0; attempt < 3 && !got; attempt++) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      got = true;
      break;
    }
    // Segunda chance: a veces el "new" se come el primer poll
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      got = true;
      break;
    }
    delay(5);
  }

  if (got) {
    String uid = uidToHex(mfrc522.uid);
    unsigned long now = millis();
    if (!(uid == lastUid && (now - lastTapMs) < 1200)) {
      lastUid = uid;
      lastTapMs = now;
      Serial.print(F("{\"uid\":\""));
      Serial.print(uid);
      Serial.println(F("\"}"));
      Serial.flush();
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(200);
    return;
  }

  if (millis() - lastBeatMs > 2000) {
    lastBeatMs = millis();
    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    Serial.print(F("{\"beat\":true,\"version\":\"0x"));
    if (v < 0x10) Serial.print('0');
    Serial.print(v, HEX);
    Serial.println(F("\",\"msg\":\"esperando tarjeta — pegala al centro del RC522\"}"));
    Serial.flush();
    if (v == 0x00 || v == 0xFF) readerOk = false;
  }
  delay(20);
}
