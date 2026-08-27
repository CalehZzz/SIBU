# Networks LAD — PLC real 1214C (plástico / latas / vidrio)

**Reglas:** sensores material `I_*` · actuadores `Q_*` · operador solo `DB_HMI.*` · espejo `DatosEstacion`.

| Pistón | Ext | Ret | Material | Manual (`DB_HMI`) |
|---|---|---|---|---|
| P1 | `Q_Piston1Ext` | `Q_Piston1Ret` | Plástico | `ManualPiston1` @ 1.4 |
| P2 | `Q_Piston2Ext` | `Q_Piston2Ret` | Latas | `ManualPiston2` @ 1.5 |
| P3 | `Q_Piston3Ext` | `Q_Piston3Ret` | Vidrio | `ManualPiston` @ 0.7 |

Doble efecto biestable · **sin** FC de posición · ciclo AUTO por `T_EmpujePistonN`.  
I/O: `TABLA_IO_1214C.md`. Sim: `tia/NETWORKS_WEB_ONLY.md`.

| Símbolo | En TIA |
|---|---|
| `\| X \|` | Contacto NO |
| `\|/ X \|` | Contacto NC |
| `( X )` | Bobina |
| `(S X)` / `(R X)` | Set / Reset |
| `[TON]` | Caja TON |

---

## OB1

| NW | Call |
|---|---|
| 1 | `FC_Modos` |
| 2 | `FC_Secuencia` |
| 3 | `FC_Alarmas` |
| 4 | `FC_EspejoWeb` |

---

## FC_Modos

### NW1 — START → `(S) M_SistemaOn`
```
---| DB_HMI.Start |---|/ DB_HMI.Stop |---|/ DB_HMI.Emergencia |----(S M_SistemaOn)
```

### NW2 — STOP / EMERGENCIA → `(R) M_SistemaOn`
```
---| DB_HMI.Stop       |----+----(R M_SistemaOn)
---| DB_HMI.Emergencia |----+
```

### NW3 — Modo auto
```
---| DB_HMI.ModoAuto |----( M_ModoAuto )
```

### NW4 — Lámpara RUN (relé 24 V → verde 220 V)
```
---| M_SistemaOn |---|/ DB_HMI.Emergencia |----( Q_LamparaRun )
```

### NW5 — Lámpara emergencia
```
---| DB_HMI.Emergencia |----( Q_LamparaEmergencia )
```

---

## FC_Secuencia

### NW1 — Banda AUTO // MANUAL → `Q_Banda`
```
  AUTO:
---| M_SistemaOn |---| M_ModoAuto |---|/ DB_HMI.Emergencia |---|/ M_Alarma |---|/ M_Clasificando |--+
                                                                                                        |
  MANUAL:                                                                                               +----( Q_Banda )
---| M_SistemaOn |---|/ M_ModoAuto |---| DB_HMI.ManualBanda |------------------------------------------+
```

### NW2 — Latch plástico → `(S) M_ClasifPlastico`
```
---| M_SistemaOn |---| M_ModoAuto |---| I_SensorPieza |---| I_BasculaLista |---| I_SensorPlastico |
---|/ I_SensorAluminio |---|/ I_SensorVidrio |---|/ M_ClasifAluminio |---|/ M_ClasifVidrio |----(S M_ClasifPlastico)
```

### NW3 — Latch latas → `(S) M_ClasifAluminio`
```
---| M_SistemaOn |---| M_ModoAuto |---| I_SensorPieza |---| I_BasculaLista |---| I_SensorAluminio |
---|/ I_SensorPlastico |---|/ I_SensorVidrio |---|/ M_ClasifPlastico |---|/ M_ClasifVidrio |----(S M_ClasifAluminio)
```

### NW4 — Latch vidrio → `(S) M_ClasifVidrio`
```
---| M_SistemaOn |---| M_ModoAuto |---| I_SensorPieza |---| I_BasculaLista |---| I_SensorVidrio |
---|/ I_SensorPlastico |---|/ I_SensorAluminio |---|/ M_ClasifPlastico |---|/ M_ClasifAluminio |----(S M_ClasifVidrio)
```

### NW5 — Comando P1 → `M_Piston1`
```
  AUTO:
---| M_ClasifPlastico |--+
                          |
  MANUAL:                 +----( M_Piston1 )
---| M_SistemaOn |---|/ M_ModoAuto |---| DB_HMI.ManualPiston1 |--+
```

### NW6 — Comando P2 → `M_Piston2`
```
  AUTO:
---| M_ClasifAluminio |--+
                          |
  MANUAL:                 +----( M_Piston2 )
---| M_SistemaOn |---|/ M_ModoAuto |---| DB_HMI.ManualPiston2 |--+
```

### NW7 — Comando P3 → `M_Piston3`
```
  AUTO:
---| M_ClasifVidrio |--+
                        |
  MANUAL:               +----( M_Piston3 )
---| M_SistemaOn |---|/ M_ModoAuto |---| DB_HMI.ManualPiston |--+
```

