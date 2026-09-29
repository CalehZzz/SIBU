# RFID RC522 + ESP32 → candado por cuenta

## Flujo

```
Tarjeta → RC522 → ESP32 --WiFi POST--> Pi :8081/api/rfid
                                         ↓
                    1) ¿hay rfid_link_pending/{authUid}? → vincula tarjeta
                    2) si no: busca rfid_tarjetas/{UID} o allowlist
                                         ↓
                    rfid_gate/{authUid} { unlocked, untilMs }
                                         ↓
                    Solo ESA cuenta Google desbloquea HMI / 📷 / 🔌
```

Cada operador abre **su** sesión. Otra persona logueada no ve el desbloqueo.

## 1) Pi — servicio

En `costa_rica/sibu.env`:

```bash
RFID_PORT=8081
RFID_TTL_SEC=300
# Opcional: CARDUID:Nombre:FIREBASE_AUTH_UID
# (sin el 3er campo hay que Vincular una vez desde la web)
RFID_ALLOW=A7240B9F:SIBUA
```

O editá `costa_rica/rfid_allow.txt`.

```bash
cd ~/SIBU
git pull
bash costa_rica/install_services.sh rfid
# o: bash costa_rica/install_services.sh web
```

Probar:

```bash
# Sin vínculo → 403 "tarjeta sin cuenta"
curl -X POST http://127.0.0.1:8081/api/rfid -H 'Content-Type: application/json' -d '{"uid":"A7240B9F"}'

# Tras Vincular en la web (o con authUid en allowlist) → 200 + authUid
```

## 2) Primera vez — vincular tarjeta a tu Google (permanente)

1. Entrá a SIBU con tu cuenta Google (se muestra el **correo** en el modal).
2. Tocá el pill **🔒 RFID** (o intentá abrir HMI/📷/🔌).
3. Pulsá **Vincular mi tarjeta**.
4. En ≤ 90 s acercá la tarjeta al RC522.
5. Queda **para siempre** en esa cuenta: otra cuenta no puede usarla ni re-vincularla.

Si acercás la tarjeta sin vincular, la web ahora muestra el feedback del lector
(`rfid_scan/last`): p.ej. “Leí A724… — tocá Vincular”.

En el Serial del ESP32 deberías ver `POST … → 200` (no solo el UID).
Si ves `403`, la Pi recibió el tap pero rechazó (falta vincular / otra cuenta).

Reiniciá el servicio tras `git pull`:

```bash
sudo systemctl restart sibu-rfid-gate
journalctl -u sibu-rfid-gate -f
```

## 3) ESP32

Abrí `esp32_rfid.ino` en Arduino IDE:

- Board: ESP32 Dev Module  
- Librería: **MFRC522**  
- Editá `WIFI_SSID`, `WIFI_PASS`, `PI_HOST`

Cableado: ver comentarios del `.ino` (3.3 V, no 5 V).  
ESP32 y Pi en **la misma WiFi/hotspot**.

## 4) Web — uso diario

1. Login Google → acercá **tu** tarjeta → entrá a HMI / cámara / PLC.
2. El pill muestra el tiempo restante (`🔓 4:32`).
3. Tocá el pill → **🔒 Bloquear ahora** (o esperá a que expire).
4. “Seguir trabajando” solo cierra el modal sin bloquear.

## 5) Reglas Firestore

```
match /rfid_gate/{uid} {
  allow read, write: if request.auth != null && request.auth.uid == uid;
}
match /rfid_link_pending/{uid} {
  allow read, write: if request.auth != null && request.auth.uid == uid;
}
match /rfid_scan/{doc} {
  allow read: if request.auth != null;
  allow write: if false; // Pi Admin SDK
}
match /rfid_tarjetas/{cardUid} {
  allow read: if request.auth != null && resource.data.authUid == request.auth.uid;
  allow write: if request.auth.token.admin == true;
}
```

Desplegar: `firebase deploy --only firestore:rules,hosting`
