# RFID RC522 + ESP32 → candado HMI / cámara

## Flujo

```
Tarjeta → RC522 → ESP32 --WiFi POST--> Pi :8081/api/rfid
                                         ↓
                              valida UID (env o Firestore)
                                         ↓
                              rfid_gate/sibu { unlocked, untilMs }
                                         ↓
                              Web SIBU desbloquea HMI / 📷 / 🔌
```

## 1) Pi — servicio

En `costa_rica/sibu.env`:

```bash
RFID_PORT=8081
RFID_TTL_SEC=300
# UIDs permitidos (hex sin : ). Varias separadas por coma.
RFID_ALLOW=A1B2C3D4:Caleb,DEADBEEF:Carla
```

```bash
cd ~/SIBU
git pull
bash costa_rica/install_services.sh rfid
# o: bash costa_rica/install_services.sh web   # (incluye rfid + vision firebase)
```

Probar a mano:

```bash
curl -X POST http://127.0.0.1:8081/api/rfid -H 'Content-Type: application/json' -d '{"uid":"A1B2C3D4"}'
```

## 2) Cómo saber el UID de tu tarjeta

1. Flash del `.ino` con Serial Monitor 115200  
2. Acercá la tarjeta → imprime `UID: ...`  
3. Copiá ese hex a `RFID_ALLOW`

O en Firestore: colección `rfid_tarjetas`, documento id = UID hex:

```json
{ "activa": true, "nombre": "Operador 1" }
```

## 3) ESP32

Abrí `esp32_rfid.ino` en Arduino IDE:

- Board: ESP32 Dev Module  
- Librería: **MFRC522**  
- Editá `WIFI_SSID`, `WIFI_PASS`, `PI_HOST` (IP de la Pi en esa WiFi)

Cableado: ver comentarios del `.ino` (3.3 V, no 5 V).

ESP32 y Pi en **la misma WiFi/hotspot**.

## 4) Web

Vistas protegidas: **HMI**, **📷**, **🔌**.  
Al entrar sin desbloqueo: “Acerque su tarjeta RFID”.  
Tras tap válido: acceso por `RFID_TTL_SEC` (default 5 min). Botón **Bloquear**.

## 5) Reglas Firestore

```
match /rfid_gate/{doc} {
  allow read: if request.auth != null;
  allow write: if false; // solo la Pi (Admin SDK)
}
match /rfid_tarjetas/{uid} {
  allow read: if request.auth != null;
  allow write: if request.auth.token.admin == true;
}
```
