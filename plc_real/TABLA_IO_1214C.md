# I/O — CPU 1214C (3 pistones · **sin espera de peso**)

Operador **100 % web** (`DB_HMI`). En mesa: sensores de posición + banda + 3 cilindros **doble efecto** + válvulas **5/2 biestables** + semáforo vía **relés**.  
Báscula solo al **final** (demo / desconocidos) — **poca importancia**, no gatea el ciclo.

| Pistón | Extender | Retractar | Material |
|---|---|---|---|
| **P1** | `Q_Piston1Ext` | `Q_Piston1Ret` | Plástico |
| **P2** | `Q_Piston2Ext` | `Q_Piston2Ret` | Latas (aluminio) |
| **P3** | `Q_Piston3Ext` | `Q_Piston3Ret` | Vidrio |

```
  [Entrada] I_SensorPieza → foto Gemini (banda SIGUE; no espera peso)
                                      │
              VisionMaterial 1/2/3/4 mientras avanza
                                      ├─ Vision=1 + I_SensorPlastico → paro + P1
                                      ├─ Vision=2 + I_SensorAluminio → paro + P2
                                      ├─ Vision=3 + I_SensorVidrio → paro + P3
                                      └─ Vision=4 → pass-through → final
                                           └─ báscula DEMO (opcional, cosmético)
```

**La IA manda.** Los sensores de vía solo confirman posición.  
Ver `NETWORKS_LAD.md`.

**Sin finales de carrera en pistones:** ciclo por **TON**.  
**Nunca** energizar Ext y Ret a la vez.

---

## Comunes del PLC (1L / 2L) — ambos a **24 V**

| Común | Salidas |
|---|---|
| **1L** | `%Q0.0` … `%Q0.4` |
| **2L** | `%Q0.5` `%Q0.6` `%Q0.7` `%Q1.0` `%Q1.1` |

**Conexión:** `1L` y `2L` → **mismo +24 V**.

---

## Entradas `%I` (5)

| Dir | Tag | Hardware | Rol |
|---|---|---|---|
| `%I0.0` | `I_SensorPieza` | Óptico **entrada** | Trigger cámara / `(S) M_EsperandoVision` — **no** para por peso |
| `%I0.1` | `I_SensorPlastico` | Óptico vía P1 | Solo frena si `VisionMaterial==1` |
| `%I0.2` | `I_SensorAluminio` | Metal vía P2 | Solo frena si `VisionMaterial==2` |
| `%I0.3` | `I_SensorVidrio` | Vidrio vía P3 | Solo frena si `VisionMaterial==3` |
| `%I0.4` | `I_BasculaFinal` | Báscula al **final** (desconocidos / demo) | **Opcional** · no gatea clasificación |

> Antes: `I_BasculaLista` + paro `M_Pesando`. **Eliminado** del ciclo.  
> `VisionMaterial==4` → pass-through; la báscula al final es solo representación.

---

## Salidas `%Q` (10 de la CPU + **3 contadores físicos**)

| Dir | Tag | Común | Hardware (24 V en el PLC) |
|---|---|---|---|
| `%Q0.0` | `Q_Banda` | 1L | Contactor / relé banda |
| `%Q0.1` | `Q_Piston1Ext` | 1L | Solenoide A — P1 plástico |
| `%Q0.2` | `Q_Piston1Ret` | 1L | Solenoide B — P1 plástico |
| `%Q0.3` | `Q_Piston2Ext` | 1L | Solenoide A — P2 latas |
| `%Q0.4` | `Q_Piston2Ret` | 1L | Solenoide B — P2 latas |
| `%Q0.5` | `Q_Piston3Ext` | 2L | Solenoide A — P3 vidrio |
| `%Q0.6` | `Q_Piston3Ret` | 2L | Solenoide B — P3 vidrio |
| `%Q0.7` | `Q_LamparaRun` | 2L | Relé → verde 220 V |
| `%Q1.0` | `Q_LamparaAlarma` | 2L | Relé → rojo 220 V |
| `%Q1.1` | `Q_LamparaEmergencia` | 2L | Relé → amarillo 220 V |
| **`%Q1.2`*** | **`Q_ContPlastico`** | † | **Pulso → contador FÍSICO plástico** |
| **`%Q1.3`*** | **`Q_ContAluminio`** | † | **Pulso → contador FÍSICO latas** |
| **`%Q1.4`*** | **`Q_ContVidrio`** | † | **Pulso → contador FÍSICO vidrio** |

\* La 1214C base suele tener solo hasta `%Q1.1` (10 DQ). Para los 3 contadores:
- agregá módulo **SM 1222** (DQ) o el DO que les den, **o**
- reasigná 3 salidas libres si el MLFB trae más DQ.

† Común del módulo de salidas (mismo +24 V).

**Contadores:** los que den en mesa (electromecánicos / digitales de pulso).  
Cada clasificación exitosa → **1 pulso corto** (~100 ms) en `Q_Cont*`.  
Eso es lo que cuenta en la demo; el Int `DatosEstacion.Cont*` es solo espejo web (opcional).

---

## Memorias

`M_SistemaOn` · `M_ModoAuto` · `M_Alarma` · `M_EsperandoVision`  
`M_ClasifPlastico` · `M_ClasifAluminio` · `M_ClasifVidrio` · `M_Clasificando` · `M_PassThrough`  
`M_Piston1/2/3` → `Q_…Ext` / `Q_…Ret`  
`M_PulsoCont1/2/3` (opcional, si el pulso lo armás con TON)

Timers: `T_EmpujePiston1/2/3` · `T_PulsoCont1/2/3` (~100 ms) · `T_TimeoutVision` · `T_TimeoutVia`

---

## DBs

`DatosEstacion` DB1 · `DB_HMI` DB3 (**≥ 8 bytes**, `VisionMaterial` Int @ 6.0) · Optimized **OFF**  
`DB_HMI.BasculaLista` @ 1.0 puede quedar como espejo de `I_BasculaFinal` (demo web); **no** usarlo para liberar clasificación.  
Ver `DB_CONTRATO_WEB.md`.
