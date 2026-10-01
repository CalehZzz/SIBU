# Guía desde CERO — TIA Portal + PLC 1214C + que funcione todo

**Meta:** proyecto TIA nuevo → ladder completo → download al PLC → ciclo real + web.  
**Carpeta del repo:** `plc_real/` (PLC **físico**; no uses el proyecto de simulación 1511C).

Seguí esto **en orden**. No saltes pasos. Cada sección dice **qué** hacer y **por qué**.

---

## Mapa mental (por qué hay tantas piezas)

```
Vos (TIA) programas el CEREBRO del PLC.
El PLC manda banda, pistones, lámparas, contadores (salidas %Q)
         y lee sensores (entradas %I).

La web NO habla S7. Un bridge en la Pi escribe/lee DBs.
Gemini (visión) escribe solo VisionMaterial en DB_HMI.
El ESP32 manda niveles de botes (y RFID) por WiFi a la Pi.
```

| Pieza | Rol | ¿Se hace en TIA? |
|---|---|---|
| TIA + CPU 1214C | Lógica LAD | **Sí — es el núcleo** |
| DBs `DatosEstacion` + `DB_HMI` | Contrato con la web/Pi | **Sí** |
| Raspberry Pi + bridge | Web ↔ PLC | Después del ladder |
| Gemini | Material 1/2/3/4 | Después |
| ESP32 (botes + RFID) | Plus mesa | Independiente / paralelo |
| Automation Studio | Rúbrica sim (otro PC) | **No** en este documento |


---

# FASE 0 — Antes de abrir TIA

### 0.1 Materiales que debe tener a mano

- PC con **TIA Portal V20**
- Cable Ethernet PC ↔ switch/PLC (misma red)
- CPU **S7-1200 1214C** (anotá el MLFB de la etiqueta, ej. `6ES7 214-1BG40-0XB0`)
- 24 V DC para sensores/actuadores (según mesa)
- Repo SIBU clonado (para copiar tablas): carpeta `plc_real/`

### 0.2 Por qué un proyecto nuevo

El proyecto de **PLCSIM / 1511C** es otra CPU.  
**No se puede** “cambiar el device” y listo: I/O, firmware y hardware config son distintos.  
Por eso: **Create new project** solo para el 1214C.

### 0.3 IP que van a usar (acordar y anotar)

Ejemplo típico Costa Rica:

| Equipo | IP ejemplo |
|---|---|
| PLC 1214C | `192.168.0.10` |
| Raspberry Pi | `192.168.0.20` |
| PC con TIA | `192.168.0.100` |
| Máscara | `255.255.255.0` |

**Por qué fija:** el bridge y la visión necesitan saber siempre a qué IP escribir. DHCP cambia y “deja de funcionar” sin aviso.

---

# FASE 1 — Crear el proyecto y la CPU

### 1.1 Crear proyecto

1. Abrí TIA Portal V20.  
2. **Create new project**.  
3. Nombre sugerido: `SIBU_PLC_Real_1214C`.  
4. Create.

**Por qué:** aislás el PLC real del demo de simulación.

### 1.2 Agregar la CPU exacta

1. **Add new device**.  
2. Controllers → **SIMATIC S7-1200**.  
3. Elegí **CPU 1214C AC/DC/Rly** (o DC/DC/DC si es la tuya — **debe coincidir con la etiqueta física**).  
4. Firmware: el más cercano al de tu CPU (Online → Accessible devices lo muestra).  
5. Add.

**Por qué:** si el MLFB/firmware no coinciden, el download falla o el hardware “no cuadra”.

### 1.3 Configurar la IP del PLC

1. Doble clic en la CPU → **Device configuration**.  
2. PROFINET interface **[X1]**.  
3. IP: `192.168.0.10` (o la acordada) · subnet `255.255.255.0`.  
4. Desactivá obtener IP por DHCP.

**Por qué:** sin IP fija no hay bridge ni Online estable.

### 1.4 PUT/GET (obligatorio para la Pi / snap7)

