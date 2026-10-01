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

En el módulo el pin suele decir **SDA** (a veces NSS/SS). Es el **chip-select SPI**, no I2C: va a GPIO **5**, no al 21 del LCD.

| RC522 (serigrafía) | ESP32 | Nota |
|---|---|---|
| **SDA** / SS / NSS | **5** | chip select |
| SCK | **14** | |
| MOSI | **13** | |
| MISO | **23** | |
| RST / RESET | **27** | no uses 15 (strapping → resets) |
| 3.3V | **3.3V** | nunca 5 V |
| GND | GND | |

### LCD I2C

SDA **21** · SCL **22** · addr `0x27`

### HX711 — módulo **Load Cell Amp / Load Cell Amplifier**

Es el amplificador verde/azul con bornes a un lado y pines al otro. El sketch ya apunta a ese módulo (librería HX711 + canal A).

```
Load cell (4 hilos) → [ E+ E− A− A+ ] HX711 [ VCC DT SCK GND ] → ESP32
```

| Serigrafía HX711 | Va a |
|---|---|
| **VCC** | **5V** ESP |
| **GND** | GND |
| **DT** o DOUT | GPIO **26** |
| **SCK** o PD_SCK | GPIO **25** |
| **E+** | rojo celda |
| **E−** | negro |
| **A+** | verde (o blanco; según celda) |
| **A−** | blanco (o verde) |
| B+ / B− | no conectar (canal B) |

### Si `pesoKg` siempre 0.0000

1. Re-flash con `DEBUG_HX = true` (ya viene ON).  
2. Serial 115200 — buscá `HX raw=` o `HX FAIL`.

| Serial | Qué hacer |
|---|---|
| `HX FAIL not ready` | VCC **5V**, DT↔26, SCK↔25, GND; no cruces DT/SCK |
| `raw=` no cambia al poner peso | Celda mal en E/A; aflojá tornillos / otra celda |
| `raw=` cambia pero `units≈0` o absurdo | **Calibrá `HX_SCALE`** |

### Calibrar

1. Vacío al boot (hace tare).  
2. Poné **100 g** conocidos.  
3. Anotá `units=XXXg` en Serial.  
4. Nuevo factor: `HX_SCALE = HX_SCALE * units / 100`  
   Ejemplo: SCALE era 420 y units=850 → `HX_SCALE = 420 * 850 / 100 = 3570`.  
5. Re-flash · vacío de nuevo · 100 g debe dar ~0.100 kg en LCD.

### LEDs / full (opcionales)

| Señal | GPIO |
|---|---|
| Full plástico | off (`-1`) |
| Full rechazo | off |
| LED amarillo | 4 |
| LED rojo | **2** (GPIO 5 = SDA/SS del RC522) |
| LED verde | off |

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
