# Networks LAD — PLC real 1214C (doble efecto · **sin** FC pistones)

**Reglas:** sensores material `I_*` · actuadores `Q_*` · operador solo `DB_HMI.*` · espejo `DatosEstacion`.

| Pistón | Ext | Ret | Material |
|---|---|---|---|
| P1 | `Q_Piston1Ext` | `Q_Piston1Ret` | Plástico |
| P2 | `Q_Piston2Ext` | `Q_Piston2Ret` | Latas |
| P3 | `Q_Piston3Ext` | `Q_Piston3Ret` | Vidrio |

Cilindros **doble efecto** · 5/2 biestable · **sin** finales de carrera.  
Ciclo AUTO por **TON** (`T_EmpujePistonN`). Comunes **1L/2L → 24 V**; semáforo vía relés.  
Detalle I/O: `TABLA_IO_1214C.md`.

Sim: `tia/NETWORKS_WEB_ONLY.md`.

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

**Banda:** AUTO (On·Auto·/Emerg·/Alarma·/Clasificando) // MANUAL (`ManualBanda`) → `Q_Banda`

**Latch plástico** (`I_SensorPlastico`, excluye otros) → `(S) M_ClasifPlastico`  
**Latch latas** (`I_SensorAluminio`) → `(S) M_ClasifAluminio`  
**Latch vidrio** (`I_SensorVidrio`) → `(S) M_ClasifVidrio`

### Comando pistón → `M_PistonN`

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

### Solenoides Ext / Ret (interlock · sin corte por FC)

```
[ M_SistemaOn ]─[ M_PistonN ]─[/ Q_PistonNRet ]──( ) Q_PistonNExt
[ M_SistemaOn ]─[/ M_PistonN ]─[/ Q_PistonNExt ]──( ) Q_PistonNRet
```

**Importante:**  
- Extender = `M_PistonN = 1` → `Q_…Ext` ON  
- Retractar = `M_PistonN = 0` → `Q_…Ret` ON (con sistema ON)  
- Ext y Ret **nunca** a la vez  
- Sin sensores de posición: el tiempo de empuje lo marca el TON

### Contar + retractar (por tiempo)

```
ClasifX ──[ TON T_EmpujePistonN  PT:=T#1s ]
T_EmpujePistonN.Q → (R) ClasifX + Cont*++ / Peso*++ + UltimoMaterial
```

Al resetear el latch, `M_PistonN` cae en AUTO → `Q_…Ret` ON.

`UltimoMaterial` = 1 / 2 / 3  
Ajusta `PT` al tiempo real de carrera del cilindro (ej. `T#800ms` … `T#1500ms`).

`M_Clasificando := ClasifPlastico OR ClasifAluminio OR ClasifVidrio`

---

## FC_Alarmas

Sin alarmas de FC contradictorios.  
Reset alarma: `DB_HMI.ResetAlarma` → `(R) M_Alarma`  
(Opcional: timeout de proceso si `ClasifX` dura demasiado — no depende de sensores de pistón.)

---

## FC_EspejoWeb
```scl
DatosEstacion.BandaOn   := Q_Banda;
DatosEstacion.Piston1On := M_Piston1;
DatosEstacion.Piston2On := M_Piston2;
DatosEstacion.Piston3On := M_Piston3;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.PesoActualKg := DB_HMI.PesoActualKg;
```