1. En Device configuration, seleccioná la **CPU** (no solo el puerto).  
2. Properties → **Protection & Security** (a veces “Connection mechanisms”).  
3. Activá: **Permit access with PUT/GET communication from remote partner**.  
4. Access level: **Full access** (o el mínimo que permita lectura/escritura de DBs).

**Por qué:** la Raspberry escribe `DB_HMI` y lee `DatosEstacion` con protocolo S7 (snap7). Sin PUT/GET el bridge da error aunque el ladder esté perfecto.

### 1.5 Download solo de hardware (ahora)

1. Conectá Ethernet.  
2. Online → Accessible devices → encontrá la CPU.  
3. Download **hardware configuration** (o hardware + software vacío).  
4. Dejá CPU en RUN o STOP según pida el asistente; al final **RUN**.

**Por qué:** la IP y el PUT/GET viven en la config de hardware; hay que bajarlos antes de pelear con DBs desde fuera.

---

# FASE 2 — Tags (I/Q) — el cableado “en software”

### 2.1 Abrí la tag table

Project tree → PLC → **PLC tags** → Default tag table (o creá `SIBU_IO`).

**Por qué:** nombres claros (`Q_Banda`) evitan programar con `%Q0.0` a ciegas y documentan el cableado.

### 2.2 Entradas — creá exactamente estas

| Dirección | Nombre | Por qué existe |
|---|---|---|
| `%I0.0` | `I_SensorPieza` | Pieza en entrada → dispara visión (la banda **sigue**) |
| `%I0.1` | `I_SensorPlastico` | Pieza llegó a la vía del pistón plástico |
| `%I0.2` | `I_SensorAluminio` | Vía latas |
| `%I0.3` | `I_SensorVidrio` | Vía vidrio |
| `%I0.4` | `I_BasculaFinal` | Báscula demo al final (opcional; **no** frena el ciclo) |
| `%I0.5` | `I_BinFullPlastico` | Bote plástico lleno (ESP/Arduino o Merker) |
| `%I0.6` | `I_BinFullAluminio` | Bote latas lleno |
| `%I0.7` | `I_BinFullVidrio` | Bote vidrio lleno |
| `%I1.0` | `I_BinFullRechazo` | Bote rechazo lleno → **paro total** |

Si todavía no cableás los `I_BinFull*`, igual creá los tags: la lógica ya queda lista; podés forzarlos Online para probar.

### 2.3 Salidas — creá exactamente estas

| Dirección | Nombre | Por qué |
|---|---|---|
| `%Q0.0` | `Q_Banda` | Motor / contactor banda |
| `%Q0.1` | `Q_Piston1Ext` | Solenoide extiende P1 (plástico) |
| `%Q0.2` | `Q_Piston1Ret` | Solenoide retrae P1 |
| `%Q0.3` | `Q_Piston2Ext` | Extiende P2 (latas) |
| `%Q0.4` | `Q_Piston2Ret` | Retrae P2 |
| `%Q0.5` | `Q_Piston3Ext` | Extiende P3 (vidrio) |
| `%Q0.6` | `Q_Piston3Ret` | Retrae P3 |
| `%Q0.7` | `Q_LamparaRun` | Verde — sistema OK |
| `%Q1.0` | `Q_LamparaAlarma` | Roja — emergencia / rechazo lleno |
| `%Q1.1` | `Q_LamparaAmarilla` | Amarilla — algún bote material lleno (desvío) |
| `%Q1.2` | `Q_ContPlastico` | Pulso contador **físico** plástico |
| `%Q1.3` | `Q_ContAluminio` | Pulso contador físico latas |
| `%Q1.4` | `Q_ContVidrio` | Pulso contador físico vidrio |

> Si la CPU solo tiene 10 DQ y no llegan los contadores: módulo expansión **SM1222** (está anotado en `TABLA_IO_1214C.md`).

**Cableado eléctrico (mesa):** comunes de relé/salidas según manual; **1L y 2L → +24 V** en la CPU relay. El 220 V del semáforo solo en contactos de relé, nunca directo al PLC.

### 2.4 Memorias `%M` (bits internos)

Creá al menos:

