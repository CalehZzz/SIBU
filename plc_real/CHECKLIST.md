# Checklist — PLC real 1214C + visión Gemini + web (Pi 4)

## Hardware / TIA
- [ ] Proyecto **nuevo** con CPU 1214C (no el de 1511C)
- [ ] IP estática anotada (ej. `192.168.0.10`)
- [ ] PUT/GET habilitado + download hardware
- [ ] `DatosEstacion` DB1 Optimized OFF · **≥ 28 bytes**
- [ ] `DB_HMI` DB3 Optimized OFF · **≥ 8 bytes**
  - ManualPiston1/2 @1.4/1.5 · SensorVidrio @1.6 · PesoActualKg @2.0
  - **`VisionMaterial` Int @ 6.0** (0/1/2/3/4)
- [ ] Tag table según `TABLA_IO_1214C.md` (**5 DI** + **10 DQ**)
  - `I_SensorPieza` = entrada (foto); **no** paro por peso
  - `I_BasculaFinal` (`I0.4`) = báscula al **final** (demo); **no** gatea
- [ ] **4× HC-SR04** (Arduino) → botes P/A/V/Rechazo · ver `costa_rica/arduino_bins/`
- [ ] DI `I_BinFull*` cableadas (o Merker desde `bins_pi`)
- [ ] Lámpara amarilla = material lleno (desvío) · roja = rechazo lleno (stop)
- [ ] **3 contadores FÍSICOS** cableados a `Q_ContPlastico/Aluminio/Vidrio`
- [ ] LAD según `NETWORKS_LAD.md`:
  - banda **sigue** tras entrada; solo para en clasif/pistón
  - latch = **VisionMaterial == N AND I_Sensor…**
  - Al terminar TON: **pulso contador físico** + `VisionMaterial := 0`
- [ ] `T_TimeoutVision` (ej. 8 s) si hay entrada sin visión → Vision=4
- [ ] **1L y 2L** al mismo **+24 V**
- [ ] Download software + CPU RUN
- [ ] Online: forzando Ext/Ret de cada pistón se oye/ve cada solenoide

## Mesa (3 sensores de posición + cámara · báscula solo demo)
- [ ] 3 ópticos/sensores en vías P1/P2/P3
- [ ] `I_SensorPieza` a la **entrada** (trigger foto; banda no para por peso)
- [ ] Báscula (si la usás) al **final** donde caen desconocidos — representación
- [ ] Cámara en **Raspberry Pi 4**
- [ ] Pi 4 en misma subnet que el PLC + internet (Gemini)
- [ ] 3× cilindro + 3× 5/2 biestable
- [ ] P1 = plástico · P2 = latas · P3 = vidrio

## Raspberry Pi 4
- [ ] `pip install google-generativeai python-snap7 firebase-admin pillow`
- [ ] `GEMINI_API_KEY` en env / `.env` (nunca en la web)
- [ ] `serviceAccountKey.json` en la carpeta del bridge
- [ ] Bridge HMI: `py plc_real/plc_bridge_real.py --ip <IP_PLC>`
- [ ] Visión: `py costa_rica/vision_gemini.py --ip <IP_PLC>`

## Prueba integral
1. START AUTO → banda ON
2. Pieza entra → `I_SensorPieza` → Pi captura → Gemini → `VisionMaterial=1/2/3/4` (**banda sigue**)
3. Sensor vía + visión → banda OFF · pistón Ext · TON · Ret · **pulso contador FÍSICO** · Vision=0
4. Desconocido (4) → pass-through · opcional báscula al final (sin importancia) · **sin** pulso de contador de material
5. Emergencia → paro + limpia visión
6. **No** hay paso “esperar báscula lista” antes de clasificar
7. Verificar que el **contador de mesa** incrementó (no solo el número en la web)