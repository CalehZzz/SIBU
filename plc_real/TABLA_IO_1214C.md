# I/O — CPU 1214C (3 pistones · plástico / latas / vidrio)

Operador **100 % web** (`DB_HMI`). En mesa: sensores + banda + 3 cilindros **doble efecto** + válvulas **5/2** (solenoide + retorno por muelle).

| Pistón | Salida | Material | Válvula |
|---|---|---|---|
| **P1** | `Q_Piston1` | Plástico | 5/2 |
| **P2** | `Q_Piston2` | Latas (aluminio) | 5/2 |
| **P3** | `Q_Piston3` | Vidrio | 5/2 |

```
  [Entrada] → [Báscula] → [Banda] → sensores
                                      ├─ plástico → P1
                                      ├─ latas    → P2
                                      └─ vidrio   → P3
```

**Doble efecto (monoestable):** `Q_PistonN = 1` → aire extiende · `Q = 0` → aire retracta (la 5/2 vuelve por muelle).  
Cada cilindro lleva **2** finales de carrera: **0%** (retractado) y **100%** (extendido).

## Entradas `%I` (11)

| Dir | Tag | Hardware |
|---|---|---|
| `%I0.0` | `I_SensorPieza` | Pieza presente |
| `%I0.1` | `I_SensorPlastico` | Plástico |
| `%I0.2` | `I_SensorAluminio` | Latas |
| `%I0.3` | `I_SensorVidrio` | Vidrio |
| `%I0.4` | `I_BasculaLista` | Báscula lista |
| `%I0.5` | `I_Piston1Extendido` | P1 @ 100% |
| `%I0.6` | `I_Piston2Extendido` | P2 @ 100% |
| `%I0.7` | `I_Piston3Extendido` | P3 @ 100% |
| `%I1.0` | `I_Piston1Retractado` | P1 @ 0% |
| `%I1.1` | `I_Piston2Retractado` | P2 @ 0% |
| `%I1.2` | `I_Piston3Retractado` | P3 @ 0% |

## Salidas `%Q` (7)

| Dir | Tag | Hardware |
|---|---|---|
| `%Q0.0` | `Q_Banda` | Banda |
| `%Q0.1` | `Q_Piston1` | Solenoide 5/2 plástico |
| `%Q0.2` | `Q_Piston2` | Solenoide 5/2 latas |
| `%Q0.3` | `Q_Piston3` | Solenoide 5/2 vidrio |
| `%Q0.4` | `Q_LamparaRun` | Verde |
| `%Q0.5` | `Q_LamparaAlarma` | Rojo |
| `%Q0.6` | `Q_LamparaEmergencia` | Amarillo |

## Memorias

`M_SistemaOn` · `M_ModoAuto` · `M_Alarma` · `M_ClasifPlastico` · `M_ClasifAluminio` · `M_ClasifVidrio` · `M_Clasificando`

## DBs

`DatosEstacion` DB1 · `DB_HMI` DB3 · Optimized **OFF** (ver `DB_CONTRATO_WEB.md`)
