# Networks — solo Web · 3 materiales (doble efecto · **sin** FC)

**Regla:** sensores + operador = `DB_HMI.*`.  
Comando = `M_Piston1/2/3` · solenoides sim = `M_PistonNExt` / `M_PistonNRet`.

Cilindros **doble efecto** · 5/2 biestable · **sin** finales de carrera · ciclo por `T_EmpujePistonN`.

```
  [Sim] → báscula → banda → sensores material
                              ├─ plástico → P1 Ext/Ret
                              ├─ latas    → P2 Ext/Ret
                              └─ vidrio   → P3 Ext/Ret
```

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

```
[ DB_HMI.Start ]──[/ Stop ]──[/ Emergencia ]──(S) M_SistemaOn
[ Stop ]──┐
          ├──(R) M_SistemaOn
[ Emerg ]─┘
[ DB_HMI.ModoAuto ]──( ) M_ModoAuto
[ M_SistemaOn ]──[/ Emergencia ]──( ) M_LamparaRun
[ M_Alarma ]──( ) M_LamparaAlarma
[ Emergencia ]──( ) M_LamparaEmergencia
```

---

## FC_Secuencia

### Banda → `M_Banda`
```
AUTO:  On · Auto · /Emerg · /Alarma · /Clasificando → M_Banda
MANUAL: On · /Auto · ManualBanda → M_Banda
```

### Latch plástico → `(S) M_ClasifPlastico` → P1
```
On · Auto · Pieza · Bascula · SensorPlastico
· /SensorAluminio · /SensorVidrio · /ClasifAluminio · /ClasifVidrio → (S) M_ClasifPlastico
```

### Latch latas → `(S) M_ClasifAluminio` → P2
```
On · Auto · Pieza · Bascula · SensorAluminio
· /SensorPlastico · /SensorVidrio · /ClasifPlastico · /ClasifVidrio → (S) M_ClasifAluminio
```

### Latch vidrio → `(S) M_ClasifVidrio` → P3
```
On · Auto · Pieza · Bascula · SensorVidrio
· /SensorPlastico · /SensorAluminio · /ClasifPlastico · /ClasifAluminio → (S) M_ClasifVidrio
```

### Comando P1/P2/P3 → `M_PistonN`
```
[ ClasifX ]──────────────────┐
                             ├──( ) M_PistonN
[ On ]─[/ Auto ]─[ Manual… ]─┘
```

| Pistón | Auto | Manual |
|---|---|---|
| P1 | `M_ClasifPlastico` | `ManualPiston1` @ 1.4 |
| P2 | `M_ClasifAluminio` | `ManualPiston2` @ 1.5 |
| P3 | `M_ClasifVidrio` | `ManualPiston` @ 0.7 |

### Solenoides Ext / Ret
```
[ M_SistemaOn ]─[ M_PistonN ]─[/ M_PistonNRet ]──( ) M_PistonNExt
[ M_SistemaOn ]─[/ M_PistonN ]─[/ M_PistonNExt ]──( ) M_PistonNRet
```

### Contar + retractar (por tiempo)
```
ClasifPlastico ──[ TON T_EmpujePiston1  PT:=T#1s ] → (R) Clasif + ContPlastico++
ClasifAluminio ──[ TON T_EmpujePiston2  PT:=T#1s ] → (R) Clasif + ContAluminio++
ClasifVidrio   ──[ TON T_EmpujePiston3  PT:=T#1s ] → (R) Clasif + ContVidrio++
```
`UltimoMaterial`: 1 plástico · 2 aluminio · 3 vidrio.

```
M_Clasificando := ClasifPlastico OR ClasifAluminio OR ClasifVidrio;
```

---

## FC_EspejoWeb

```scl
DatosEstacion.BandaOn   := M_Banda;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.Piston1On := M_Piston1;
DatosEstacion.Piston2On := M_Piston2;
DatosEstacion.Piston3On := M_Piston3;
DatosEstacion.PesoActualKg := DB_HMI.PesoActualKg;
```
