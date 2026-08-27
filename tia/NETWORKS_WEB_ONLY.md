# Networks — solo Web · 3 materiales (plástico / latas / vidrio)

**Regla:** sensores + operador = `DB_HMI.*`.  
Actuadores = `M_Banda`, `M_PistonN` (comando), `M_PistonNExt` / `M_PistonNRet` (solenoides sim).

```
  [Sim] → báscula → banda → sensores material
                              ├─ plástico → P1 Ext/Ret
                              ├─ latas    → P2 Ext/Ret
                              └─ vidrio   → P3 Ext/Ret
```

Doble efecto biestable · **sin** FC · ciclo AUTO por `T_EmpujePistonN`.  
PLC real: `plc_real/NETWORKS_LAD.md`.

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

### NW4 — Lámpara RUN
```
---| M_SistemaOn |---|/ DB_HMI.Emergencia |----( M_LamparaRun )
```

### NW5 — Lámpara emergencia
```
---| DB_HMI.Emergencia |----( M_LamparaEmergencia )
```

---

## FC_Secuencia

### NW1 — Banda AUTO // MANUAL → `M_Banda`
```
  AUTO:
---| M_SistemaOn |---| M_ModoAuto |---|/ DB_HMI.Emergencia |---|/ M_Alarma |---|/ M_Clasificando |--+
                                                                                                        |
  MANUAL:                                                                                               +----( M_Banda )
---| M_SistemaOn |---|/ M_ModoAuto |---| DB_HMI.ManualBanda |------------------------------------------+
```

### NW2 — Latch plástico → `(S) M_ClasifPlastico`
```
---| M_SistemaOn |---| M_ModoAuto |---| DB_HMI.SensorPieza |---| DB_HMI.BasculaLista |---| DB_HMI.SensorPlastico |
---|/ DB_HMI.SensorAluminio |---|/ DB_HMI.SensorVidrio |---|/ M_ClasifAluminio |---|/ M_ClasifVidrio |----(S M_ClasifPlastico)
```

### NW3 — Latch latas → `(S) M_ClasifAluminio`
```
---| M_SistemaOn |---| M_ModoAuto |---| DB_HMI.SensorPieza |---| DB_HMI.BasculaLista |---| DB_HMI.SensorAluminio |
---|/ DB_HMI.SensorPlastico |---|/ DB_HMI.SensorVidrio |---|/ M_ClasifPlastico |---|/ M_ClasifVidrio |----(S M_ClasifAluminio)
```

### NW4 — Latch vidrio → `(S) M_ClasifVidrio`
```
---| M_SistemaOn |---| M_ModoAuto |---| DB_HMI.SensorPieza |---| DB_HMI.BasculaLista |---| DB_HMI.SensorVidrio |
---|/ DB_HMI.SensorPlastico |---|/ DB_HMI.SensorAluminio |---|/ M_ClasifPlastico |---|/ M_ClasifAluminio |----(S M_ClasifVidrio)
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

### NW8 — P1 Ext / Ret
```
---| M_SistemaOn |---| M_Piston1 |---|/ M_Piston1Ret |----( M_Piston1Ext )
---| M_SistemaOn |---|/ M_Piston1 |---|/ M_Piston1Ext |----( M_Piston1Ret )
```

### NW9 — P2 Ext / Ret
```
---| M_SistemaOn |---| M_Piston2 |---|/ M_Piston2Ret |----( M_Piston2Ext )
---| M_SistemaOn |---|/ M_Piston2 |---|/ M_Piston2Ext |----( M_Piston2Ret )
```

### NW10 — P3 Ext / Ret
```
---| M_SistemaOn |---| M_Piston3 |---|/ M_Piston3Ret |----( M_Piston3Ext )
---| M_SistemaOn |---|/ M_Piston3 |---|/ M_Piston3Ext |----( M_Piston3Ret )
```

### NW11 — TON empuje P1
```
---| M_ClasifPlastico |----[ TON T_EmpujePiston1  PT:=T#1s ]
```

### NW12 — Contar plástico + reset
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

### NW14 — Contar latas + reset
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

### NW16 — Contar vidrio + reset
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

---

## FC_Alarmas

### NW1 — Lámpara alarma
```
---| M_Alarma |----( M_LamparaAlarma )
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
---| DB_HMI.SensorPlastico |---| DB_HMI.SensorAluminio |----(S M_Alarma)
---| DB_HMI.SensorPlastico |---| DB_HMI.SensorVidrio   |----(S M_Alarma)
---| DB_HMI.SensorAluminio |---| DB_HMI.SensorVidrio   |----(S M_Alarma)
```

---

## FC_EspejoWeb (SCL)
```scl
DatosEstacion.SistemaOn    := M_SistemaOn;
DatosEstacion.ModoAuto     := M_ModoAuto;
DatosEstacion.Emergencia   := DB_HMI.Emergencia;
DatosEstacion.Alarma       := M_Alarma;
DatosEstacion.BandaOn      := M_Banda;
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
