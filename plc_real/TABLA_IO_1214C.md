# I/O — CPU 1214C (3 pistones doble efecto · 6 solenoides + relés semáforo)

Operador **100 % web** (`DB_HMI`). En mesa: sensores + banda + 3 cilindros **doble efecto** + válvulas **5/2 biestables** (2 solenoides c/u) + semáforo 220 V vía **relés 24 V**.

| Pistón | Extender | Retractar | Material |
|---|---|---|---|
| **P1** | `Q_Piston1Ext` | `Q_Piston1Ret` | Plástico |
| **P2** | `Q_Piston2Ext` | `Q_Piston2Ret` | Latas (aluminio) |
| **P3** | `Q_Piston3Ext` | `Q_Piston3Ret` | Vidrio |

```
  [Entrada] → [Báscula] → [Banda] → sensores
                                      ├─ plástico → P1 (Ext/Ret)
                                      ├─ latas    → P2 (Ext/Ret)
                                      └─ vidrio   → P3 (Ext/Ret)
```

**Doble efecto biestable:** `Q_…Ext = 1` y `Q_…Ret = 0` → aire extiende · al revés retracta.  
**Nunca** energizar Ext y Ret a la vez (interlock en LAD).  
Cada cilindro: **2** finales de carrera — **0 %** (`…Retractado`) y **100 %** (`…Extendido`).

---

## Comunes del PLC (1L / 2L) — ambos a **24 V**

La CPU 1214C AC/DC/Rly agrupa salidas en dos comunes:

| Común | Salidas |
|---|---|
| **1L** | `%Q0.0` … `%Q0.4` |
| **2L** | `%Q0.5` `%Q0.6` `%Q0.7` `%Q1.0` `%Q1.1` |

**Conexión:** `1L` y `2L` → **mismo +24 V** (o el polo que use tu cableado de bobinas).  
Banda + 6 solenoides + **bobinas de los 3 relés del semáforo** viven a 24 V en el PLC.

El semáforo (lámparas **220 V**) **no** se cablea al común del PLC: cada `Q_Lampara*` alimenta la **bobina 24 V** de un relé; el contacto NA del relé conmuta la fase 220 V hacia la lámpara (neutro común a las tres).

```
  PLC Q_LamparaRun ──► bobina relé K1 (24 V)
                         contacto NA K1 ── L 220 V ──► lámpara verde ── N

  (igual K2 rojo / K3 amarillo)
```

Protege el circuito 220 V con fusible/breaker aparte.

---

## Entradas `%I` (11)

| Dir | Tag | Hardware |
|---|---|---|
| `%I0.0` | `I_SensorPieza` | Pieza presente |
| `%I0.1` | `I_SensorPlastico` | Plástico |
| `%I0.2` | `I_SensorAluminio` | Latas |
| `%I0.3` | `I_SensorVidrio` | Vidrio |
| `%I0.4` | `I_BasculaLista` | Báscula lista |
| `%I0.5` | `I_Piston1Extendido` | P1 @ 100 % |
| `%I0.6` | `I_Piston2Extendido` | P2 @ 100 % |
| `%I0.7` | `I_Piston3Extendido` | P3 @ 100 % |
| `%I1.0` | `I_Piston1Retractado` | P1 @ 0 % |
| `%I1.1` | `I_Piston2Retractado` | P2 @ 0 % |
| `%I1.2` | `I_Piston3Retractado` | P3 @ 0 % |

---

## Salidas `%Q` (10)

| Dir | Tag | Común | Hardware (24 V en el PLC) |
|---|---|---|---|
| `%Q0.0` | `Q_Banda` | 1L | Contactor / relé banda |
| `%Q0.1` | `Q_Piston1Ext` | 1L | Solenoide A — P1 plástico |
| `%Q0.2` | `Q_Piston1Ret` | 1L | Solenoide B — P1 plástico |
| `%Q0.3` | `Q_Piston2Ext` | 1L | Solenoide A — P2 latas |
| `%Q0.4` | `Q_Piston2Ret` | 1L | Solenoide B — P2 latas |
| `%Q0.5` | `Q_Piston3Ext` | 2L | Solenoide A — P3 vidrio |
| `%Q0.6` | `Q_Piston3Ret` | 2L | Solenoide B — P3 vidrio |
| `%Q0.7` | `Q_LamparaRun` | 2L | Bobina relé → lámpara verde 220 V |
| `%Q1.0` | `Q_LamparaAlarma` | 2L | Bobina relé → lámpara roja 220 V |
| `%Q1.1` | `Q_LamparaEmergencia` | 2L | Bobina relé → lámpara amarilla 220 V |

**Conteo:** 6 solenoides + 1 banda + 3 relés semáforo = **10** salidas (llena el 1214C).

---

## Memorias

`M_SistemaOn` · `M_ModoAuto` · `M_Alarma` · `M_ClasifPlastico` · `M_ClasifAluminio` · `M_ClasifVidrio` · `M_Clasificando`  
`M_Piston1` / `M_Piston2` / `M_Piston3` = **comando** “quiero extendido” (AUTO o MANUAL); las `Q_…Ext` / `Q_…Ret` se derivan en LAD.

---

## DBs

`DatosEstacion` DB1 · `DB_HMI` DB3 · Optimized **OFF** (ver `DB_CONTRATO_WEB.md`)
