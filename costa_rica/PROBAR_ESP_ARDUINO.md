# Probar ahora — Arduino RFID + ESP32 bins

Sin PLC. Solo periféricos → Pi → web.

```
Arduino RC522 ──USB──→ Pi rfid_gate → Firestore → candado web
ESP32 HC/LCD/HX711 ─WiFi─→ Pi :8082 → bins_pi/estado → panel botes
```

---

## 0) Pi — preparar una vez

```bash
cd ~/SIBU
git pull origin cursor/vision-test-page-c9e3
cp -n costa_rica/sibu.env.example costa_rica/sibu.env
# serviceAccountKey.json debe estar en ~/SIBU/
```

---

## 1) Arduino RFID — primero en el PC (IDE)

1. Cableá RC522: **3.3V** · GND · SS→D10 · SCK→13 · MOSI→11 · MISO→12 · RST→D9  
2. Arduino IDE → placa Uno/Nano → librería **MFRC522**  
3. Subí `costa_rica/arduino_rfid/rfid_serial.ino`  
4. Serial Monitor **115200**

| Salida | Acción |
|---|---|
| `version=0x91` / `0x92` / `0x88` | OK → acercá tarjeta |
| `{"uid":"A7240B9F"}` | Listo para la Pi |
| `version=0x0` / `0xFF` | Cableado / 3.3V — no pases a la Pi |

**Cerrá el Serial Monitor** antes de enchufar a la Pi (solo un programa usa el puerto).

### En la Pi

```bash
# 1. Enchufá el Arduino por USB
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null

# 2. Poné el puerto en sibu.env (casi siempre ACM0 en Uno)
nano costa_rica/sibu.env
# RFID_SERIAL=/dev/ttyACM0
# RFID_BAUD=115200

sudo usermod -aG dialout "$USER"   # una vez; luego logout/login

# 3. Solo RFID
bash costa_rica/install_services.sh rfid

# 4. Logs en vivo
journalctl -u sibu-rfid-gate -f
```

Acercá la tarjeta. En el log debe verse el UID / unlock.  
En la web (login con tu Google): candado RFID / vincular si es la primera vez.

**Si el log no muestra SER / uid:**
```bash
# ¿Otro proceso come el puerto?
sudo fuser -v /dev/ttyACM0
# Probar crudo (pará el servicio antes):
sudo systemctl stop sibu-rfid-gate
cat /dev/ttyACM0   # 115200; deberías ver version= y uid=
# Ctrl+C → volvé a arrancar
sudo systemctl start sibu-rfid-gate
```

---

## 2) ESP32 bins — flash y WiFi

1. Arduino IDE → **ESP32 Dev Module**  
2. Librerías: **LiquidCrystal I2C** + **HX711** (bogde)  
3. Abrí `costa_rica/arduino_bins/bins_nivel.ino` y editá:

```cpp
const char* WIFI_SSID = "TU_HOTSPOT";   // mismo WiFi que la Pi
const char* WIFI_PASS = "TU_PASSWORD";
const char* PI_HOST   = "192.168.0.20"; // IP de la Pi (ip a / hostname -I)
const int   PI_PORT   = 8082;
```

4. Cableá (mínimo para probar):
   - HC plástico: TRIG **13** · ECHO **12** · VCC 5V · GND  
   - HC rechazo: TRIG **33** · ECHO **32**  
   - LCD (opcional): SDA **21** · SCL **22** · addr `0x27`  
   - HX711 (opcional): DT **26** · SCK **25**  

5. Flash · Serial Monitor **115200** → debe decir WiFi connected y POSTs.

### En la Pi

```bash
# IP de la Pi (la que pusiste en PI_HOST)
hostname -I

bash costa_rica/install_services.sh bins
# o las dos cosas juntas:
# bash costa_rica/install_services.sh web

journalctl -u sibu-bins -f
```

### Probar sin ESP (curl desde la Pi)

```bash
curl -s http://127.0.0.1:8082/api/health
# → {"ok": true, "bins": true}

curl -s -X POST "http://127.0.0.1:8082/api/bins" \
  -H "Content-Type: application/json" \
  -d '{"plastico":{"cm":20,"pct":55,"lleno":false},"aluminio":{"cm":-1,"pct":0,"lleno":false},"vidrio":{"cm":-1,"pct":0,"lleno":false},"rechazo":{"cm":30,"pct":25,"lleno":false},"pesoKg":0.12,"pesoMaterial":"plastico","lamp":"green","stop":false,"divert":{"plastico":false,"aluminio":false,"vidrio":false},"demo":true}'
# → {"ok": true}
```

Si responde ok, el servicio Pi + Firestore están bien.  
En la web: panel de botes debe mostrar ~55% plástico / ~25% rechazo.


---

## 3) Las dos juntas (recomendado)

```bash
cd ~/SIBU
# RFID_SERIAL y BINS_PORT ya en sibu.env
bash costa_rica/install_services.sh web   # visión + rfid + bins

systemctl is-active sibu-rfid-gate sibu-bins
journalctl -u sibu-rfid-gate -u sibu-bins -f
```

Checklist rápido:

- [ ] IDE Arduino: `uid` al tap  
- [ ] `journalctl` RFID: misma línea al tap  
- [ ] Web: unlock / vincular  
- [ ] Serial ESP: WiFi OK + POST 200  
- [ ] `curl` bins OK  
- [ ] Web: % plástico / rechazo se mueven  

---

## Fallos típicos

| Qué ves | Causa |
|---|---|
| RFID IDE OK, Pi silencio | IDE aún abierto · mal `RFID_SERIAL` · no en grupo `dialout` |
| ESP WiFi fail | SSID/PASS · Pi en otra red · hotspot 2.4 GHz |
| ESP WiFi OK, bins no llegan | `PI_HOST` mal · firewall · `sibu-bins` down |
| Solo un HC “online” | ECHO/TRIG cruzados o sin GND común en el otro |
| LCD en blanco | addr `0x27` vs `0x3F` · SDA/SCL |
| HX711 0 o loco | calibrar `HX_SCALE` · cable DT/SCK |

Detalle cableado: `arduino_rfid/README.md` · `arduino_bins/README.md`
