# Checklist — PLC real 1214C + web (3 pistones)

## Hardware / TIA
- [ ] Proyecto **nuevo** con CPU 1214C (no el de 1511C)
- [ ] IP estática anotada (ej. `192.168.0.10`)
- [ ] PUT/GET habilitado + download hardware
- [ ] `DatosEstacion` DB1 Optimized OFF · **≥ 28 bytes** (`ContVidrio` @22 · `PesoVidrioKg` @24 · `Piston1/2/3On` @17.0–17.2)
- [ ] `DB_HMI` DB3 Optimized OFF · **≥ 7 bytes** (`PesoActualKg` @2.0 · Extendido/Retractado @6.x · `SensorVidrio` @6.1)
- [ ] Tag table según `TABLA_IO_1214C.md` (**Q_Piston1..3** 5/2 + **6** FC posición 0%/100%)
- [ ] FCs según `NETWORKS_LAD.md`
- [ ] Timers: `T_RetardoPiston2/3`, `T_TimeoutPiston2/3`
- [ ] Download software + CPU RUN
- [ ] Online: forzando `Q_Banda` / `Q_Piston1` / `Q_Piston2` / `Q_Piston3` se oye/ve cada actuador
- [ ] `DB_HMI` tiene `ManualPiston1` @1.6 · `ManualPiston2` @1.7 · `ManualPiston` @0.7 (Optimized OFF)
- [ ] LAD P1–P3: rama MANUAL = `M_SistemaOn` · `/M_ModoAuto` · `DB_HMI.Manual…`
- [ ] Prueba HMI: START → AUTO off → **Extender** P1 (Q ON) → **Retractar** (Q OFF; aire mete el vástago)

## Mesa real (sin botonera)
- [ ] **Sin** pulsadores Start/Stop/Emergencia/Manual — todo desde HMI web
- [ ] 3× cilindro doble efecto + 3× válvula 5/2
- [ ] Sensores 0% y 100% en cada cilindro → `I_PistonNRetractado` / `I_PistonNExtendido`
- [ ] Sensores pieza / plástico / aluminio / báscula lista
- [ ] P1 = plástico · P2 = latas · P3 = vidrio

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

## Prueba
1. START desde web → `M_SistemaOn` / `Q_LamparaRun`
2. Pieza plástica → `Q_Piston1` activo
3. Pieza + plástico → `Q_Piston2` → sensor 100% → contador plástico
4. Pieza + aluminio → `Q_Piston3` → sensor 100% → contador aluminio
5. Emergencia desde web → paro
