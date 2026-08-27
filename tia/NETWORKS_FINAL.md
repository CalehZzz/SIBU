# Networks FINAL — sin confusión

> **Modo actual:** simulación **solo HMI · 3 pistones doble efecto (Ext/Ret · sin FC)** → **`tia/NETWORKS_WEB_ONLY.md`**.  
> Este archivo describe el esquema anterior (1 pistón). No lo uses para el proyecto sim actual.

**Regla única (modo web-only · legado 1 pistón):**
- `DB_HMI.*` = botones **y sensores simulados** de la web
- `M_Banda` / `M_Piston` / latches = lógica interna PLC
- `DatosEstacion` = espejo a la web
- **No** AS / KEPServer

---

## OB1
| NW | Contenido |
|---|---|
| 1 | Call `FC_Modos` |
| 2 | Call `FC_Secuencia` |
| 3 | Call `FC_Alarmas` |
| 4 | Call `FC_EspejoWeb` |

---

## FC_Modos

### NW1 — START
```
---| DB_HMI.Start |---|/ DB_HMI.Stop |---|/ DB_HMI.Emergencia |----(S M_SistemaOn)
```

### NW2 — STOP / EMERGENCIA
```
---| DB_HMI.Stop       |----+----(R M_SistemaOn)
---| DB_HMI.Emergencia |----+
```

### NW3 — Modo auto (copia desde web)
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

### NW2 — Pulso plástico
```
---| M_SistemaOn |---| M_ModoAuto |---| M_SensorPieza |---| M_BasculaLista |
---| M_SensorPlastico |---|/ M_SensorAluminio |----(P M_PulsePlastico)
```
(memoria flanco P: `M_EdgePlastico`)

### NW3 — Contar plástico (SCL)
```scl
IF M_PulsePlastico THEN
    DatosEstacion.ContPlastico := DatosEstacion.ContPlastico + 1;
    DatosEstacion.PesoPlasticoKg := DatosEstacion.PesoPlasticoKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 1;
END_IF;
```
*(Peso: copia `DB_HMI.PesoActualKg` a `DatosEstacion.PesoActualKg` en EspejoWeb o aquí)*

### NW4 — Aluminio → clasificar
```
---| M_SistemaOn |---| M_ModoAuto |---| M_SensorPieza |---| M_BasculaLista |
---| M_SensorAluminio |---|/ M_SensorPlastico |----(S M_Clasificando)
```

### NW5 — Pistón AUTO // MANUAL → `M_Piston`
```
  AUTO:
---| M_Clasificando |---|/ M_PistonExtendido |--+
                                                |
  MANUAL:                                       +----( M_Piston )
---| M_SistemaOn |---|/ M_ModoAuto |---| DB_HMI.ManualPiston |--+
```

### NW6 — TON retardo
```
---| M_Clasificando |---| M_PistonExtendido |----[ TON T_RetardoPiston  PT:=T#500ms ]
```

### NW7 — Contar aluminio
**LAD:** `---| T_RetardoPiston.Q |----(R M_Clasificando)`

**SCL:**
```scl
IF T_RetardoPiston.Q THEN
    DatosEstacion.ContAluminio := DatosEstacion.ContAluminio + 1;
    DatosEstacion.PesoAluminioKg := DatosEstacion.PesoAluminioKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 2;
END_IF;
```

### NW8 — Timeout pistón
```
---| M_Clasificando |----[ TON T_TimeoutPiston  PT:=T#3s ]
---| T_TimeoutPiston.Q |---|/ M_PistonExtendido |----(S M_Alarma)
```

### NW9 — Sensores contradictorios
```
---| M_SensorPieza |---| M_SensorPlastico |---| M_SensorAluminio |----(S M_Alarma)
```

---

## FC_Alarmas

### NW1
```
---| M_Alarma |----( M_LamparaAlarma )
```

### NW2
```
---| DB_HMI.ResetAlarma |----(R M_Alarma)
```

### NW3
```
---| DB_HMI.Emergencia |----(S M_Alarma)
```

---

## FC_EspejoWeb (SCL)
```scl
DatosEstacion.SistemaOn   := M_SistemaOn;
DatosEstacion.ModoAuto    := M_ModoAuto;
DatosEstacion.Emergencia  := DB_HMI.Emergencia;
DatosEstacion.Alarma      := M_Alarma;
DatosEstacion.BandaOn     := M_Banda;
DatosEstacion.PistonOn    := M_Piston;
DatosEstacion.FinSesion   := DB_HMI.FinSesion;
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

---

## KEPServer — solo estas M (AS)

| Tag PLC | AS |
|---|---|
| `M_SensorPieza` | sensor pieza |
| `M_SensorPlastico` | plástico |
| `M_SensorAluminio` | aluminio |
| `M_BasculaLista` | báscula lista |
| `M_PistonRetractado` | sensor 0% |
| `M_PistonExtendido` | sensor 100% |
| `M_Banda` | banda |
| `M_Piston` | solenoide |

**No mapees `DB_HMI` en KEP.**

---

## Web 🖥️ — solo DB_HMI

Start, Stop, Emergencia, ResetAlarma, ModoAuto, FinSesion, ManualBanda, ManualPiston, PesoActualKg, (sims opcionales si no tienes AS).
