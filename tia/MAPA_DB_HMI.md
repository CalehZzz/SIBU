# DB_HMI — HMI web (sim · 3 materiales · **sin** FC pistones)

**DB3** · Optimized **OFF** · ≥ **6 bytes**

| Offset | Nombre | Tipo | Uso |
|---|---|---|---|
| 0.0–0.5 | `Start` `Stop` `Emergencia` `ResetAlarma` `ModoAuto` `FinSesion` | Bool | Operador |
| 0.6 | `ManualBanda` | Bool | Banda manual |
| 0.7 | `ManualPiston` | Bool | Manual **P3 vidrio** |
| 1.0 | `BasculaLista` | Bool | Sim báscula |
| 1.1 | `SensorPieza` | Bool | Sim pieza |
| 1.2 | `SensorPlastico` | Bool | Sim plástico → P1 |
| 1.3 | `SensorAluminio` | Bool | Sim latas → P2 |
| **1.4** | `ManualPiston1` | Bool | Manual **P1 plástico** |
| **1.5** | `ManualPiston2` | Bool | Manual **P2 latas** |
| **1.6** | `SensorVidrio` | Bool | Sim vidrio → P3 |
| 1.7 | *(libre)* | — | reservado |
| 2.0 | `PesoActualKg` | Real | Peso kg |

Sin `PistonNExtendido` / `PistonNRetractado`: el ciclo AUTO usa TON de empuje.
