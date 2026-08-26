# Contrato web — 3 pistones (plástico / latas / vidrio)

Optimized **OFF** en ambos DB. Operador 100 % web → `DB_HMI`.

Actuadores reales: **6 solenoides** (`Q_PistonNExt` / `Q_PistonNRet`) + banda + 3 relés semáforo.  
El espejo web sigue usando `PistonNOn` = comando `M_PistonN` (Extender / Retractar en HMI).

---

## `DatosEstacion` — DB **1** · **28 bytes**

| Offset | Nombre | Tipo | Notas |
|---|---|---|---|
| 0.0 | `ContPlastico` | Int | |
| 2.0 | `ContAluminio` | Int | Latas |
| 4.0 | `PesoPlasticoKg` | Real | |
| 8.0 | `PesoAluminioKg` | Real | Latas |
| 12.0 | `PesoActualKg` | Real | |
| 16.0–16.7 | `SesionActiva`…`PistonOn` | Bool | `PistonOn` = OR P1/P2/P3 |
| 17.0 | `Piston1On` | Bool | Plástico (`M_Piston1`) |
| 17.1 | `Piston2On` | Bool | Latas (`M_Piston2`) |
| 17.2 | `Piston3On` | Bool | Vidrio (`M_Piston3`) |
| 18.0 | `EstadoMaquina` | Int | 0…4 |
| 20.0 | `UltimoMaterial` | Int | 0 ninguno · 1 plástico · 2 aluminio · **3 vidrio** |
| 22.0 | `ContVidrio` | Int | |
| 24.0 | `PesoVidrioKg` | Real | |

```scl
DatosEstacion.Piston1On := M_Piston1; // real: comando; Q_Piston1Ext/Ret se derivan
DatosEstacion.Piston2On := M_Piston2;
DatosEstacion.Piston3On := M_Piston3;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
```

---

## `DB_HMI` — DB **3** · **7 bytes**

| Offset | Nombre | Uso |
|---|---|---|
| 0.0–0.7 | Start…ManualPiston | ManualPiston = **P3 vidrio** |
| 1.0–1.3 | BasculaLista · SensorPieza · SensorPlastico · SensorAluminio | Sim / real ignora → `I_*` |
| 1.4–1.5 | Piston1Extendido · Piston2Extendido | Sim FC **100 %** |
| 1.6–1.7 | ManualPiston1 · ManualPiston2 | P1 plástico · P2 latas |
| 2.0 | PesoActualKg | Real |
| 6.0 | Piston3Extendido | Sim FC P3 **100 %** |
| 6.1 | SensorVidrio | Sim vidrio · real → `I_SensorVidrio` |
| 6.2 | Piston1Retractado | Sim FC P1 **0 %** |
| 6.3 | Piston2Retractado | Sim FC P2 **0 %** |
| 6.4 | Piston3Retractado | Sim FC P3 **0 %** |

> En reposo (casa): `PistonNRetractado = 1` · `PistonNExtendido = 0`.  
> Nunca ambos a 1 (alarma en LAD).