| Tag | Uso |
|---|---|
| `M_SistemaOn` | Sistema armado (Start retenido) |
| `M_ModoAuto` | Auto vs manual |
| `M_Alarma` | Alarma latcheada |
| `M_StopBotes` | Rechazo lleno |
| `M_EsperandoVision` | Esperando que Gemini escriba material |
| `M_ClasifPlastico` / `M_ClasifAluminio` / `M_ClasifVidrio` | Latch de clasificación |
| `M_Clasificando` | OR de los tres (para la banda) |
| `M_Piston1` / `M_Piston2` / `M_Piston3` | Comando “quiero este pistón” |
| `M_DivertPlastico` / `M_DivertAluminio` / `M_DivertVidrio` | Bote lleno → desviar a rechazo |
| `M_PassThrough` | Dejar pasar sin pistón |

**Por qué Merker:** la lógica intermedia no debe vivir solo en bobinas de salida; facilita Auto/Manual y espejo a la web.

### 2.5 Timers (IEC TON)

Insertá bloques TON (Add new block → o desde la librería al dibujar) con instancia:

| Instancia | PT sugerido | Para qué |
|---|---|---|
| `T_EmpujePiston1` | `T#1s` | Tiempo que P1 queda extendido |
| `T_EmpujePiston2` | `T#1s` | Idem P2 |
| `T_EmpujePiston3` | `T#1s` | Idem P3 |
| `T_PulsoCont1` | `T#200ms` | Ancho del pulso al contador físico P |
| `T_PulsoCont2` | `T#200ms` | Contador A |
| `T_PulsoCont3` | `T#200ms` | Contador V |
| `T_TimeoutVision` | `T#8s` | Entrada sin visión → tratar como desconocido (4) |
| `T_TimeoutVia` | `T#10s` | Visión lista pero no llegó al sensor de vía |

**Por qué TON y no FC de pistón:** en mesa real los cilindros van por tiempo (doble efecto biestable), no por finales de carrera 0%/100%.

---

# FASE 3 — Data Blocks (contrato con la web) — CRÍTICO

Si esto está mal, el ladder puede “andar” en mesa y la web/Pi **no**.

### 3.1 Crear `DatosEstacion` = DB **1**

1. Program blocks → Add new block → **Data block**.  
2. Name: `DatosEstacion`.  
3. Number: **1** (Manual).  
4. Type: Global DB.  
5. **Properties → Attributes → Optimized block access = OFF (desmarcado).**  

**Por qué Optimized OFF:** snap7 lee offsets fijos (byte 0, 2, 6…). Con Optimized ON esos offsets no existen como espera el bridge → `Invalid address`.

### 3.2 Campos de `DatosEstacion` (en este orden / offsets)

Copiá de `plc_real/DB_CONTRATO_WEB.md`. Resumen:

| Offset | Nombre | Tipo |
|---|---:|---|
| 0.0 | `ContPlastico` | Int |
| 2.0 | `ContAluminio` | Int |
| 4.0 | `PesoPlasticoKg` | Real |
| 8.0 | `PesoAluminioKg` | Real |
| 12.0 | `PesoActualKg` | Real |
| 16.0 | `SesionActiva` | Bool |
| 16.1 | `SistemaOn` | Bool |
| 16.2 | `ModoAuto` | Bool |
| 16.3 | `Emergencia` | Bool |
| 16.4 | `Alarma` | Bool |
| 16.5 | `BandaOn` | Bool |
| 16.6 | `PistonOn` | Bool |
| 16.7 | *(libre o el que diga el mapa)* | Bool |
| 17.0 | `Piston1On` | Bool |
| 17.1 | `Piston2On` | Bool |
| 17.2 | `Piston3On` | Bool |
| 18.0 | `EstadoMaquina` | Int |
| 20.0 | `UltimoMaterial` | Int |
| 22.0 | `ContVidrio` | Int |
| 24.0 | `PesoVidrioKg` | Real |

Tamaño final **≥ 28 bytes**. Mira la columna Offset en la vista del DB: deben verse números, no “symbolic only”.

### 3.3 Crear `DB_HMI` = DB **3**

1. Add new → Data block → Name `DB_HMI` · Number **3**.  
2. **Optimized block access = OFF**.  
3. Campos (mínimo **8 bytes**):

