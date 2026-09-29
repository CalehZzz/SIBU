# Botes · 4× HC-SR04 + LCD I2C (Arduino / ESP32)

## Qué hace

| Sensor | Bote |
|---|---|
| HC0 | Plástico |
| HC1 | Aluminio (latas) |
| HC2 | Vidrio |
| HC3 | **Rechazo** (desconocido / desviados) |

- LCD I2C (por ahora **solo plástico**, addr `0x27`) muestra `%` llenado.
- Si P/A/V está **lleno** (≥90 %) → lámpara **amarilla** · ese material se **desvía a rechazo** (ladder).
- Si **rechazo** está lleno → lámpara **roja** · **stop total**.
- Publica JSON a la Pi (`:8082`) → Firestore `bins_pi/estado` → panel web.

## Conexiones — ESP32 (recomendado, WiFi)

| HC-SR04 | ESP32 |
|---|---|
| VCC | **5 V** (o 3V3 si el módulo aguanta; muchos HC-SR04 quieren 5 V en VCC y nivel OK en Echo con divisor) |
| GND | GND |
| Plastico TRIG/ECHO | GPIO **13** / **12** |
| Aluminio TRIG/ECHO | GPIO **14** / **27** |
| Vidrio TRIG/ECHO | GPIO **26** / **25** |
| Rechazo TRIG/ECHO | GPIO **33** / **32** |

| LCD I2C 1602 | ESP32 |
|---|---|
| VCC | 5 V (o 3V3 según módulo) |
| GND | GND |
| SDA | GPIO **21** |
| SCL | GPIO **22** |
| Addr | `0x27` (o `0x3F` — cambiá en el `.ino`) |

| Señal | GPIO | A PLC (opcional, vía optoacoplador) |
|---|---|---|
| Full plástico | 16 | `I_BinFullPlastico` |
| Full aluminio | 17 | `I_BinFullAluminio` |
| Full vidrio | 18 | `I_BinFullVidrio` |
| Full rechazo | 19 | `I_BinFullRechazo` |
| LED amarillo local | 4 | — |
| LED rojo local | 5 | — |
| LED verde local | 15 | — |

> Echo a 5 V → divisor 1k/2k hacia GPIO si alimentás el HC a 5 V.

En el `.ino` editá `WIFI_SSID`, `WIFI_PASS`, `PI_HOST`.

## Conexiones — Arduino Uno / Nano (Serial USB)

| HC-SR04 | Uno |
|---|---|
| Plastico TRIG/ECHO | **2** / **3** |
| Aluminio | **4** / **5** |
| Vidrio | **6** / **7** |
| Rechazo | **8** / **9** |
| LCD SDA/SCL | **A4** / **A5** |
| Full P/A/V/R | **10** / **11** / **12** / **A0** |
| LED Y/R/G | **A1** / **A2** / **A3** |

La Pi lee el Serial JSON (`bins_firebase.py --serial /dev/ttyUSB0`) o usá ESP32 con WiFi.

## Calibración

Con el bote vacío medí cm en Serial → `VACIO_CM[]`.  
Casi lleno → `LLENO_CM[]`. Ajustá `LLENO_PCT` (default 90).

## Pi

```bash
# en sibu.env
BINS_PORT=8082

cd ~/SIBU && git pull
bash costa_rica/install_services.sh bins
# o: bash costa_rica/install_services.sh web   # incluye bins
```

Probar:

```bash
curl -X POST http://127.0.0.1:8082/api/bins -H 'Content-Type: application/json' \
  -d '{"plastico":{"cm":12,"pct":70,"lleno":false},"aluminio":{"cm":20,"pct":40,"lleno":false},"vidrio":{"cm":15,"pct":55,"lleno":false},"rechazo":{"cm":30,"pct":10,"lleno":false},"lamp":"green","stop":false,"divert":{"plastico":false,"aluminio":false,"vidrio":false}}'
```

## Librería Arduino

Library Manager → **LiquidCrystal I2C** (Frank de Brabander).  
Si `lcd.init()` falla, probá `lcd.begin()` según la lib, o addr `0x3F`.
