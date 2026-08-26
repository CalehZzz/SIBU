# Checklist — PLC real 1214C + web (3 pistones · 6 Q + relés)

## Hardware / TIA
- [ ] Proyecto **nuevo** con CPU 1214C (no el de 1511C)
- [ ] IP estática anotada (ej. `192.168.0.10`)
- [ ] PUT/GET habilitado + download hardware
- [ ] `DatosEstacion` DB1 Optimized OFF · **≥ 28 bytes** (`ContVidrio` @22 · `PesoVidrioKg` @24 · `Piston1/2/3On` @17.0–17.2)
- [ ] `DB_HMI` DB3 Optimized OFF · **≥ 7 bytes** (`PesoActualKg` @2.0 · Ext @1.4/1.5/6.0 · Ret @6.2–6.4 · `SensorVidrio` @6.1)
- [ ] Tag table según `TABLA_IO_1214C.md` (**6 solenoides** Ext/Ret + banda + 3 relés + 6 FC)
- [ ] **1L y 2L** al mismo **+24 V** (no mezclar 220 V en el PLC)
- [ ] 3 relés intermedios: bobina 24 V ← `Q_Lampara*` · contacto NA → lámparas 220 V
- [ ] FCs según `NETWORKS_LAD.md` (comando `M_PistonN` → `Q_…Ext` / `Q_…Ret`)
- [ ] Timers: `T_RetardoPiston1/2/3`, `T_TimeoutPiston1/2/3`
- [ ] Download software + CPU RUN
- [ ] Online: forzando `Q_Banda` / `Q_Piston1Ext` / `Q_Piston1Ret` (y P2/P3) se oye/ve cada solenoide
- [ ] `DB_HMI` tiene `ManualPiston1` @1.6 · `ManualPiston2` @1.7 · `ManualPiston` @0.7 (Optimized OFF)
- [ ] LAD P1–P3: rama MANUAL = `M_SistemaOn` · `/M_ModoAuto` · `DB_HMI.Manual…`
- [ ] Prueba HMI: START → AUTO off → **Extender** P1 (Ext ON) → **Retractar** (Ret ON)

## Mesa real (sin botonera)
- [ ] **Sin** pulsadores Start/Stop/Emergencia/Manual — todo desde HMI web
- [ ] 3× cilindro doble efecto + 3× válvula **5/2 biestable** (2 solenoides c/u)
- [ ] Sensores 0 % y 100 % en cada cilindro → `I_PistonNRetractado` / `I_PistonNExtendido`
- [ ] Sensores pieza / plástico / aluminio / vidrio / báscula lista
- [ ] P1 = plástico · P2 = latas · P3 = vidrio
- [ ] Semáforo: 3 relés + 220 V (no directo al PLC)

## PC / red
- [ ] PC en la misma subnet que el PLC
- [ ] Firewall permite TCP **102**
- [ ] `serviceAccountKey.json` en la carpeta del bridge
- [ ] `py -m pip install firebase-admin python-snap7`

## Bridge
```powershell
py plc_real/plc_bridge_real.py
# o edita IP dentro del script / pásala así:
py plc_real/plc_bridge_real.py --ip 192.168.0.10
```

Si ves `Invalid address (0x05)` o “DB_HMI demasiado pequeño”: **`FIX_DB_INVALID_ADDRESS.md`**  
Diagnóstico: `py plc_probe.py --ip 192.168.0.10`

Estación Firestore: **`colegio-don-bosco-real`**

## Web
- [ ] Apartado **PLC real** en la app
- [ ] Acceso con Google
- [ ] Conectar a estación real / panel HMI real (sin sim de sensores AS)
- [ ] Chips P1 / P2 / P3 en vivo
- [ ] Sim: botones **100 %** / **0 %** por pistón (o AUTO feedback)

## Prueba
1. START desde web → `M_SistemaOn` / `Q_LamparaRun` (relé verde)
2. Pieza plástica → `M_Piston1` · `Q_Piston1Ext` activo
3. FC 100 % P1 → cuenta plástico · luego `Q_Piston1Ret` hasta 0 %
4. Latas → P2 Ext/Ret · vidrio → P3 Ext/Ret
5. Emergencia desde web → paro + `Q_LamparaEmergencia`
