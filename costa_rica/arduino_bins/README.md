# ESP32 · Botes (demo) + LCD + báscula HX711

## Arquitectura

```
ESP32 ─WiFi POST─→ Pi :8082 /api/bins → Firestore bins_pi/estado → web
  ├─ HC-SR04 plástico
  ├─ HC-SR04 rechazo
  ├─ LCD I2C ( % plástico + kg )
  └─ HX711 (peso plástico ejemplo)
```

## Si ves esto en el Serial

```json
"plastico":{"cm":-1.0,...}, "rechazo":{"cm":3.0,"pct":100,"lleno":true}, "lamp":"red"
```

| Campo | Significado |
|---|---|
| `plastico.cm = -1` | **Sin eco** en HC plástico (TRIG/ECHO/VCC/GND o pin malo) |
| `rechazo.cm ≈ 3–5` | El HC rechazo **sí mide**, pero algo está a ~3 cm → lo toma como **lleno** → rojo |
| `lamp=red` / `stop=true` | Consecuencia de rechazo “lleno” |

No es la Pi ni Firestore: es el HC / montaje / calibración.

### Qué hacer YA

1. `git pull` y **re-flash** `bins_nivel.ino` (pines nuevos + debug).
2. Serial 115200 — buscá líneas `HC0 plastico` / `HC1 rechazo`.
3. **Aire libre** (no dentro del bote): poné la mano a ~20 cm de cada sensor.

| Resultado | Acción |
|---|---|
| `HC0 FAIL us=0` | Cableá de nuevo plástico: TRIG **18** · ECHO **19** · VCC **5V** · GND |
| `HC1 cm=3` fijo sin nada delante | Sensor mirando a una pared/tapa a 3 cm, o Echo mal; apuntá al techo/aire |
| Mano a 20 cm → `cm≈20` | Sensor OK → montá en bote vacío, copiá ese `cm` a `VACIO_CM` |
| Intercambiás los 2 módulos y el FAIL se mueve | El módulo está malo |
| Intercambiás y el FAIL se queda en plástico | Cable/pin de ese canal |

### Pines (actualizados — GPIO 12 ya no)

| Sensor | TRIG | ECHO |
|---|---|---|
| Plástico | GPIO **18** | GPIO **19** |
| Rechazo | GPIO **33** | GPIO **32** |

VCC ambos HC → **5 V** · GND común con ESP32.  
Echo es 5 V: ideal divisor 1k+2k a 3.3 V (si no, a veces anda igual un rato).

### Calibrar % del bote

1. Bote **vacío**, sensor arriba mirando al fondo → anotá `cm=` (ej. 32).  
2. En el `.ino`: `VACIO_CM[i] = 32`.  
3. `LLENO_CM[i]` ≈ distancia con el bote casi lleno (ej. 8).  
4. Re-flash.

Si vacío te da 3 cm, el sensor **no** está mirando el fondo del bote (está tapado o muy cerca de una cara).

Para probar sin rojo permanente: `DEMO_NO_STOP = true` en el `.ino`.

---

## Conexiones resto

### LCD I2C 1602

| LCD | ESP32 |
|---|---|
| VCC | 5 V / 3V3 |
| GND | GND |
| SDA | **21** |
| SCL | **22** |
| Addr | `0x27` (o `0x3F`) |

### HX711

| HX711 | ESP32 |
|---|---|
| DT | **26** |
| SCK | **25** |

### Salidas opcionales

| Señal | GPIO |
|---|---|
| Full plástico | 16 |
| Full rechazo | **27** |
| LED Y / R / G | 4 / 5 / 15 |

## WiFi

Editá `WIFI_SSID`, `WIFI_PASS`, `PI_HOST` (IP de la Pi).

## Pi

```bash
bash costa_rica/install_services.sh bins
journalctl -u sibu-bins -f
```

## Librerías

- LiquidCrystal I2C · HX711 (bogde) · Board: **ESP32 Dev Module**
