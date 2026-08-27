# Checklist — PLC real 1214C + web (3 pistones · 6 Q · sin FC)

## Hardware / TIA
- [ ] Proyecto **nuevo** con CPU 1214C (no el de 1511C)
- [ ] IP estática anotada (ej. `192.168.0.10`)
- [ ] PUT/GET habilitado + download hardware
- [ ] `DatosEstacion` DB1 Optimized OFF · **≥ 28 bytes**
- [ ] `DB_HMI` DB3 Optimized OFF · **≥ 6 bytes** (`ManualPiston1` @1.4 · `ManualPiston2` @1.5 · `SensorVidrio` @1.6 · `PesoActualKg` @2.0)
- [ ] **Sin** tags `I_Piston*Extendido` / `Retractado` ni bools FC en `DB_HMI`
- [ ] Tag table según `TABLA_IO_1214C.md` (**5 DI** material + **10 DQ**)
- [ ] **1L y 2L** al mismo **+24 V**
- [ ] 3 relés intermedios para semáforo 220 V
- [ ] FCs según `NETWORKS_LAD.md` (`T_EmpujePistonN` por tiempo, no por FC)
- [ ] Download software + CPU RUN
- [ ] Online: forzando Ext/Ret de cada pistón se oye/ve cada solenoide
- [ ] LAD MANUAL: `M_SistemaOn` · `/M_ModoAuto` · `DB_HMI.Manual…`
- [ ] Prueba HMI: START → AUTO off → Extender / Retractar P1

## Mesa real (sin botonera)
- [ ] **Sin** pulsadores Start/Stop/Emergencia/Manual — todo desde HMI web
- [ ] 3× cilindro doble efecto + 3× válvula **5/2 biestable** (2 solenoides c/u)
- [ ] **Sin** finales de carrera en pistones
- [ ] Sensores pieza / plástico / aluminio / vidrio / báscula lista
- [ ] P1 = plástico · P2 = latas · P3 = vidrio
- [ ] Semáforo: 3 relés + 220 V

## PC / red
- [ ] PC en la misma subnet que el PLC
- [ ] Firewall permite TCP **102**
- [ ] `serviceAccountKey.json` en la carpeta del bridge
- [ ] `py -m pip install firebase-admin python-snap7`

## Bridge
```powershell
py plc_real/plc_bridge_real.py --ip 192.168.0.10
```

Si ves `Invalid address (0x05)`: **`FIX_DB_INVALID_ADDRESS.md`**  
Estación Firestore: **`colegio-don-bosco-real`**

## Prueba
1. START → `Q_LamparaRun`
2. Pieza plástica → `Q_Piston1Ext` · tras TON → cuenta + `Q_Piston1Ret`
3. Latas → P2 · vidrio → P3
4. Emergencia → paro + `Q_LamparaEmergencia`
