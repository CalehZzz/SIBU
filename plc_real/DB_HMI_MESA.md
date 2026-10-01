# DB_HMI — programa TIA **mesa** (CPU 1215C)

Mapa real del proyecto en TIA (pantallazos). Distinto de `tia/MAPA_DB_HMI.md` (SIBU/sim).

Bridge: `py plc_real/plc_bridge_real.py --ip …` → **`--perfil mesa`** (default).

## Comandos (web → bridge → PLC)

| Tag TIA | Offset | Key Firestore / bridge |
|---|---|---|
| `Start` | 0.0 | `Start` |
| `Stop` | 0.1 | `Stop` |
| `Reset` | 0.2 | `ResetAlarma` |
| `Emergencia` | 0.3 | `Emergencia` |
| `ModoAuto` | 0.4 | `ModoAuto` |
| `ModoManual` | 0.5 | derivado: `NOT ModoAuto` |
| `IA_Plastico` … `IA_AnalisisListo` | 0.6–1.2 | (visión; no los pisa el bridge) |
| `Manual_Banda` | **1.3** | `ManualBanda` |
| `Manual_PistonPlastico` | 1.4 | `ManualPiston1` |
| `Manual_PistonVidrio` | 1.5 | `ManualPiston` (P3 web) |
| `Manual_PistonMetal` | 1.6 | `ManualPiston2` (P2 web) |

## Estado (PLC → bridge → web)

| Tag TIA | Offset | Campo web |
|---|---|---|
| `Estado_SistemaOn` | **1.7** | `plc.sistemaOn` |
| `Estado_AutoActivo` | 2.0 | `plc.modoAuto` |
| `Estado_ManualActivo` | 2.1 | (info) |
| `Estado_Standby` | 2.2 | |
| `Estado_Falla` | 2.3 | `plc.alarma` |
| `Estado_Emergencia` | 2.4 | `plc.emergencia` |
| `Estado_BandaActiva` | 2.5 | `plc.banda` |
| `Estado_PistonPlastico` | 2.6 | `plc.piston1` |
| `Estado_PistonVidrio` | 2.7 | `plc.piston3` |
| `Estado_PistonMetal` | 3.0 | `plc.piston2` |
| `Conteo_Plastico/Vidrio/Metal` | 4 / 6 / 8 | materiales.* |
| `Peso_*` | 14 / 18 / 22 | materiales.*.pesoKg |

## Reglas del bridge mesa

1. **RMW** solo bytes 0–1 de comandos — **no** escribe `Estado_SistemaOn@1.7` ni bytes ≥2.  
2. Si hay `ManualBanda` / pistón → fuerza `ModoManual=1` y `ModoAuto=0`.  
3. Banda Manual en LAD: `M_SistemaOn` · `M_ManualActivo` · `Manual_Banda` · `/M_Falla` · `/I_Emergencia`.

## Bug que había

El bridge SIBU escribía 6 bytes en cero desde offset 0 → **borraba** `Estado_SistemaOn` y el resto de estados cada ciclo, y además `ManualBanda` iba a **0.6** (en mesa eso es `IA_Plastico`, no la banda).
