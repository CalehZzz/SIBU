# RFID · Arduino → Raspberry (USB Serial)

## Arquitectura

```
Tarjeta → RC522 → Arduino Uno/Nano --USB Serial--> Pi rfid_gate.py
                                                    → Firestore rfid_gate/{authUid}
```

El **ESP32 ya no hace RFID**. El ESP32 hace botes + báscula + LCD  
(`costa_rica/arduino_bins/`).

## Cableado RC522 → Arduino Uno

| RC522 | Uno |
|---|---|
| SDA/SS | **D10** |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| RST | **D9** |
| 3.3V | **3.3V** (no 5V) |
| GND | GND |

## Flash

1. Arduino IDE → placa Uno/Nano  
2. Librería **MFRC522**  
3. Subí `rfid_serial.ino`  
4. USB a la Pi  

## Pi

```bash
# Detectar puerto
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null

# en costa_rica/sibu.env
RFID_SERIAL=/dev/ttyUSB0
# o /dev/ttyACM0
RFID_BAUD=115200

cd ~/SIBU && git pull
sudo usermod -aG dialout sibucr   # una vez; luego re-login
bash costa_rica/install_services.sh rfid
journalctl -u sibu-rfid-gate -f
```

Al acercar la tarjeta deberías ver `ALLOW` / `LINK` / `DENY` en el journal.

Serial Monitor (PC) también muestra `{"uid":"A7240B9F"}` a 115200.
