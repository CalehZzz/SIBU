# SIBU — PLC real (S7-1200 CPU 1215C DC/DC/DC · 3 pistones)

**CPU de mesa:** `6ES7 215-1HG40-0XB0` · **1215C DC/DC/DC** · FW **V4.5** → [`CPU.md`](CPU.md)

Proyecto **aparte** del de simulación (1511C + PLCSIM).  
No reutilices el hardware config del 1511C: crea un proyecto TIA nuevo con el **MLFB exacto**.

**Actuadores neumáticos (real):** 3 cilindros **doble efecto** — P1 plástico · P2 latas · P3 vidrio · **6 solenoides** (Ext/Ret) · **sin** FC de posición.  
**Semáforo:** `%Q` 24 V (transistor) → **relés externos** → lámparas 220 V.  
**Operador de mesa:** HMI web (plus). En sim: AS + KEP. Ver [`docs/13_CONECTAR_TODO.md`](../docs/13_CONECTAR_TODO.md).

```
Página SIBU / HMI web  (Start, Stop, manual, emergencia…)
        ↕ Firestore  (estación: colegio-don-bosco-real)
   Raspberry Pi 4
        ├─ plc_bridge_real.py  (HMI ↔ PLC ↔ web)
        └─ costa_rica/vision_gemini.py  (cámara → Gemini → VisionMaterial)
        ↕ snap7  (IP del 1215C)
   CPU 1215C + 3 sensores POSICIÓN + banda + 6 solenoides + relés externos
   Latch: VisionMaterial==N AND I_Sensor vía N → paro banda + pistón
```

Visión / Costa Rica: carpeta [`costa_rica/`](../costa_rica/).

**Guía desde cero:** [`docs/14_GUIA_TIA_PLC_DESDE_CERO.md`](../docs/14_GUIA_TIA_PLC_DESDE_CERO.md)

| Modo | Carpeta | CPU | Qué hay en mesa |
|---|---|---|---|
| Demo / sim | `tia/` + HMI 🖥️ | 1511C PLCSIM | 3 pistones sim (sin FC) |
| **PLC real** | **`plc_real/`** | **1215C** (`215-1HG40`) | Sensores + **6 Q pistones** + relés; mando solo web |

---

## Archivos

| Archivo | Contenido |
|---|---|
| `CPU.md` | MLFB / FW / notas DC/DC/DC |
| `00_PROYECTO_TIA.md` | Crear proyecto desde cero en TIA V20 |
| `TABLA_IO_1214C.md` | Asignación `%I` / `%Q` / `%M` / DBs (nombre histórico) |
| `NETWORKS_LAD.md` | Networks FC (bloques) |
| `DB_CONTRATO_WEB.md` | `DatosEstacion` + `DB_HMI` (mismo contrato web) |
| `plc_bridge_real.py` | Bridge con defaults para PLC real |
| `CHECKLIST.md` | PUT/GET, red, prueba |

---

## Bridge (desde esta carpeta o la raíz)

```powershell
py plc_real/plc_bridge_real.py
```

Sustituye la IP por la del 1215C (`PLC_IP`).

---

## Plan B — bridge en tu PC (sin Pi)

Misma web (Firestore) · bridge en la laptop · Ethernet al 1215C.

1. **Pará el bridge de la Pi** (si no, pelean por el PLC / la estación):
   ```bash
   sudo systemctl stop sibu-bridge
   ```
2. Cable Ethernet PC ↔ switch/PLC. IP del PC en la misma subnet que el PLC  
   (ej. PLC `192.168.0.1` → PC `192.168.0.100` / mask `255.255.255.0`).
3. En TIA: PUT/GET ON · DBs Optimized OFF · CPU **RUN** · online OK.
4. En la PC (PowerShell), desde la raíz del repo:
   ```powershell
   git pull
   pip install firebase-admin python-snap7
   # serviceAccountKey.json debe estar en la raíz del repo
   py plc_real/plc_bridge_real.py --ip 192.168.0.1 --pulse-hold 2.0
   ```
   Usá la IP real del 1215C (la de TIA / etiqueta).
5. Log bueno:
   - `✅ PLC conectado`
   - `👂 Escuchando hmi_comandos…`
   - Al START en la web: `HMI←FS(snap) pulse Start=1` → `On=1`
6. Web: **🔌 → Abrir HMI (modo real)** · estación `colegio-don-bosco-real` · **START**.

Si `ping` al PLC falla, el bridge no arranca. Si ping OK pero snap7 falla → PUT/GET / firewall / IP distinta.