| Offset | Nombre | Tipo | Quién lo escribe |
|---|---|---|---|
| 0.0 | `Start` | Bool | Web vía bridge |
| 0.1 | `Stop` | Bool | Web |
| 0.2 | `Emergencia` | Bool | Web |
| 0.3 | `ResetAlarma` | Bool | Web |
| 0.4 | `ModoAuto` | Bool | Web |
| 0.5 | `FinSesion` | Bool | Web |
| 0.6 | `ManualBanda` | Bool | Web |
| 0.7 | `ManualPiston` | Bool | Web · manual P3 vidrio |
| 1.0 | `BasculaLista` | Bool | Espejo/demo |
| 1.1 | `SensorPieza` | Bool | Espejo/sim (en real usás `%I`) |
| 1.2 | `SensorPlastico` | Bool | Espejo/sim |
| 1.3 | `SensorAluminio` | Bool | Espejo/sim |
| 1.4 | `ManualPiston1` | Bool | Manual P1 |
| 1.5 | `ManualPiston2` | Bool | Manual P2 |
| 1.6 | `SensorVidrio` | Bool | Espejo/sim |
| 2.0 | `PesoActualKg` | **Real** | Bridge / demo |
| **6.0** | **`VisionMaterial`** | **Int** | **Solo la Pi (Gemini)** · 0/1/2/3/4 |

**No crees** `PistonNExtendido` / `PistonNRetractado` (diseño viejo con FC).

**Por qué `VisionMaterial` en offset 6:** el bridge HMI escribe bytes 0–5 y **no pisa** el Int de visión. Si lo ponés más arriba, se pisan comandos y visión.

### 3.4 Compile

Right-click PLC → Compile → Hardware and software.  
Sin errores rojos antes de seguir.

---

# FASE 4 — Bloques de programa (estructura)

### 4.1 Crear los FC

Add new block → Function (FC), lenguaje **LAD** (salvo el espejo):

| Bloque | Lenguaje | Por qué |
|---|---|---|
| `FC_Modos` | LAD | Start/Stop/Emergencia/lámparas |
| `FC_Secuencia` | LAD | Banda, visión, pistones, contadores |
| `FC_Alarmas` | LAD | Timeouts y rechazo lleno |
| `FC_EspejoWeb` | **SCL** | Copiar estado a `DatosEstacion` (más claro en texto) |

### 4.2 OB1 — solo llamadas (orden fijo)

En `Main [OB1]`, networks:

1. Call `FC_Modos`  
2. Call `FC_Secuencia`  
3. Call `FC_Alarmas`  
4. Call `FC_EspejoWeb`

**Por qué este orden:** primero modos (quién puede correr), luego secuencia, luego alarmas que pueden cortar, al final espejo para la web.

---

# FASE 5 — Programar `FC_Modos` (LAD)

Idea: la web pone pulsos/bits en `DB_HMI`; el PLC **retiene** On/Auto en Merker.

### NW — Arranque (Set)

```
DB_HMI.Start  —|/| DB_HMI.Stop —|/| DB_HMI.Emergencia  ——— (S) M_SistemaOn
```

### NW — Paro / emergencia (Reset)

```
DB_HMI.Stop        ———┐
DB_HMI.Emergencia  ———┴—— (R) M_SistemaOn
```

### NW — Modo auto

```
DB_HMI.ModoAuto ——— ( ) M_ModoAuto
```

(Bobina normal: copia el bit cada ciclo.)

### NW — Lámpara roja

```
DB_HMI.Emergencia  OR  M_Alarma  OR  I_BinFullRechazo  ——— ( ) Q_LamparaAlarma
```

### NW — Lámpara amarilla

```
|/| I_BinFullRechazo
—| I_BinFullPlastico OR I_BinFullAluminio OR I_BinFullVidrio ——— ( ) Q_LamparaAmarilla
```

### NW — Lámpara verde

```
M_SistemaOn —|/| Q_LamparaAlarma —|/| Q_LamparaAmarilla ——— ( ) Q_LamparaRun
```

### NW — Stop botes

```
I_BinFullRechazo ——— ( ) M_StopBotes
```

