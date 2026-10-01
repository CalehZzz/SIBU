# Costa Rica — Visión Gemini + Raspberry Pi 4

## RFID + botes (Costa Rica)

```
ESP32 (unificado)
  ├─ HC + LCD + HX711 ──WiFi──→ Pi :8082 /api/bins
  └─ RC522 RFID       ──WiFi──→ Pi :8081 /api/rfid
```

**Probar / pines:** [`arduino_bins/README.md`](arduino_bins/README.md) · checklist [`PROBAR_ESP_ARDUINO.md`](PROBAR_ESP_ARDUINO.md)

Arduino USB RFID = **legado** (`arduino_rfid/`). No hace falta si el RC522 está en el ESP.

```bash
# sibu.env: BINS_PORT=8082  RFID_PORT=8081
# (RFID_SERIAL vacío si usás ESP WiFi)
bash costa_rica/install_services.sh web
```


## Página SIBU 📷 (recomendado — cualquier red)

La web habla con la Pi **por Firestore** (no hace falta `http://IP:8080`).

```bash
cd ~/SIBU
git pull origin cursor/vision-test-page-c9e3
cp -n costa_rica/sibu.env.example costa_rica/sibu.env
nano costa_rica/sibu.env   # GEMINI_API_KEY + PLC_IP

# serviceAccountKey.json debe estar en ~/SIBU/
bash costa_rica/install_services.sh web
```

Luego abrí la **app SIBU normal** (Firebase Hosting) → login → **📷** → Actualizar / Identificar.

### Reglas Firestore (si falla permiso)

En Firebase Console → Firestore → Rules, agregá algo como:

```
match /vision_pi/{doc} {
  allow read: if request.auth != null;
  allow write: if request.auth != null;
}
```

(La Pi usa Admin SDK y no depende de esas rules.)

### Cuota
- Listener `on_snapshot` + comandos solo al tocar botones (bajo uso).
- Heartbeat online 1 write/min.
- **No** actives `sibu-bridge` hasta que TIA tenga PUT/GET + DBs.

## Autostart

| Comando | Qué |
|---|---|
| `bash costa_rica/install_services.sh web` | visión Firestore + RFID + **bins** |
| `bash costa_rica/install_services.sh bins` | solo niveles HC-SR04 :8082 |
| `bash costa_rica/install_services.sh http` | opcional :8080 LAN |
| `bash costa_rica/install_services.sh all` | web + bridge + vision→PLC |
| `bash costa_rica/install_services.sh stop` | apaga todo |

```bash
systemctl status sibu-vision-firebase
journalctl -u sibu-vision-firebase -f
```

## PLC (TIA desde cero · 1215C `6ES7 215-1HG40-0XB0` V4.5)

Ver `plc_real/00_PROYECTO_TIA.md` · `NETWORKS_LAD.md` · `CHECKLIST.md`.

**Lógica actual:** entrada → foto Gemini **con banda en marcha** (no espera peso) → Vision + sensor de vía → pistón.  
Báscula solo al **final** (desconocidos / demo), poca importancia.

IP tipica PLC `192.168.0.1` · Pi eth0 `192.168.0.20`.
