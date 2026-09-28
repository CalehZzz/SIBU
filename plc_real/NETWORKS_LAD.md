# Networks LAD — PLC real 1214C · visión Gemini + 3 sensores de posición

**Regla de oro (Costa Rica / mesa real):**

1. La **Raspberry Pi 4** + Gemini decide **qué** material es → escribe `DB_HMI.VisionMaterial` (1/2/3).
2. Los **3 sensores físicos** (`I_SensorPlastico` / `Aluminio` / `Vidrio`) son **posición en la banda** (uno por vía/pistón), no clasificación.
3. Solo cuando **visión dice X** **y** el **sensor de la vía X** se activa → se **para la banda**, se activa el pistón X y se cuenta.

```
  [Entrada] → [Báscula] → [Banda] → cámara (Gemini: VisionMaterial)
                                      │
              ┌───────────────────────┘
              ▼
   espera sensor de esa vía:
     Vision=1 + I_SensorPlastico  → paro + P1
     Vision=2 + I_SensorAluminio  → paro + P2
     Vision=3 + I_SensorVidrio    → paro + P3
```

| Pistón | Ext | Ret | Vía / sensor | VisionMaterial |
|---|---|---|---|---|
| P1 | `Q_Piston1Ext` | `Q_Piston1Ret` | `I_SensorPlastico` | **1** |
| P2 | `Q_Piston2Ext` | `Q_Piston2Ret` | `I_SensorAluminio` | **2** |
| P3 | `Q_Piston3Ext` | `Q_Piston3Ret` | `I_SensorVidrio` | **3** |

Cilindros **doble efecto** · 5/2 biestable · **sin** FC · ciclo AUTO por **TON**.  
Operador solo `DB_HMI.*`. I/O: `TABLA_IO_1214C.md`. Contrato: `DB_CONTRATO_WEB.md`.

---

## OB1
`FC_Modos` → `FC_Secuencia` → `FC_Alarmas` → `FC_EspejoWeb`

---

## FC_Modos
- START → `(S) M_SistemaOn` (con `/Stop` `/Emergencia`)
- STOP / EMERGENCIA → `(R) M_SistemaOn`
- `DB_HMI.ModoAuto` → `M_ModoAuto`
- Lámparas (bobinas relé 24 V):
  - `M_SistemaOn` · `/Emergencia` → `Q_LamparaRun`
  - `M_Alarma` → `Q_LamparaAlarma`
  - `DB_HMI.Emergencia` → `Q_LamparaEmergencia`

---

## FC_Secuencia

### NW1 — Banda AUTO // MANUAL → `Q_Banda`

```
AUTO:   M_SistemaOn · M_ModoAuto · /Emergencia · /M_Alarma · /M_Clasificando ─┐
                                                                              ├──( ) Q_Banda
MANUAL: M_SistemaOn · /M_ModoAuto · DB_HMI.ManualBanda ───────────────────────┘
```

`M_Clasificando = 1` **para la banda** mientras el pistón empuja.

---

### NW2 — Latch plástico (visión + sensor vía P1) → `(S) M_ClasifPlastico`

En TIA: contacto Compare `DB_HMI.VisionMaterial` **==** `1` (Int).

```
---| M_SistemaOn |---| M_ModoAuto |---|/ DB_HMI.Emergencia |---|/ M_Alarma |
---| VisionMaterial == 1 |---| I_SensorPlastico |
---|/ M_ClasifAluminio |---|/ M_ClasifVidrio |----(S) M_ClasifPlastico
```

> La banda sigue corriendo hasta que este latch se pone a 1 (porque `M_Clasificando` corta `Q_Banda`).

---

### NW3 — Latch latas (visión + sensor vía P2) → `(S) M_ClasifAluminio`

```
---| M_SistemaOn |---| M_ModoAuto |---|/ DB_HMI.Emergencia |---|/ M_Alarma |
---| VisionMaterial == 2 |---| I_SensorAluminio |
---|/ M_ClasifPlastico |---|/ M_ClasifVidrio |----(S) M_ClasifAluminio
```

---

### NW4 — Latch vidrio (visión + sensor vía P3) → `(S) M_ClasifVidrio`

```
---| M_SistemaOn |---| M_ModoAuto |---|/ DB_HMI.Emergencia |---|/ M_Alarma |
---| VisionMaterial == 3 |---| I_SensorVidrio |
---|/ M_ClasifPlastico |---|/ M_ClasifAluminio |----(S) M_ClasifVidrio
```

---

### NW5 — `M_Clasificando` (OR de los 3 latches)

```scl
M_Clasificando := M_ClasifPlastico OR M_ClasifAluminio OR M_ClasifVidrio;
```

(o 3 contactos en paralelo → bobina `M_Clasificando`)

---

### NW6–8 — Comando pistón → `M_PistonN`

```
AUTO:   ClasifX ─────────────────────┐
                                     ├──( ) M_PistonN
MANUAL: M_SistemaOn · /M_ModoAuto · DB_HMI.Manual… ─┘
```

