# Costa Rica — Visión Gemini + Raspberry Pi 4 + PLC

## Prueba desde la web (cámara de la Pi, **sin PLC**)

En la Pi (API key ya exportada):

```bash
cd ~/SIBU
source .venv/bin/activate
export GEMINI_API_KEY="TU_KEY"
# CSI:
python costa_rica/vision_http.py --port 8080 --capture-cmd 'libcamera-still -n -t 1 -o {path}'
# USB / OpenCV:
# python costa_rica/vision_http.py --port 8080
```

En el celular/PC (misma red que la Pi):

1. Abrí `http://IP_DE_LA_PI:8080/`
2. Nav **📷**
3. URL = `http://IP_DE_LA_PI:8080` → Probar conexión
4. **Actualizar vista** / **Identificar con Gemini (Pi)**

No hace falta PLC ni `plc_bridge` para esta prueba.

---

## Estación real (con PLC)

```
Cámara → Raspberry Pi 4
           ├─ vision_http.py     (pruebas web, sin PLC)
           ├─ vision_gemini.py   (I_SensorPieza → Gemini → VisionMaterial)
           └─ plc_bridge_real.py (HMI ↔ PLC ↔ Firestore)
```

| Archivo | Qué |
|---|---|
| `QUE_NECESITO_GEMINI.md` | Lista para la API |
| `vision_http.py` | HTTP snapshot + classify (web 📷) |
| `vision_gemini.py` | Loop estación → escribe PLC |
| `../plc_real/NETWORKS_LAD.md` | Ladder visión + sensor |

### Arranque estación (2–3 procesos)

```bash
# Terminal 1 — HMI ↔ PLC ↔ web
python plc_real/plc_bridge_real.py --ip 192.168.0.10

# Terminal 2 — cámara → Gemini → VisionMaterial
export GEMINI_API_KEY="tu_key"
python costa_rica/vision_gemini.py --ip 192.168.0.10

# Opcional — panel 📷 en el navegador
python costa_rica/vision_http.py --port 8080 --capture-cmd 'libcamera-still -n -t 1 -o {path}'
```
