# RFID · Arduino → Pi (legado)

> **Preferido ahora:** RC522 en el **mismo ESP32** que botes/LCD/HX711.  
> Ver [`../arduino_bins/`](../arduino_bins/) — POST WiFi a `:8081`, sin USB Serial.

Este camino (Arduino USB → `RFID_SERIAL`) queda por si no tenés el RC522 en el ESP.

## Arquitectura (legado)

```
Tarjeta → RC522 → Arduino Uno/Nano --USB Serial--> Pi rfid_gate.py
```

## Cableado RC522 → Arduino Uno / Nano

| RC522 | Arduino | Nota |
|---|---|---|
| SDA/SS | D10 | |
| SCK | D13 | |
| MOSI | D11 | |
| MISO | D12 | |
| RST | D9 | |
| 3.3V | 3.3V | **Nunca 5V** |
| GND | GND | |

## Pi (solo si usás Arduino)

```bash
RFID_SERIAL=/dev/ttyACM0
bash costa_rica/install_services.sh rfid
```

Con el ESP unificado **no** hace falta `RFID_SERIAL` (el gate escucha HTTP).
