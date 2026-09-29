# RFID · Arduino → Raspberry (USB Serial)

## Arquitectura

```
Tarjeta → RC522 → Arduino Uno/Nano --USB Serial--> Pi rfid_gate.py
```

## Cableado RC522 → Arduino Uno / Nano

| RC522 (módulo) | Arduino | Nota |
|---|---|---|
| **SDA** o **SS** o **NSS** | **D10** | Chip select (no es I2C) |
| **SCK** | D13 | |
| **MOSI** | D11 | |
| **MISO** | D12 | |
| **RST** o **RESET** | **D9** | |
| **3.3V** / **VCC** | **3.3V** | **Nunca 5V** en el RC522 |
| **GND** | **GND** | Obligatorio |

Cables **cortos** y firmes. El pin 3.3V del Uno es débil: si `version=0x00`, alimentá el RC522 con un regulador 3.3V externo (GND común).

## Diagnóstico en Arduino IDE (hacé esto primero)

1. Placa: **Arduino Uno** (o Nano)  
2. Librería: **MFRC522** (Miguel Balboa)  
3. Subí `rfid_serial.ino`  
4. Serial Monitor → **115200 baud**  

### Qué debe salir al abrir el Monitor

| Mensaje | Significado |
|---|---|
| `"version":"0x91"` o `0x92` (o `0x88`) | Lector **OK** → acercá la tarjeta |
| `"version":"0x0"` / `0x00` / `0xFF` | **No hay SPI** → cableado o 3.3V |
| `{"uid":"A7240B9F"}` | Tap leído bien |
| `esperando tarjeta` cada 2 s | Lector vivo, esperando tap |

Si no sale **nada** en el Monitor: mal puerto COM / baud / placa incorrecta.

### Si version OK pero no lee la tarjeta

- Acercá la tarjeta a **1–2 cm** del antena (círculo del PCB)  
- Probá otra tarjeta/llavero MIFARE  
- Revisá que no esté el RC522 al revés o con cables cruzados MOSI/MISO  
- Algunos clones fallan con USB hub flojo: probá otro cable USB  

### Si version = 0x00 / 0xFF

1. ¿VCC del RC522 en **3.3V**? (si pusiste 5V puede haberse dañado)  
2. ¿GND común Arduino↔RC522?  
3. ¿SDA del módulo a **D10** (no a A4)?  
4. ¿SCK→13 MOSI→11 MISO→12 RST→9?  
5. Sacá foto del cableado y compará pin a pin  

## Pi (solo cuando el IDE ya muestra `uid`)

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
# en costa_rica/sibu.env:
RFID_SERIAL=/dev/ttyACM0   # o ttyUSB0
RFID_BAUD=115200

sudo usermod -aG dialout sibucr
# cerrar sesión y volver a entrar

cd ~/SIBU && git pull origin cursor/vision-test-page-c9e3
bash costa_rica/install_services.sh rfid
journalctl -u sibu-rfid-gate -f
```

Si el IDE no lee, la Pi **tampoco** va a leer: arreglá el RC522 primero en el PC.
