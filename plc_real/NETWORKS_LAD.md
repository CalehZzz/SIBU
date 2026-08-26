# Networks LAD — PLC real 1214C (doble efecto · 6 solenoides)

**Reglas:** sensores `I_*` · actuadores `Q_*` · operador solo `DB_HMI.*` · espejo `DatosEstacion`.

| Pistón | Ext | Ret | Material |
|---|---|---|---|
| P1 | `Q_Piston1Ext` | `Q_Piston1Ret` | Plástico |
| P2 | `Q_Piston2Ext` | `Q_Piston2Ret` | Latas |
| P3 | `Q_Piston3Ext` | `Q_Piston3Ret` | Vidrio |

Cilindros **doble efecto** · válvula **5/2 biestable** (2 solenoides) · FC 100 % / 0 %.  
Comunes **1L y 2L → 24 V**; semáforo 220 V vía relés (`Q_Lampara*`).  
Detalle I/O: `TABLA_IO_1214C.md`.

La versión sim (mismos roles, sensores en `DB_HMI`) está en `tia/NETWORKS_WEB_ONLY.md`.

---

## OB1
`FC_Modos` → `FC_Secuencia` → `FC_Alarmas` → `FC_EspejoWeb`

---

## FC_Modos
- START → `(S) M_SistemaOn` (con `/Stop` `/Emergencia`)
- STOP / EMERGENCIA → `(R) M_SistemaOn`
- `DB_HMI.ModoAuto` → `M_ModoAuto`
- Lámparas (bobinas de relé 24 V):
  - `M_SistemaOn` · `/Emergencia` → `Q_LamparaRun`
  - `M_Alarma` → `Q_LamparaAlarma`
  - `DB_HMI.Emergencia` → `Q_LamparaEmergencia`

---

## FC_Secuencia

**Banda:** AUTO (On·Auto·/Emerg·/Alarma·/Clasificando) // MANUAL (`ManualBanda`) → `Q_Banda`

**Latch plástico** (`I_SensorPlastico`, excluye otros) → `(S) M_ClasifPlastico`  
**Latch latas** (`I_SensorAluminio`) → `(S) M_ClasifAluminio`  
**Latch vidrio** (`I_SensorVidrio`) → `(S) M_ClasifVidrio`

### Comando pistón → `M_PistonN` (deseo extendido)

Una bobina por pistón, dos ramas (igual que antes):

```
AUTO:   ClasifX · /I_PistonNExtendido ──┐
                                        ├──( ) M_PistonN
MANUAL: M_SistemaOn · /M_ModoAuto · DB_HMI.Manual… ─┘
```

| Pistón | Auto | Manual (`DB_HMI`) |
|---|---|---|
| P1 | `M_ClasifPlastico` | `ManualPiston1` @ 1.6 |
| P2 | `M_ClasifAluminio` | `ManualPiston2` @ 1.7 |
| P3 | `M_ClasifVidrio` | `ManualPiston` @ 0.7 |

### Solenoides Ext / Ret (interlock + corte por FC)

```
[ M_PistonN ]─[/ I_PistonNExtendido ]─[/ Q_PistonNRet ]──( ) Q_PistonNExt
[/ M_PistonN ]─[/ I_PistonNRetractado ]─[/ Q_PistonNExt ]──( ) Q_PistonNRet
```

**Importante (doble efecto · 5/2 biestable):**  
- **Extender** = `M_PistonN = 1` → `Q_…Ext` ON hasta FC 100 %  
- **Retractar** = `M_PistonN = 0` → `Q_…Ret` ON hasta FC 0 %  
- Ext y Ret **nunca** a la vez (`/Q_…` cruzados)  
- Sin **START** (`M_SistemaOn`) la rama MANUAL no pone `M_PistonN`  
- Debe existir modo **MANUAL** (`DB_HMI.ModoAuto = 0`)  
- Casa = `I_PistonNRetractado` · fuera = `I_PistonNExtendido`

**Contar:** TON retardo con `I_PistonNExtendido` → reset latch + incrementar `Cont*` / `Peso*`  
`UltimoMaterial` = 1 / 2 / 3

**Timeouts:** TON 3 s sin FC 100 % → `(S) M_Alarma`

`M_Clasificando := ClasifPlastico OR ClasifAluminio OR ClasifVidrio`

---

## FC_Alarmas (extra doble efecto)

```
I_PistonNExtendido · I_PistonNRetractado → (S) M_Alarma   // sensores contradictorios
```

(Repetir para N = 1, 2, 3. Reset alarma con `DB_HMI.ResetAlarma`.)

---

## FC_EspejoWeb
```scl
DatosEstacion.BandaOn   := Q_Banda;
DatosEstacion.Piston1On := M_Piston1;  // comando / deseo extendido (no solo Q_Ext)
DatosEstacion.Piston2On := M_Piston2;
DatosEstacion.Piston3On := M_Piston3;
DatosEstacion.PistonOn  := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.PesoActualKg := DB_HMI.PesoActualKg;
```
