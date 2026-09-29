# I/O — CPU 1214C (3 pistones doble efecto · **sin** FC de posición)

Operador **100 % web** (`DB_HMI`). En mesa: sensores de material + banda + 3 cilindros **doble efecto** + válvulas **5/2 biestables** (2 solenoides c/u) + semáforo 220 V vía **relés 24 V**.

| Pistón | Extender | Retractar | Material |
|---|---|---|---|
| **P1** | `Q_Piston1Ext` | `Q_Piston1Ret` | Plástico |
| **P2** | `Q_Piston2Ext` | `Q_Piston2Ret` | Latas (aluminio) |
| **P3** | `Q_Piston3Ext` | `Q_Piston3Ret` | Vidrio |

```
  [Entrada] → PARA (M_Pesando) → báscula + cámara Gemini (VisionMaterial 1/2/3/4)
                                      │
              Vision<>0 + báscula OK → banda sigue
              espera SOLO el sensor de ESA vía (otros se ignoran):
                                      ├─ Vision=1 + I_SensorPlastico (detecta-todo, final) → paro + P1
                                      ├─ Vision=2 + I_SensorAluminio (metal) → paro + P2
                                      ├─ Vision=3 + I_SensorVidrio → paro + P3
                                      └─ Vision=4 desconocido → pass-through (sin pistón)
```

**La IA manda.** Los sensores pueden activarse varios a la vez; solo cuenta el de la vía pedida por Gemini.  
Ver `NETWORKS_LAD.md`.

**Sin finales de carrera en pistones:** el ciclo de empuje es por **TON**, no por FC 0 % / 100 %.  
**Doble efecto biestable:** `Q_…Ext = 1` / `Q_…Ret = 0` → extiende · al revés retracta.  
**Nunca** energizar Ext y Ret a la vez (interlock en LAD).

---

## Comunes del PLC (1L / 2L) — ambos a **24 V**

| Común | Salidas |
|---|---|
| **1L** | `%Q0.0` … `%Q0.4` |
| **2L** | `%Q0.5` `%Q0.6` `%Q0.7` `%Q1.0` `%Q1.1` |

**Conexión:** `1L` y `2L` → **mismo +24 V**.  
Banda + 6 solenoides + bobinas de los 3 relés del semáforo a 24 V.

Semáforo (lámparas **220 V**): cada `Q_Lampara*` → bobina 24 V de un relé; el contacto NA conmuta fase 220 V (neutro común). Fusible/breaker aparte en 220 V.

---

## Entradas `%I` (5) — pieza / posición por vía / báscula

| Dir | Tag | Hardware | Rol |
|---|---|---|---|
| `%I0.0` | `I_SensorPieza` | Óptico / presencia en báscula | Entrada → `(S) M_Pesando` (paro para peso+foto) |
| `%I0.1` | `I_SensorPlastico` | Óptico **detecta todo** (vía P1, al **final**) | Solo frena si `VisionMaterial==1` |
| `%I0.2` | `I_SensorAluminio` | Sensor **metal** (vía P2) | Solo frena si `VisionMaterial==2` |
| `%I0.3` | `I_SensorVidrio` | Sensor **vidrio** (vía P3) | Solo frena si `VisionMaterial==3` |
| `%I0.4` | `I_BasculaLista` | Báscula lista | Peso válido + fin de `M_Pesando` |

> Tipo de material = `DB_HMI.VisionMaterial` (Gemini). Otros sensores pueden activarse a la vez: **se ignoran** si no coinciden con la IA.  
> `VisionMaterial==4` (desconocido) → pass-through, sin pistón.

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

---

## Memorias

`M_SistemaOn` · `M_ModoAuto` · `M_Alarma` · `M_ClasifPlastico` · `M_ClasifAluminio` · `M_ClasifVidrio` · `M_Clasificando`  
`M_Piston1` / `M_Piston2` / `M_Piston3` = comando “quiero extendido” → derivan `Q_…Ext` / `Q_…Ret`.

Timers: `T_EmpujePiston1/2/3` (empuje) · `T_TimeoutVision` (visión sin sensor).

---

## DBs

`DatosEstacion` DB1 · `DB_HMI` DB3 (**≥ 8 bytes**, incluye `VisionMaterial` Int @ 6.0) · Optimized **OFF**  
Ver `DB_CONTRATO_WEB.md`.
