# Networks — solo Web · 3 materiales (plástico / latas / vidrio)

**Regla:** sensores + operador = `DB_HMI.*`.  
Actuadores = `M_Banda`, `M_Piston1` (plástico), `M_Piston2` (latas/aluminio), `M_Piston3` (vidrio).

Cilindros **doble efecto** · 5/2 monoestable · FC `PistonNExtendido` (100%) + `PistonNRetractado` (0%) por pistón.

```
  [Sim] → báscula → banda → sensores material
                              ├─ plástico → P1
                              ├─ latas    → P2
                              └─ vidrio   → P3
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

### P1 plástico → `M_Piston1`
```
[ ClasifPlastico ]─[/ Piston1Extendido ]─┐
                                         ├──( ) M_Piston1
[ On ]─[/ Auto ]─[ ManualPiston1 ]───────┘
```

### P2 latas → `M_Piston2`
```
[ ClasifAluminio ]─[/ Piston2Extendido ]─┐
                                         ├──( ) M_Piston2
[ On ]─[/ Auto ]─[ ManualPiston2 ]───────┘
```

### P3 vidrio → `M_Piston3`
```
[ ClasifVidrio ]─[/ Piston3Extendido ]─┐
                                       ├──( ) M_Piston3
[ On ]─[/ Auto ]─[ ManualPiston ]──────┘
```

`M_PistonN = 1` → simula solenoide 5/2 (extiende).  
`M_PistonN = 0` → válvula en reposo (aire retracta). Casa = `PistonNRetractado`.

### Retardo + contar (cada material)
```
ClasifPlastico · Piston1Extendido → TON T_RetardoPiston1 → (R) Clasif + ContPlastico++
ClasifAluminio · Piston2Extendido → TON T_RetardoPiston2 → (R) Clasif + ContAluminio++
ClasifVidrio   · Piston3Extendido → TON T_RetardoPiston3 → (R) Clasif + ContVidrio++
```
`UltimoMaterial`: 1 plástico · 2 aluminio · 3 vidrio.

### Timeouts
```
ClasifX · TON Timeout → /PistonNExtendido → (S) M_Alarma
```

```
M_Clasificando := ClasifPlastico OR ClasifAluminio OR ClasifVidrio;
```

---

## FC_Alarmas

```
PistonNExtendido · PistonNRetractado → (S) M_Alarma   // N = 1..3
```

---

## FC_EspejoWeb

```scl
DatosEstacion.BandaOn   := M_Banda;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.Piston1On := M_Piston1;  // plástico
DatosEstacion.Piston2On := M_Piston2;  // latas
DatosEstacion.Piston3On := M_Piston3;  // vidrio
DatosEstacion.PesoActualKg := DB_HMI.PesoActualKg;
```
