# SIBU — PLC real (S7-1200 CPU 1214C AC/DC/Rly · 3 pistones)

Proyecto **aparte** del de simulación (1511C + PLCSIM).  
No reutilices el hardware config del 1511C: crea un proyecto TIA nuevo con CPU **1214C**.

**Actuadores neumáticos (real):** 3 cilindros **doble efecto** — P1 plástico · P2 latas · P3 vidrio · **6 solenoides** (Ext/Ret) · **sin** FC de posición.  
**Semáforo:** 3 relés 24 V (`Q_Lampara*`) → lámparas 220 V. Comunes **1L y 2L → 24 V**.  
**Operador de mesa:** HMI web (plus). En sim: AS + KEP. Ver [`docs/13_CONECTAR_TODO.md`](../docs/13_CONECTAR_TODO.md).

```
Página SIBU / HMI web  (Start, Stop, manual, emergencia…)
        ↕ Firestore  (estación: colegio-don-bosco-real)
   Raspberry Pi 4
        ├─ plc_bridge_real.py  (HMI ↔ PLC ↔ web)
        └─ costa_rica/vision_gemini.py  (cámara → Gemini → VisionMaterial)
        ↕ snap7  (IP del 1214C)
   CPU 1214C + 3 sensores POSICIÓN + banda + 6 solenoides + 3 relés
   Latch: VisionMaterial==N AND I_Sensor vía N → paro banda + pistón
```

Visión / Costa Rica: carpeta [`costa_rica/`](../costa_rica/).

**Guía desde cero:** [`docs/14_GUIA_TIA_PLC_DESDE_CERO.md`](../docs/14_GUIA_TIA_PLC_DESDE_CERO.md)

| Modo | Carpeta | CPU | Qué hay en mesa |
|---|---|---|---|
| Demo / sim | `tia/` + HMI 🖥️ | 1511C PLCSIM | 3 pistones sim (sin FC) |
| **PLC real** | **`plc_real/`** | **1214C** | Sensores material + **6 Q pistones** + relés; mando solo web |

---

## Archivos

| Archivo | Contenido |
|---|---|
| `00_PROYECTO_TIA.md` | Crear proyecto desde cero en TIA V20 |
| `TABLA_IO_1214C.md` | Asignación `%I` / `%Q` / `%M` / DBs |
| `NETWORKS_LAD.md` | Networks FC (bloques) |
| `DB_CONTRATO_WEB.md` | `DatosEstacion` + `DB_HMI` (mismo contrato web) |
| `plc_bridge_real.py` | Bridge con defaults para PLC real |
| `CHECKLIST.md` | PUT/GET, red, prueba |

---

## Bridge (desde esta carpeta o la raíz)

```powershell
py plc_real/plc_bridge_real.py
```

o:

```powershell
py plc_bridge.py colegio-don-bosco-real --ip 192.168.0.10 --db 1 --db-hmi 3
```

Sustituye `192.168.0.10` por la IP del 1214C.