**Por qué:** el operador no tiene botonera física; Start/Stop vienen de la HMI web. Las lámparas cuentan la historia a quien mira la mesa.

---

# FASE 6 — Programar `FC_Secuencia` (el corazón)

Referencia detallada: `plc_real/NETWORKS_LAD.md`.

### 6.1 Qué debe pasar en la mesa (entendé esto antes de dibujar)

1. Start Auto → banda ON.  
2. Pieza activa `I_SensorPieza` → plc pone `M_EsperandoVision` (Gemini trabaja; **banda sigue**).  
3. Pi escribe `DB_HMI.VisionMaterial` = 1, 2, 3 o 4.  
4. Cuando la pieza llega al sensor de **esa** vía **y** el bote no está en desvío → latch Clasif → banda OFF → pistón Ext → TON → Ret → pulso contador → `VisionMaterial := 0`.  
5. Si Vision=4 o bote material lleno → pass-through a rechazo (sin contador de ese material).  
6. Si rechazo lleno → todo parado (ya en Modos/Alarmas).

### 6.2 NW — Entrada pide visión

Condiciones en serie hacia un flanco o Set:

- `M_SistemaOn`, `M_ModoAuto`, no Emergencia, no `M_Alarma`, no `M_StopBotes`
- `I_SensorPieza`
- no `M_EsperandoVision`, no `M_Clasificando`
- `DB_HMI.VisionMaterial == 0`  
→ **(S) `M_EsperandoVision`**

**Por qué `VisionMaterial == 0`:** solo pedís una foto/clasificación nueva si no hay material pendiente.

### 6.3 NW — Banda

**AUTO:**  
`M_SistemaOn` · `M_ModoAuto` · /Emerg · /Alarma · /`M_StopBotes` · /`M_Clasificando` → `Q_Banda`

**MANUAL (paralelo):**  
`M_SistemaOn` · /`M_ModoAuto` · `DB_HMI.ManualBanda` · /`M_StopBotes` → `Q_Banda`

**Por qué para al clasificar:** el pistón empuja con la banda quieta.

### 6.4 NW — Visión llegó

```
M_EsperandoVision · (VisionMaterial <> 0) ——— (R) M_EsperandoVision
```

### 6.5 NW — Flags de desvío (SCL en network o Assign)

```scl
M_DivertPlastico := I_BinFullPlastico AND NOT I_BinFullRechazo;
M_DivertAluminio := I_BinFullAluminio AND NOT I_BinFullRechazo;
M_DivertVidrio   := I_BinFullVidrio   AND NOT I_BinFullRechazo;
M_PassThrough    := (DB_HMI.VisionMaterial = 4)
                 OR (DB_HMI.VisionMaterial = 1 AND M_DivertPlastico)
                 OR (DB_HMI.VisionMaterial = 2 AND M_DivertAluminio)
                 OR (DB_HMI.VisionMaterial = 3 AND M_DivertVidrio);
```

### 6.6 NW — Latch plástico / latas / vidrio

Ejemplo plástico (Set):

- On · Auto · /Emerg · /Alarma · /StopBotes · /DivertPlastico  
- `VisionMaterial == 1` · `I_SensorPlastico`  
- /ClasifAluminio · /ClasifVidrio  
→ **(S) `M_ClasifPlastico`**

Igual para Aluminio (Vision==2 + `I_SensorAluminio`) y Vidrio (Vision==3 + `I_SensorVidrio`).

### 6.7 NW — `M_Clasificando`

```scl
M_Clasificando := M_ClasifPlastico OR M_ClasifAluminio OR M_ClasifVidrio;
```

### 6.8 NW — Pass-through limpia visión

Cuando `M_PassThrough` y la pieza “sale” (podés usar el sensor de vía que corresponda o un TON de paso):  
**MOVE 0 → `DB_HMI.VisionMaterial`**.

### 6.9 NW — Comando pistón `M_PistonN`

Para cada N=1,2,3 bobina `M_PistonN`:

- rama Auto: `M_Clasif…` correspondiente  
- rama Manual: On · /Auto · `ManualPiston1/2` o `ManualPiston` (P3)

