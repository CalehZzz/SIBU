# Probar ahora — ESP32 unificado (bins + RFID)

Sin PLC. Sin Arduino (salvo legado).

```
ESP32 HC/LCD/HX711 ─WiFi─→ Pi :8082 → bins_pi/estado → web
ESP32 RC522         ─WiFi─→ Pi :8081 → rfid_gate → candado web
```

¿Se satura? **No** — ver nota en `arduino_bins/README.md`.

---

## 0) Pi

```bash
cd ~/SIBU
git pull origin cursor/vision-test-page-c9e3
cp -n costa_rica/sibu.env.example costa_rica/sibu.env
# serviceAccountKey.json en ~/SIBU/
# RFID_SERIAL vacío o comentado (RFID por HTTP del ESP)
nano costa_rica/sibu.env
bash costa_rica/install_services.sh web
journalctl -u sibu-rfid-gate -u sibu-bins -f
```

---

## 1) ESP32 — flash

1. IDE → ESP32 Dev Module · librerías: LiquidCrystal I2C · HX711 · **MFRC522**  
2. `costa_rica/arduino_bins/bins_nivel.ino` → WiFi + `PI_HOST`  
3. Cableá:

| Pieza | Pines |
|---|---|
| HC plástico | TRIG **18** ECHO **19** |
| HC rechazo | TRIG **33** ECHO **32** |
| RC522 | **SDA/SS → 5** · SCK **14** · MOSI **13** · MISO **23** · RST **27** · **3.3V** |
| LCD | SDA **21** SCL **22** |
| HX711 | DT **26** SCK **25** |

4. Serial 115200: `RC522 version=0x91 OK` · acercá tarjeta → `RFID uid=...` · `POST rfid → 200` · `POST bins → 200`

---

## 2) Pruebas rápidas en la Pi

```bash
curl -s http://127.0.0.1:8082/api/health
curl -s http://127.0.0.1:8081/api/health
curl -s -X POST http://127.0.0.1:8081/api/rfid \
  -H 'Content-Type: application/json' -d '{"uid":"A7240B9F"}'
```

Web: panel botes + candado RFID / vincular.

---

## Arduino RFID (solo si no usás RC522 en el ESP)

Ver `arduino_rfid/README.md` (legado USB Serial).
