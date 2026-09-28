# DB_HMI — HMI web + visión Gemini (3 materiales · **sin** FC pistones)

**DB3** · Optimized **OFF** · ≥ **8 bytes**

| Offset | Nombre | Tipo | Uso |
|---|---|---|---|
| 0.0–0.5 | `Start` `Stop` `Emergencia` `ResetAlarma` `ModoAuto` `FinSesion` | Bool | Operador |
| 0.6 | `ManualBanda` | Bool | Banda manual |
| 0.7 | `ManualPiston` | Bool | Manual **P3 vidrio** |
| 1.0 | `BasculaLista` | Bool | Sim / espejo báscula |
| 1.1 | `SensorPieza` | Bool | Sim / espejo pieza (trigger cámara) |
| 1.2 | `SensorPlastico` | Bool | Sim / espejo **posición** vía P1 |
| 1.3 | `SensorAluminio` | Bool | Sim / espejo **posición** vía P2 |
| **1.4** | `ManualPiston1` | Bool | Manual **P1 plástico** |
| **1.5** | `ManualPiston2` | Bool | Manual **P2 latas** |
| **1.6** | `SensorVidrio` | Bool | Sim / espejo **posición** vía P3 |
| 1.7 | *(libre)* | — | reservado |
| 2.0 | `PesoActualKg` | Real | Peso kg |
| **6.0** | **`VisionMaterial`** | **Int** | 0 ninguno · **1** plástico · **2** aluminio · **3** vidrio |

Sin `PistonNExtendido` / `PistonNRetractado`: el ciclo AUTO usa TON de empuje.

**Quién escribe `VisionMaterial`:** solo el servicio en la **Raspberry Pi 4** (Gemini).  
El bridge HMI escribe bytes 0–5; el PLC lo limpia a 0 al terminar clasificar.  
Latch real: `VisionMaterial == N` **AND** `I_Sensor…` → paro banda + pistón (ver `plc_real/NETWORKS_LAD.md`).
