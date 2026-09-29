# Contrato web — 3 pistones (plástico / latas / vidrio) · sin FC

Optimized **OFF** en ambos DB. Operador 100 % web → `DB_HMI`.

Actuadores reales: **6 solenoides** (`Q_PistonNExt` / `Q_PistonNRet`) + banda + 3 relés.  
Espejo: `PistonNOn` = comando `M_PistonN`. **Sin** bits de extendido/retractado en el DB.

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
DatosEstacion.Piston1On := M_Piston1;
DatosEstacion.Piston2On := M_Piston2;
DatosEstacion.Piston3On := M_Piston3;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
```

---

## `DB_HMI` — DB **3** · **8 bytes** (mínimo)

| Offset | Nombre | Uso |
|---|---|---|
| 0.0–0.7 | Start…ManualPiston | ManualPiston = **P3 vidrio** |
| 1.0 | BasculaLista | Espejo / sim → `I_BasculaFinal` (báscula al **final**, demo). **No** gatea clasificación |
| 1.1 | SensorPieza | Espejo / sim → `I_SensorPieza` (trigger cámara) |
| 1.2 | SensorPlastico | Espejo / sim vía P1 (posición) |
| 1.3 | SensorAluminio | Espejo / sim vía P2 (posición) |
| **1.4** | **ManualPiston1** | P1 plástico |
| **1.5** | **ManualPiston2** | P2 latas |
| **1.6** | **SensorVidrio** | Espejo / sim vía P3 (posición) |
| 1.7 | *(libre)* | reservado |
| 2.0 | PesoActualKg | Real |
| **6.0** | **`VisionMaterial`** | **Int** · **0** vacío/pendiente · **1** plástico · **2** aluminio · **3** vidrio · **4** desconocido (pass-through) |

> **Visión (Pi 4 + Gemini):** escribe `VisionMaterial` @ 6.0 (`4` si no reconoce).  
> **Bridge HMI:** escribe bytes 0–5 (no pisa el Int @ 6).  
> **PLC:** pone `VisionMaterial := 0` al terminar empuje, al dejar pasar desconocido, o por timeout.  
> **Peso:** opcional / demo (`PesoActualKg`). El ciclo **no** espera báscula lista.  
> Optimized **OFF**. Tamaño mínimo **8 bytes**.  
> **Eliminados:** `PistonNExtendido` / `PistonNRetractado` (no hay FC).
