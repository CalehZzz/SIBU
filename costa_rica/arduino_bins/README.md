# ESP32 · Botes (demo) + LCD + báscula HX711

## Arquitectura

```
ESP32 ─WiFi POST─→ Pi :8082 /api/bins → Firestore bins_pi/estado → web
  ├─ HC-SR04 plástico
  ├─ HC-SR04 rechazo
  ├─ LCD I2C ( % plástico + kg )
  └─ HX711 (peso plástico ejemplo)

Arduino Uno ─USB─→ Pi Serial → RFID  (ver ../arduino_rfid/)
```

Demo actual: **2 HC + 1 LCD + 1 báscula**. Aluminio/vidrio quedan en 0 en el JSON (próximos).

## Conexiones ESP32

### HC-SR04

| Sensor | TRIG | ECHO |
|---|---|---|
| Plástico | GPIO **13** | GPIO **12** |
| Rechazo | GPIO **33** | GPIO **32** |

VCC HC → 5 V (Echo con divisor a 3.3 V si hace falta) · GND común.

### LCD I2C 1602 (plástico)

| LCD | ESP32 |
|---|---|
| VCC | 5 V / 3V3 |
| GND | GND |
| SDA | **21** |
| SCL | **22** |
| Addr | `0x27` (o `0x3F`) |

### HX711 + celda de carga (plástico)

| HX711 | ESP32 |
|---|---|
| VCC | 5 V (o 3V3 según módulo) |
| GND | GND |
| DT  | GPIO **26** |
| SCK | GPIO **25** |
| E+ E− A+ A− | según celda (puente) |

### Lógica / PLC opcional

| Señal | GPIO |
|---|---|
| Full plástico | 16 |
| Full rechazo | 19 |
| LED amarillo / rojo / verde | 4 / 5 / 15 |

## Calibrar HX711

1. Flash · Serial 115200 · vacío en el plato → ya hace `tare`  
2. Poné un peso conocido (ej. 100 g)  
3. Ajustá `HX_SCALE` en el `.ino` hasta que el LCD muestre ~0.100 kg  
4. Re-flash  

## WiFi

Editá `WIFI_SSID`, `WIFI_PASS`, `PI_HOST` (IP de la Pi en el hotspot).

## Pi

```bash
BINS_PORT=8082   # sibu.env
bash costa_rica/install_services.sh bins
# o web (incluye bins)
```

## Librerías Arduino IDE

- LiquidCrystal I2C  
- HX711 (bogde)  
- Board: ESP32 Dev Module  