| Pistón | Auto | Manual (`DB_HMI`) |
|---|---|---|
| P1 | `M_ClasifPlastico` | `ManualPiston1` @ **1.4** |
| P2 | `M_ClasifAluminio` | `ManualPiston2` @ **1.5** |
| P3 | `M_ClasifVidrio` | `ManualPiston` @ 0.7 |

---

### NW9–11 — Solenoides Ext / Ret (interlock)

```
[ M_SistemaOn ]─[ M_PistonN ]─[/ Q_PistonNRet ]──( ) Q_PistonNExt
[ M_SistemaOn ]─[/ M_PistonN ]─[/ Q_PistonNExt ]──( ) Q_PistonNRet
```

Nunca Ext y Ret a la vez. Empuje por tiempo (TON), no por FC.

---

### NW12–14 — Contar + retractar + **limpiar visión**

```
ClasifX ──[ TON T_EmpujePistonN  PT:=T#1s ]
```

Cuando `T_EmpujePistonN.Q = 1`:

1. `(R) M_ClasifX`
2. Contador / peso / `UltimoMaterial` (SCL abajo)
3. **`DB_HMI.VisionMaterial := 0`** ← obligatorio (libera la siguiente pieza)

```scl
IF T_EmpujePiston1.Q THEN
    M_ClasifPlastico := FALSE;
    DB_HMI.VisionMaterial := 0;
    DatosEstacion.ContPlastico := DatosEstacion.ContPlastico + 1;
    DatosEstacion.PesoPlasticoKg := DatosEstacion.PesoPlasticoKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 1;
END_IF;

IF T_EmpujePiston2.Q THEN
    M_ClasifAluminio := FALSE;
    DB_HMI.VisionMaterial := 0;
    DatosEstacion.ContAluminio := DatosEstacion.ContAluminio + 1;
    DatosEstacion.PesoAluminioKg := DatosEstacion.PesoAluminioKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 2;
END_IF;

IF T_EmpujePiston3.Q THEN
    M_ClasifVidrio := FALSE;
    DB_HMI.VisionMaterial := 0;
    DatosEstacion.ContVidrio := DatosEstacion.ContVidrio + 1;
    DatosEstacion.PesoVidrioKg := DatosEstacion.PesoVidrioKg + DatosEstacion.PesoActualKg;
    DatosEstacion.UltimoMaterial := 3;
END_IF;
```

Ajusta `PT` al tiempo real del cilindro (`T#800ms` … `T#1500ms`).

Al caer el latch, `M_PistonN` cae en AUTO → `Q_…Ret` ON → banda vuelve (porque `M_Clasificando` = 0).

---

## FC_Alarmas

### Timeout visión sin sensor (pieza perdida / vía incorrecta)

```
---| VisionMaterial <> 0 |---|/ M_Clasificando |----[ TON T_TimeoutVision  PT:=T#8s ]
---| T_TimeoutVision.Q |----(S) M_Alarma
---| T_TimeoutVision.Q |----  MOVE 0 → DB_HMI.VisionMaterial
```

Si Gemini dijo un material pero el sensor de esa vía nunca llegó en 8 s → alarma y limpia visión.

### Reset / emergencia

```
---| DB_HMI.ResetAlarma |----(R) M_Alarma
---| DB_HMI.Emergencia  |----(S) M_Alarma
---| DB_HMI.Emergencia  |---- MOVE 0 → VisionMaterial + (R) Clasif*
```

---

## FC_EspejoWeb
```scl
DatosEstacion.BandaOn   := Q_Banda;
DatosEstacion.Piston1On := M_Piston1;
DatosEstacion.Piston2On := M_Piston2;
DatosEstacion.Piston3On := M_Piston3;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.PesoActualKg := DB_HMI.PesoActualKg;
DatosEstacion.SistemaOn := M_SistemaOn;
DatosEstacion.ModoAuto  := M_ModoAuto;
DatosEstacion.Alarma    := M_Alarma;
```

---

## Tags nuevos (memorias / timers)

| Tag | Tipo | Uso |
|---|---|---|
| `M_ClasifPlastico` / `Aluminio` / `Vidrio` | Bool | Latch vía |
| `M_Clasificando` | Bool | Para banda |
| `M_Piston1/2/3` | Bool | Comando extendido |
| `T_EmpujePiston1/2/3` | TON | Tiempo empuje |
| `T_TimeoutVision` | TON | Visión sin sensor |
| `DB_HMI.VisionMaterial` | Int @ **6.0** | 0 ninguno · 1 plástico · 2 aluminio · 3 vidrio |

---

## Quién escribe qué

| Campo | Quién |
|---|---|
| `DB_HMI` bytes 0–5 (Start, sensores sim HMI, peso…) | Bridge HMI / web |
| `DB_HMI.VisionMaterial` @ 6.0 | **Solo** servicio visión en Pi 4 (Gemini) |
| Clear `VisionMaterial := 0` | **PLC** al terminar empuje o timeout |
| `I_Sensor*` | Hardware (ópticos de posición) |