### 6.10 NW — Solenoides Ext / Ret (¡una bobina por salida!)

Para P1 (igual P2/P3):

```
M_SistemaOn · M_Piston1 · / (Ret activo)  ——— ( ) Q_Piston1Ext
M_SistemaOn · /M_Piston1 · / (Ext activo) ——— ( ) Q_Piston1Ret
```

Interlock: no Ext y Ret a la vez.  
**Por qué:** 5/2 biestable; Ext energiza un solenoide, Ret el otro.

### 6.11 NW — TON empuje + contador físico + limpiar

Para P1:

1. `M_ClasifPlastico` alimenta `T_EmpujePiston1` (PT 1 s).  
2. Mientras Clasif y no Q del TON → `M_Piston1` (o ya lo tenés del NW anterior).  
3. Cuando `T_EmpujePiston1.Q`:  
   - **(R) `M_ClasifPlastico`**  
   - MOVE 1 → `DatosEstacion.UltimoMaterial` (opcional)  
   - arrancá pulso `T_PulsoCont1` → mientras timer · `Q_ContPlastico`  
   - INC `DatosEstacion.ContPlastico` (espejo web; el contador de mesa es el físico)  
   - **MOVE 0 → `DB_HMI.VisionMaterial`**

Repetí para P2 y P3.

**Por qué Vision:=0 al final:** libera el camino para la siguiente pieza.

---

# FASE 7 — `FC_Alarmas`

### Timeout visión

Si `M_EsperandoVision` más de `T_TimeoutVision` → MOVE **4** a `VisionMaterial` (desconocido) o Set alarma (elegí una política y documentala; la del repo: pasar a 4).

### Timeout vía

Si Vision ≠ 0, no pass-through, no clasificando, y no llega sensor en `T_TimeoutVia` → Set `M_Alarma`.

### Rechazo lleno

```
I_BinFullRechazo ——— (S) M_Alarma
                 ——— (R) M_EsperandoVision
                 ——— (R) todos M_Clasif*
                 ——— MOVE 0 → VisionMaterial
```

### Reset alarma

```
DB_HMI.ResetAlarma · /I_BinFullRechazo ——— (R) M_Alarma
```

---

# FASE 8 — `FC_EspejoWeb` (SCL)

Abrí el FC en SCL y copiá algo así (ajustá nombres si tu DB usa otros):

```scl
DatosEstacion.SistemaOn     := M_SistemaOn;
DatosEstacion.ModoAuto      := M_ModoAuto;
DatosEstacion.Emergencia    := DB_HMI.Emergencia;
DatosEstacion.Alarma        := M_Alarma;
DatosEstacion.BandaOn       := Q_Banda;
DatosEstacion.Piston1On     := M_Piston1;
DatosEstacion.Piston2On     := M_Piston2;
DatosEstacion.Piston3On     := M_Piston3;
DatosEstacion.PistonOn      := M_Piston1 OR M_Piston2 OR M_Piston3;
DatosEstacion.PesoActualKg  := DB_HMI.PesoActualKg;
```

**Por qué:** la web lee un solo DB de estado; no lee I/Q crudos.

---

# FASE 9 — Compilar, bajar y poner en RUN

### 9.1 Compile completo

Compile → Hardware and software (rebuild all si hace falta).  
Cero errores.

### 9.2 Download

1. CPU accesible en la red.  
2. Download **hardware + software**.  
3. Si pregunta STOP: aceptá, después **RUN**.  
4. Online → Monitoring on (gafas) en OB1 / FC_Secuencia.

### 9.3 Prueba sin Pi (forzado Online)

Con Monitoring:

1. Forzá `DB_HMI.Start` = 1 un instante → `M_SistemaOn` queda.  
2. `DB_HMI.ModoAuto` = 1 → debe intentar `Q_Banda` (si no hay alarma/stop).  
3. Forzá `DB_HMI.VisionMaterial` = 1 y `I_SensorPlastico` = 1 → debe clasificar P1 (Ext→TON→Ret→pulso contador).  
4. Forzá `I_BinFullRechazo` = 1 → roja y banda off.

**Por qué:** si esto no anda, no culpes a la web ni a Gemini.