### NW8 — P1 Ext / Ret → `Q_Piston1Ext` · `Q_Piston1Ret`
```
---| M_SistemaOn |---| M_Piston1 |---|/ Q_Piston1Ret |----( Q_Piston1Ext )
---| M_SistemaOn |---|/ M_Piston1 |---|/ Q_Piston1Ext |----( Q_Piston1Ret )
```

### NW9 — P2 Ext / Ret → `Q_Piston2Ext` · `Q_Piston2Ret`
```
---| M_SistemaOn |---| M_Piston2 |---|/ Q_Piston2Ret |----( Q_Piston2Ext )
---| M_SistemaOn |---|/ M_Piston2 |---|/ Q_Piston2Ext |----( Q_Piston2Ret )
```

### NW10 — P3 Ext / Ret → `Q_Piston3Ext` · `Q_Piston3Ret`
```
---| M_SistemaOn |---| M_Piston3 |---|/ Q_Piston3Ret |----( Q_Piston3Ext )
---| M_SistemaOn |---|/ M_Piston3 |---|/ Q_Piston3Ext |----( Q_Piston3Ret )
```

> Extender = `M_PistonN = 1` → Ext ON. Retractar = `M_PistonN = 0` → Ret ON. Nunca ambos.

### NW11 — TON empuje P1
```
---| M_ClasifPlastico |----[ TON T_EmpujePiston1  PT:=T#1s ]
```

### NW12 — Contar plástico + reset latch
**LAD:** `---| T_EmpujePiston1.Q |----(R M_ClasifPlastico)`

**SCL:**
```scl
IF T_EmpujePiston1.Q THEN
    DatosEstacion.ContPlastico := DatosEstacion.ContPlastico + 1;
    DatosEstacion.PesoPlasticoKg := DatosEstacion.PesoPlasticoKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 1;
END_IF;
```

### NW13 — TON empuje P2
```
---| M_ClasifAluminio |----[ TON T_EmpujePiston2  PT:=T#1s ]
```

### NW14 — Contar latas + reset latch
**LAD:** `---| T_EmpujePiston2.Q |----(R M_ClasifAluminio)`

**SCL:**
```scl
IF T_EmpujePiston2.Q THEN
    DatosEstacion.ContAluminio := DatosEstacion.ContAluminio + 1;
    DatosEstacion.PesoAluminioKg := DatosEstacion.PesoAluminioKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 2;
END_IF;
```

### NW15 — TON empuje P3
```
---| M_ClasifVidrio |----[ TON T_EmpujePiston3  PT:=T#1s ]
```

### NW16 — Contar vidrio + reset latch
**LAD:** `---| T_EmpujePiston3.Q |----(R M_ClasifVidrio)`

**SCL:**
```scl
IF T_EmpujePiston3.Q THEN
    DatosEstacion.ContVidrio := DatosEstacion.ContVidrio + 1;
    DatosEstacion.PesoVidrioKg := DatosEstacion.PesoVidrioKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 3;
END_IF;
```

### NW17 — `M_Clasificando` (OR)
```
---| M_ClasifPlastico  |----+
---| M_ClasifAluminio  |----+----( M_Clasificando )
---| M_ClasifVidrio    |----+
```

Ajusta `PT` de cada TON al tiempo real de carrera (ej. `T#800ms` … `T#1500ms`).

---

## FC_Alarmas

### NW1 — Lámpara alarma
```
---| M_Alarma |----( Q_LamparaAlarma )
```

### NW2 — Reset alarma
```
---| DB_HMI.ResetAlarma |----(R M_Alarma)
```

### NW3 — Emergencia → alarma
```
---| DB_HMI.Emergencia |----(S M_Alarma)
```

### NW4 — Sensores material contradictorios
```
---| I_SensorPlastico |---| I_SensorAluminio |----(S M_Alarma)
---| I_SensorPlastico |---| I_SensorVidrio   |----(S M_Alarma)
---| I_SensorAluminio |---| I_SensorVidrio   |----(S M_Alarma)
```

---

## FC_EspejoWeb (SCL)
```scl
DatosEstacion.SistemaOn    := M_SistemaOn;
DatosEstacion.ModoAuto     := M_ModoAuto;
DatosEstacion.Emergencia   := DB_HMI.Emergencia;
DatosEstacion.Alarma       := M_Alarma;
DatosEstacion.BandaOn      := Q_Banda;
DatosEstacion.Piston1On    := M_Piston1;
DatosEstacion.Piston2On    := M_Piston2;
DatosEstacion.Piston3On    := M_Piston3;
DatosEstacion.PistonOn     := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.FinSesion    := DB_HMI.FinSesion;
DatosEstacion.PesoActualKg := DB_HMI.PesoActualKg;

IF DB_HMI.Emergencia THEN
    DatosEstacion.EstadoMaquina := 4;
ELSIF M_Alarma THEN
    DatosEstacion.EstadoMaquina := 3;
ELSIF M_Clasificando THEN
    DatosEstacion.EstadoMaquina := 2;
ELSIF M_SistemaOn THEN
    DatosEstacion.EstadoMaquina := 1;
ELSE
    DatosEstacion.EstadoMaquina := 0;
END_IF;
```
