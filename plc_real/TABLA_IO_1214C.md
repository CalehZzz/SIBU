# I/O — CPU 1214C (pistones + botes HC-SR04 + contadores físicos)

Operador de mesa: **HMI web** (plus del equipo). Rúbrica: **AS (sim) + TIA + este físico**. Mesa: sensores de vía + banda + 3 pistones + **4 botes con HC-SR04** + contadores físicos + semáforo.

```
  Entrada → foto Gemini (banda sigue)
       ├─ Vision=1 + bote P OK  → P1 + contador físico
       ├─ Vision=1 + bote P LLENO → desvío rechazo · lámpara AMARILLA
       ├─ (igual A/V)
       ├─ Vision=4 → rechazo
       └─ bote rechazo LLENO → lámpara ROJA · STOP TOTAL
```

Ver `NETWORKS_LAD.md` · Arduino: `costa_rica/arduino_bins/`.

---

## Entradas `%I`

| Dir | Tag | Hardware | Rol |
|---|---|---|---|
| `%I0.0` | `I_SensorPieza` | Óptico entrada | Trigger visión |
| `%I0.1` | `I_SensorPlastico` | Óptico vía P1 | Latch P1 si Vision=1 y bote P OK |
| `%I0.2` | `I_SensorAluminio` | Metal vía P2 | Latch P2 |
| `%I0.3` | `I_SensorVidrio` | Vidrio vía P3 | Latch P3 |
| `%I0.4` | `I_BasculaFinal` | Báscula final (demo) | Opcional |
| `%I0.5` | `I_BinFullPlastico` | Arduino OUT full P | Bote plástico lleno |
| `%I0.6` | `I_BinFullAluminio` | Arduino OUT full A | Bote latas lleno |
| `%I0.7` | `I_BinFullVidrio` | Arduino OUT full V | Bote vidrio lleno |
| `%I1.0` | `I_BinFullRechazo` | Arduino OUT full R | Bote rechazo lleno → **stop** |

> Si no cableás DI, el bridge puede escribir Merker `M_BinFull*` desde Firestore `bins_pi/estado`.

---

## Salidas `%Q`

| Dir | Tag | Rol |
|---|---|---|
| `%Q0.0` | `Q_Banda` | Banda |
| `%Q0.1`…`%Q0.6` | `Q_PistonNExt/Ret` | 6 solenoides |
| `%Q0.7` | `Q_LamparaRun` | **Verde** — OK |
| `%Q1.0` | `Q_LamparaAlarma` | **Roja** — rechazo lleno / alarma / emergencia |
| `%Q1.1` | `Q_LamparaAmarilla` | **Amarilla** — algún bote material lleno (desvío) |
| `%Q1.2`* | `Q_ContPlastico` | Pulso contador **físico** P |
| `%Q1.3`* | `Q_ContAluminio` | Pulso contador físico A |
| `%Q1.4`* | `Q_ContVidrio` | Pulso contador físico V |

\* Expansión SM1222 si la CPU solo tiene 10 DQ.  
`1L`/`2L` → +24 V.

---

## Memorias / timers

`M_SistemaOn` · `M_ModoAuto` · `M_Alarma` · `M_StopBotes` · `M_EsperandoVision`  
`M_DivertPlastico/Aluminio/Vidrio` · `M_PassThrough` · `M_Clasif*` · `M_Clasificando` · `M_PistonN`  
`T_EmpujePiston*` · `T_PulsoCont*` · `T_TimeoutVision` · `T_TimeoutVia`

---

## DBs

`DatosEstacion` DB1 · `DB_HMI` DB3 · Optimized **OFF**  
Ver `DB_CONTRATO_WEB.md`.
