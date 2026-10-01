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

```
Load cell (4 hilos) → [ E+ E− A− A+ ] HX711 [ VCC DT SCK GND ] → ESP32
```

#### Colores de la celda (lo habitual)

| Cable celda | Borne HX711 | Qué es |
|---|---|---|
| **Rojo** | **E+** | excitación + |
| **Negro** | **E−** | excitación − |
| **Verde** | **A+** | señal + |
| **Blanco** | **A−** | señal − |

Verde y blanco son la **señal diferencial** (no son VCC/GND). Si `raw` no cambia al cargar, **intercambiá verde ↔ blanco**.

#### ESP sin pin 5V

**No estás regado.** Alimentá el HX711 con **3V3** del ESP (el chip aguanta 2.6–5.5 V). La señal es un poco más débil; calibrá `HX_SCALE` igual.

| Serigrafía HX711 | Va a |
|---|---|
| **VCC** | **3V3** (o **VIN**/5V si tu placa lo trae y está alimentada por USB) |
| **GND** | GND |
| **DT** / DOUT | GPIO **26** |
| **SCK** / PD_SCK | GPIO **25** |
| B+ / B− | no conectar |

### Si el peso “anda pero mal” (mismo valor / números locos)

1. Serial: mirá `delta=` vacío vs con peso.  
2. **`delta` casi 0** → problema **mecánico / 3D**, no del código:  
   - Un extremo de la celda fijo a la base, el otro **solo** a la plataforma.  
   - La plataforma **no** debe tocar paredes ni tornillos que salten la celda.  
3. **`delta` cambia mucho** pero kg loco → calibrá:  
   `HX_SCALE = |delta| / gramos` (ej. delta=45000 con 100 g → `HX_SCALE = 450.0`).  
4. Si al cargar el raw **baja**, poné `HX_INVERT = true` o intercambiá verde/blanco.

### Si el RFID dejó de leer

- Sacá la tarjeta y volvé a acercarla (tras Halt no relee pegada).  
- El sketch re-init del RC522 cada ~7 s y después de cada HTTP (WiFi a veces lo duerme).  
- Serial: `RFID uid=` o `RFID kick … FAIL`.  
- 3.3V estable; cables cortos SS/SCK/MOSI/MISO/RST.

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
