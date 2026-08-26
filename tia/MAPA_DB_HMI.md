# DB_HMI — HMI web (sim · 3 materiales · doble efecto)

**DB3** · Optimized **OFF** · ≥ **7 bytes**

| Offset | Nombre | Tipo | Uso |
|---|---|---|---|
| 0.0–0.5 | `Start` `Stop` `Emergencia` `ResetAlarma` `ModoAuto` `FinSesion` | Bool | Operador |
| 0.6 | `ManualBanda` | Bool | Banda manual |
| 0.7 | `ManualPiston` | Bool | Manual **P3 vidrio** |
| 1.0 | `BasculaLista` | Bool | Sim báscula |
| 1.1 | `SensorPieza` | Bool | Sim pieza |
| 1.2 | `SensorPlastico` | Bool | Sim plástico → P1 |
| 1.3 | `SensorAluminio` | Bool | Sim latas → P2 |
| 1.4 | `Piston1Extendido` | Bool | FC P1 **100 %** |
| 1.5 | `Piston2Extendido` | Bool | FC P2 **100 %** |
| 1.6 | `ManualPiston1` | Bool | Manual **P1 plástico** |
| 1.7 | `ManualPiston2` | Bool | Manual **P2 latas** |
| 2.0 | `PesoActualKg` | Real | Peso kg |
| 6.0 | `Piston3Extendido` | Bool | FC P3 **100 %** |
| 6.1 | `SensorVidrio` | Bool | Sim vidrio → P3 |
| 6.2 | `Piston1Retractado` | Bool | FC P1 **0 %** |
| 6.3 | `Piston2Retractado` | Bool | FC P2 **0 %** |
| 6.4 | `Piston3Retractado` | Bool | FC P3 **0 %** |

Doble efecto: casa = `*Retractado` · fuera = `*Extendido`. Nunca ambos a 1.
