# Costa Rica — Visión Gemini + Raspberry Pi 4 + PLC

Flujo de campo para la evaluación (todo desde cero con el material que den).

```
Cámara → Raspberry Pi 4
           ├─ I_SensorPieza (flanco) → captura foto
           ├─ Gemini API → plastico | aluminio | vidrio | desconocido
           ├─ snap7 escribe DB_HMI.VisionMaterial @ 6.0  (1/2/3)
           └─ opc. Firestore (espejo “último visto”)
                │
PLC 1214C ← espera VisionMaterial==N AND I_Sensor de esa vía
           → para banda → pistón N → limpia VisionMaterial
                │
Bridge HMI (también en la Pi) ↔ Firestore ↔ web Guacamayos
```

## Docs

| Archivo | Qué |
|---|---|
| `QUE_NECESITO_GEMINI.md` | Lista exacta para integrar la API |
| `vision_gemini.py` | Servicio Pi 4 (stub listo para API key) |
| `../plc_real/NETWORKS_LAD.md` | Ladder: visión + sensor posición |
| `../plc_real/DB_CONTRATO_WEB.md` | `VisionMaterial` Int @ 6.0 |
| `../plc_real/CHECKLIST.md` | Checklist 2 h |

## Prueba desde la web (sin PLC)

En la app SIBU: nav **📷** → pegá `GEMINI_API_KEY` → cámara o foto → **Identificar**.

Eso llama Gemini desde el navegador (key solo en `localStorage`). **No necesita PLC ni bridge.**  
En estación real sigue siendo la Pi la que escribe `VisionMaterial` al PLC.

## Arranque en la Pi 4

```bash
# Terminal 1 — HMI ↔ PLC ↔ web
python plc_real/plc_bridge_real.py --ip 192.168.0.10

# Terminal 2 — cámara → Gemini → VisionMaterial
export GEMINI_API_KEY="tu_key"
python costa_rica/vision_gemini.py --ip 192.168.0.10
```

Misma subnet PLC ↔ Pi. Internet solo lo necesita Gemini.
