# Checklist — PLC real 1214C + visión Gemini + web (Pi 4)

## Hardware / TIA
- [ ] Proyecto **nuevo** con CPU 1214C (no el de 1511C)
- [ ] IP estática anotada (ej. `192.168.0.10`)
- [ ] PUT/GET habilitado + download hardware
- [ ] `DatosEstacion` DB1 Optimized OFF · **≥ 28 bytes**
- [ ] `DB_HMI` DB3 Optimized OFF · **≥ 8 bytes**
  - ManualPiston1/2 @1.4/1.5 · SensorVidrio @1.6 · PesoActualKg @2.0
  - **`VisionMaterial` Int @ 6.0** (0/1/2/3)
- [ ] Tag table según `TABLA_IO_1214C.md` (**5 DI** posición + **10 DQ**)
- [ ] LAD según `NETWORKS_LAD.md`: latch = **VisionMaterial == N AND I_Sensor…**
- [ ] Al terminar TON: `VisionMaterial := 0`
- [ ] `T_TimeoutVision` (ej. 8 s) si hay visión sin sensor
- [ ] **1L y 2L** al mismo **+24 V**
- [ ] Download software + CPU RUN
- [ ] Online: forzando Ext/Ret de cada pistón se oye/ve cada solenoide

## Mesa (3 sensores de posición + cámara)
- [ ] 3 ópticos en vías P1/P2/P3 (`I_SensorPlastico/Aluminio/Vidrio`)
- [ ] `I_SensorPieza` a la entrada (trigger foto)
- [ ] Cámara en **Raspberry Pi 4** apuntando zona estable
- [ ] Pi 4 en misma subnet que el PLC + internet (Gemini)
- [ ] 3× cilindro doble efecto + 3× 5/2 biestable (sin FC)
- [ ] P1 = plástico · P2 = latas · P3 = vidrio

## Raspberry Pi 4
- [ ] `pip install google-generativeai python-snap7 firebase-admin pillow`
- [ ] `GEMINI_API_KEY` en env / `.env` (nunca en la web)
- [ ] `serviceAccountKey.json` en la carpeta del bridge
- [ ] Bridge HMI: `py plc_real/plc_bridge_real.py --ip <IP_PLC>`
- [ ] Visión: `py costa_rica/vision_gemini.py --ip <IP_PLC>`

## Prueba integral
1. START AUTO → banda ON
2. Pieza entra → `I_SensorPieza` → Pi captura → Gemini → `VisionMaterial=1/2/3`
3. Banda **sigue** hasta el sensor de esa vía
4. Sensor vía + visión → banda OFF · pistón Ext · TON · Ret · Vision=0 · contador++
5. Emergencia → paro + limpia visión
