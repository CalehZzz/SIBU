# ESP32 · mesa unificada (botes + báscula + RFID)

Un solo ESP32. **No hace falta Arduino** para la tarjeta.

```
ESP32
  ├─ 2× HC-SR04     ─┐
  ├─ LCD I2C        ├─→ WiFi POST Pi :8082 /api/bins  → bins_pi/estado
  ├─ HX711          ─┘
  └─ RC522 RFID     ──→ WiFi POST Pi :8081 /api/rfid  → unlock web
```

## ¿Se satura el ESP32?

**No.** Carga típica:

| Tarea | Cada cuánto | Costo |
|---|---|---|
| Poll RC522 | ~15–20 ms | casi nada |
| 2× HC-SR04 | ~400 ms | ~60–100 ms bloqueantes |
| HX711 | con los bins | bajo |
| POST bins | ~400 ms | 1 HTTP corto |
| POST RFID | solo al tap | 1 HTTP |

Es una fracción de lo que aguanta un ESP32. Si más adelante sumás 2 HC más, seguí bien; si notás lag en el tap, bajá `DEBUG_HC` o subí el intervalo de bins a 600 ms.

---

## Pines

### HC-SR04

| Sensor | TRIG | ECHO |
|---|---|---|
| Plástico | **18** | **19** |
| Rechazo | **33** | **32** |

VCC → 5 V · GND común.

### RC522 (RFID) — **3.3 V nada más**

| RC522 | ESP32 |
|---|---|
| SDA / SS | **5** |
| SCK | **14** |
| MOSI | **13** |
| MISO | **23** |
| RST | **17** |
| 3.3V | **3.3V** |
| GND | GND |

### LCD I2C

SDA **21** · SCL **22** · addr `0x27`

### HX711

DT **26** · SCK **25**

### LEDs / full (opcionales)

| Señal | GPIO |
|---|---|
| Full plástico | 16 |
| Full rechazo | 27 |
| LED amarillo | 4 |
| LED rojo | **2** (antes era 5 = ahora SS RFID) |
| LED verde | 15 |

---

## Flash

1. IDE → **ESP32 Dev Module**  
2. Librerías: LiquidCrystal I2C · HX711 · **MFRC522**  
3. Editá WiFi + `PI_HOST` en `bins_nivel.ino`  
4. Subí el sketch  

Serial 115200:

```
RC522 version=0x91 OK
...
RFID uid=A7240B9F
POST rfid → 200 ...
POST bins → 200
```

Si `version=0x00` / `0xFF`: cableado RC522 o alimentaste con 5 V.

---

## Pi (sin Arduino)

```bash
# sibu.env — dejá RFID_SERIAL vacío o comentado (solo HTTP)
# RFID_PORT=8081
# BINS_PORT=8082

bash costa_rica/install_services.sh web   # rfid + bins (+ visión)
# o:
bash costa_rica/install_services.sh rfid
bash costa_rica/install_services.sh bins

journalctl -u sibu-rfid-gate -u sibu-bins -f
```

Prueba RFID sin tarjeta (desde PC/Pi):

```bash
curl -s -X POST http://IP_PI:8081/api/rfid \
  -H 'Content-Type: application/json' \
  -d '{"uid":"A7240B9F"}'
```

---

## Diagnóstico rápido

| Síntoma | Qué mirar |
|---|---|
| `plastico.cm=-1` | HC TRIG/ECHO/VCC |
| rechazo siempre ~3 cm / rojo | sensor cerca de tapa; calibrá `VACIO_CM` |
| RC522 `0x00` | 3.3V · SS5 SCK14 MOSI13 MISO23 RST17 |
| Tap OK en Serial, web no | `sibu-rfid-gate` · `PI_HOST` · puerto **8081** |
| Bins OK, RFID no | WiFi OK pero POST a 8081; firewall / servicio |

Arduino Uno + RC522 queda como **legado** en `../arduino_rfid/`.
