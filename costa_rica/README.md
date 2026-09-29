# Costa Rica — Visión Gemini + Raspberry Pi 4

## RFID (HMI / cámara)

Ver `costa_rica/esp32_rfid/README.md`.

```bash
# en sibu.env: RFID_ALLOW=UIDHEX:Nombre
bash costa_rica/install_services.sh web   # incluye sibu-rfid-gate
curl -X POST http://127.0.0.1:8081/api/rfid -H 'Content-Type: application/json' -d '{"uid":"TEST"}'
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
| `bash costa_rica/install_services.sh web` | visión Firestore (página) |
| `bash costa_rica/install_services.sh http` | opcional :8080 LAN |
| `bash costa_rica/install_services.sh all` | web + bridge + vision→PLC |
| `bash costa_rica/install_services.sh stop` | apaga todo |

```bash
systemctl status sibu-vision-firebase
journalctl -u sibu-vision-firebase -f
```

## PLC (mañana con TIA)

Ver `plc_real/NETWORKS_LAD.md` · IP tipica PLC `192.168.0.1` · Pi eth0 `192.168.0.20`.