### 9.4 Prueba solenoides uno a uno

Online Force (con cuidado, aire/presión OK):

- `Q_Piston1Ext` / Ret, luego P2, P3.  
- Escuchá válvulas / mirá cilindros.

---

# FASE 10 — Conectar Pi + web (para que “funcione 100%”)

Solo cuando el ladder Online ya clasifica forzando tags.

### 10.1 En la Raspberry

```bash
cd ~/SIBU
git pull
# serviceAccountKey.json en ~/SIBU/
cp -n costa_rica/sibu.env.example costa_rica/sibu.env
nano costa_rica/sibu.env   # PLC_IP=192.168.0.10  GEMINI_API_KEY=...  RFID_SERIAL=
```

```bash
bash costa_rica/install_services.sh all
# o al menos:
#   bridge + visión + rfid + bins
```

Bridge manual de prueba:

```bash
python plc_real/plc_bridge_real.py --ip 192.168.0.10
```

**Por qué:** sin bridge, la HMI web no mueve `DB_HMI.Start`.

### 10.2 Web

1. Login Google en la app SIBU.  
2. Estación **`colegio-don-bosco-real`**.  
3. Abrir HMI → Start · Auto.  
4. Ciclo con pieza real + cámara.

### 10.3 ESP32 (botes + RFID)

Ya flasheado según `costa_rica/arduino_bins/`.  
Misma WiFi que la Pi · `PI_HOST` = IP de la Pi.

---

# FASE 11 — Checklist “¿está 100%?”

Marcá en orden:

**TIA / PLC**

- [ ] Proyecto 1214C nuevo (no 1511C)  
- [ ] IP fija + PUT/GET + download hardware  
- [ ] Tags I/Q según tabla  
- [ ] DB1 y DB3 Optimized **OFF**, tamaños OK  
- [ ] OB1 llama los 4 FC  
- [ ] Online: Start → banda; Vision+sensor → pistón; rechazo lleno → rojo  

**Mesa**

- [ ] Sensores cableados a las I correctas  
- [ ] 6 solenoides a las Q correctas  
- [ ] Contadores físicos pulsan  
- [ ] 1L/2L a +24 V  

**Pi / plus**

- [ ] Bridge conectado sin `Invalid address`  
- [ ] Gemini escribe `VisionMaterial`  
- [ ] Web Start mueve el PLC  
- [ ] ESP bins/RFID OK  

Si el bridge dice DB pequeño: leé `plc_real/FIX_DB_INVALID_ADDRESS.md` (casi siempre Optimized ON o falta el Int @ 6.0).

---

# Errores típicos (y por qué)

| Síntoma | Causa probable |
|---|---|
| Bridge `Invalid address` | DB Optimized ON o DB chico |
| Web Start no hace nada | Bridge apagado / IP mala / PUT/GET off |
| Banda nunca para | No se setea `M_Clasificando` |
| Pistón no sale | Vision≠N o sensor vía no llega o Divert activo |
| Contador web sube pero mesa no | Falta pulso `Q_Cont*` o cable contador |
| Siempre rojo | `I_BinFullRechazo` a 1 o flotando mal |
| Visión no clasifica | Pi/Gemini; forzá Vision Online para separar fallos |

---

# Dónde está cada cosa en el repo

| Archivo | Contenido |
|---|---|
| `plc_real/00_PROYECTO_TIA.md` | Crear proyecto |
| `plc_real/TABLA_IO_1214C.md` | I/Q |
| `plc_real/DB_CONTRATO_WEB.md` | Offsets DB |
| `plc_real/NETWORKS_LAD.md` | Lógica networks |
| `plc_real/CHECKLIST.md` | Lista corta |
| `plc_real/FIX_DB_INVALID_ADDRESS.md` | Arreglar bridge |
| `docs/13_CONECTAR_TODO.md` | AS sim vs mesa real |
| `docs/12_RUBRICA_CAPAS.md` | Qué es rúbrica vs plus |

---

**Regla de oro:**  
Primero el PLC debe clasificar **solo con TIA Online** (forzando tags). Después Pi y web. Si invertís el orden, vas a depurar tres sistemas a la vez.
